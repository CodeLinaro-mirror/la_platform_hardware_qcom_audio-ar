/*
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Changes from Qualcomm Technologies, Inc. are provided under the following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define ATRACE_TAG (ATRACE_TAG_AUDIO | ATRACE_TAG_HAL)

#define LOG_TAG "AHAL_Stream_QTI"

#include <android-base/logging.h>
#include <android/binder_ibinder_platform.h>
#include <pthread.h>
#include <qti-audio-core/Module.h>
#include <qti-audio-core/Stream.h>
#include <qti-audio-core/Utils.h>
#include <utils/SystemClock.h>
#include <utils/Trace.h>

// uncomment this to enable logging of very verbose logs like burst commands.
// #define VERY_VERBOSE_LOGGING 1
using aidl::android::hardware::audio::common::AudioOffloadMetadata;
using aidl::android::hardware::audio::common::SinkMetadata;
using aidl::android::hardware::audio::common::SourceMetadata;
using aidl::android::media::audio::common::AudioDevice;
using aidl::android::media::audio::common::AudioDualMonoMode;
using aidl::android::media::audio::common::AudioInputFlags;
using aidl::android::media::audio::common::AudioIoFlags;
using aidl::android::media::audio::common::AudioLatencyMode;
using aidl::android::media::audio::common::AudioOffloadInfo;
using aidl::android::media::audio::common::AudioOutputFlags;
using aidl::android::media::audio::common::AudioPlaybackRate;
using aidl::android::media::audio::common::MicrophoneDynamicInfo;
using aidl::android::media::audio::common::MicrophoneInfo;

using ::aidl::android::hardware::audio::core::IStreamCallback;
using ::aidl::android::hardware::audio::core::IStreamCommon;
using aidl::android::hardware::audio::core::MmapBufferDescriptor;
using ::aidl::android::hardware::audio::core::StreamDescriptor;
using ::aidl::android::hardware::audio::core::VendorParameter;

namespace qti::audio::core {

void StreamContext::fillDescriptor(StreamDescriptor* desc) {
    if (mCommandMQ) {
        desc->command = mCommandMQ->dupeDesc();
    }
    if (mReplyMQ) {
        desc->reply = mReplyMQ->dupeDesc();
    }
    if (mDataMQ) {
        desc->frameSizeBytes = getFrameSize();
        desc->bufferSizeFrames = getBufferSizeInFrames();
        desc->audio.set<StreamDescriptor::AudioBuffer::Tag::fmq>(mDataMQ->dupeDesc());
    }
}

size_t StreamContext::getBufferSizeInFrames() const {
    if (mDataMQ) {
        return mDataMQ->getQuantumCount() * mDataMQ->getQuantumSize() / getFrameSize();
    }
    return 0;
}

size_t StreamContext::getFrameSize() const {
    return getFrameSizeInBytes(mFormat, mChannelLayout);
}

bool StreamContext::isValid() const {
    if (mCommandMQ && !mCommandMQ->isValid()) {
        HAL_LOGE << "command FMQ is invalid";
        return false;
    }
    if (mReplyMQ && !mReplyMQ->isValid()) {
        HAL_LOGE << "reply FMQ is invalid";
        return false;
    }
    if (getFrameSize() == 0) {
        HAL_LOGE << "frame size is invalid";
        return false;
    }
    if (mDataMQ && !mDataMQ->isValid()) {
        HAL_LOGE << "data FMQ is invalid";
        return false;
    }
    return true;
}

void StreamContext::reset() {
    mCommandMQ.reset();
    mReplyMQ.reset();
    mDataMQ.reset();
}

pid_t StreamWorkerCommonLogic::getTid() const {
#if defined(__ANDROID__)
    return pthread_gettid_np(pthread_self());
#else
    return 0;
#endif
}

std::string StreamWorkerCommonLogic::init() {
    if (mContext->getCommandMQ() == nullptr) return "Command MQ is null";
    if (mContext->getReplyMQ() == nullptr) return "Reply MQ is null";
    StreamContext::DataMQ* const dataMQ = mContext->getDataMQ();
    if (dataMQ == nullptr) return "Data MQ is null";
    if (sizeof(DataBufferElement) != dataMQ->getQuantumSize()) {
        return "Unexpected Data MQ quantum size: " + std::to_string(dataMQ->getQuantumSize());
    }
    mDataBufferSize = dataMQ->getQuantumCount() * dataMQ->getQuantumSize();
    mDataBuffer.reset(new (std::nothrow) DataBufferElement[mDataBufferSize]);
    if (mDataBuffer == nullptr) {
        return "Failed to allocate data buffer for element count " +
               std::to_string(dataMQ->getQuantumCount()) +
               ", size in bytes: " + std::to_string(mDataBufferSize);
    }
    if (::android::status_t status = mDriver->init(); status != STATUS_OK) {
        return "Failed to initialize the driver: " + std::to_string(status);
    }
    constexpr int kThreadNameMaxLen = 15;
    const auto& threadName = mContext->getStreamName().substr(0, kThreadNameMaxLen);
    if (int errCode = pthread_setname_np(pthread_self(), threadName.c_str()); errCode != 0) {
        HAL_LOGW << "Failed to set name for stream worker thread: " << strerror(errCode) << " "
                 << threadName;
    }
    return "";
}

void StreamWorkerCommonLogic::populateReply(StreamDescriptor::Reply* reply,
                                            bool isConnected) const {
    if (reply->status != STATUS_DEAD_OBJECT) {
        reply->status = STATUS_OK;
    }

    static const StreamDescriptor::Position kUnknownPosition = {
            .frames = StreamDescriptor::Position::UNKNOWN,
            .timeNs = StreamDescriptor::Position::UNKNOWN};

    reply->latencyMs = mContext->getNominalLatencyMs();

    reply->observable.frames = mContext->getFrameCount();
    reply->observable.timeNs = ::android::uptimeNanos();
    if (auto status = mDriver->refinePosition(reply); status == ::android::OK) {
        return;
    } else {
        if (hasMMapFlagsEnabled(mContext->getFlags())) {
            // if mmap position fails,return error to framework
            // for any error other than.. not enough data, AAudio will stop
            reply->status = STATUS_INVALID_OPERATION;
        }
    }

    reply->observable = reply->hardware = kUnknownPosition;
}

void StreamWorkerCommonLogic::populateReplyWrongState(
        StreamDescriptor::Reply* reply, const StreamDescriptor::Command& command) const {
    HAL_LOGW << "command '" << toString(command.getTag()) << "' can not be handled in the state "
             << toString(mState);
    reply->status = STATUS_INVALID_OPERATION;
}

void StreamWorkerCommonLogic::populateReplyUnsupportedCommand(
        StreamDescriptor::Reply* reply, const StreamDescriptor::Command& command) const {
    LOG(WARNING) << "command '" << toString(command.getTag()) << "' is not supported by the stream";
    reply->status = STATUS_INVALID_OPERATION;
}

StreamInWorkerLogic::Status StreamInWorkerLogic::cycle() {
    // Note: for input streams, draining is driven by the client, thus
    // "empty buffer" condition can only happen while handling the 'burst'
    // command. Thus, unlike for output streams, it does not make sense to
    // delay the 'DRAINING' state here by 'mTransientStateDelayMs'.
    // TODO: Add a delay for transitions of async operations when/if they added.

    StreamDescriptor::Command command{};
    if (!mContext->getCommandMQ()->readBlocking(&command, 1)) {
        HAL_LOGE << "reading of command from MQ failed";
        mState = StreamDescriptor::State::ERROR;
        return Status::ABORT;
    }

    HAL_LOGV << "received " << command.toString() << " in state " << toString(mState);

    StreamDescriptor::Reply reply{};
    reply.status = STATUS_BAD_VALUE;

    using Tag = StreamDescriptor::Command::Tag;
    switch (command.getTag()) {
        case Tag::halReservedExit:
            if (const int32_t cookie = command.get<Tag::halReservedExit>();
                cookie == mContext->getInternalCommandCookie()) {
                mDriver->shutdown();
                setClosed();
                // This is an internal command, no need to reply.
                return Status::EXIT;
            } else {
                HAL_LOGW << "EXIT command has a bad cookie: " << cookie;
            }
            break;
        case Tag::getStatus:
            populateReply(&reply, mIsConnected);
            break;
        case Tag::start:
            if (mState == StreamDescriptor::State::STANDBY ||
                mState == StreamDescriptor::State::DRAINING) {
                if (::android::status_t status = mDriver->start(); status == ::android::OK) {
                    populateReply(&reply, mIsConnected);
                    mState = mState == StreamDescriptor::State::STANDBY
                                     ? StreamDescriptor::State::IDLE
                                     : StreamDescriptor::State::ACTIVE;
                } else {
                    HAL_LOGE << "start failed: " << status;
                    // uncomment below, to treat the failure as HARD error, stream not recoverable
                    // mState = StreamDescriptor::State::ERROR;
                }
            } else {
                populateReplyWrongState(&reply, command);
            }
            break;
        case Tag::burst:
            if (const int32_t fmqByteCount = command.get<Tag::burst>(); fmqByteCount >= 0) {
#ifdef VERY_VERBOSE_LOGGING
                HAL_LOGV << "'" << toString(command.getTag()) << "' command for " << fmqByteCount
                         << " bytes";
#endif
                if (mState == StreamDescriptor::State::IDLE ||
                    mState == StreamDescriptor::State::ACTIVE ||
                    mState == StreamDescriptor::State::PAUSED ||
                    mState == StreamDescriptor::State::DRAINING) {
                    if (mContext->isMmap()) {
                        readMmap(&reply);
                    } else if (!read(fmqByteCount, &reply)) {
                        // uncomment below, to treat the failure as HARD error, stream not
                        // recoverable mState = StreamDescriptor::State::ERROR;
                    }
                    if (mState == StreamDescriptor::State::IDLE ||
                        mState == StreamDescriptor::State::PAUSED) {
                        mState = StreamDescriptor::State::ACTIVE;
                    } else if (mState == StreamDescriptor::State::DRAINING) {
                        // To simplify the reference code, we assume that the
                        // read operation has consumed all the data remaining in
                        // the hardware buffer. In a real implementation, here
                        // we would either remain in the 'DRAINING' state, or
                        // transfer to 'STANDBY' depending on the buffer state.
                        mState = StreamDescriptor::State::STANDBY;
                    }
                } else {
                    populateReplyWrongState(&reply, command);
                }
            } else {
                HAL_LOGW << "invalid burst byte count: " << fmqByteCount;
            }
            break;
        case Tag::drain:
            if (const auto mode = command.get<Tag::drain>();
                mode == StreamDescriptor::DrainMode::DRAIN_UNSPECIFIED) {
                if (mState == StreamDescriptor::State::ACTIVE) {
                    if (::android::status_t status = mDriver->drain(mode);
                        status == ::android::OK) {
                        populateReply(&reply, mIsConnected);
                        mState = StreamDescriptor::State::DRAINING;
                    } else {
                        HAL_LOGE << "drain failed: " << status;
                        // uncomment below, to treat the failure as HARD error, stream not
                        // recoverable mState = StreamDescriptor::State::ERROR;
                    }
                } else {
                    populateReplyWrongState(&reply, command);
                }
            } else {
                HAL_LOGW << "invalid drain mode: " << toString(mode);
            }
            break;
        case Tag::standby:
            if (mState == StreamDescriptor::State::IDLE) {
                if (::android::status_t status = mDriver->standby(); status == ::android::OK) {
                    populateReply(&reply, mIsConnected);
                    mState = StreamDescriptor::State::STANDBY;
                } else {
                    HAL_LOGE << "standby failed: " << status;
                    // uncomment below, to treat the failure as HARD error, stream not recoverable
                    // mState = StreamDescriptor::State::ERROR;
                }
            } else {
                populateReplyWrongState(&reply, command);
            }
            break;
        case Tag::pause:
            if (mState == StreamDescriptor::State::ACTIVE) {
                if (::android::status_t status = mDriver->pause(); status == ::android::OK) {
                    populateReply(&reply, mIsConnected);
                    mState = StreamDescriptor::State::PAUSED;
                } else {
                    HAL_LOGE << "pause failed: " << status;
                    // uncomment below, to treat the failure as HARD error, stream not recoverable
                    // mState = StreamDescriptor::State::ERROR;
                }
            } else {
                populateReplyWrongState(&reply, command);
            }
            break;
        case Tag::flush:
            if (mState == StreamDescriptor::State::PAUSED) {
                if (::android::status_t status = mDriver->flush(); status == ::android::OK) {
                    populateReply(&reply, mIsConnected);
                    mDriver->standby(); // move to standby
                    mState = StreamDescriptor::State::STANDBY;
                } else {
                    HAL_LOGE << "flush failed: " << status;
                    // uncomment below, to treat the failure as HARD error, stream not recoverable
                    // mState = StreamDescriptor::State::ERROR;
                }
            } else {
                populateReplyWrongState(&reply, command);
            }
            break;
        case Tag::flushFromFrame:
            populateReplyUnsupportedCommand(&reply, command);
            break;
    }
    reply.state = mState;

    using LogSeverity = ::android::base::LogSeverity;
    const LogSeverity severity =
            (reply.status != STATUS_OK) ? LogSeverity::ERROR : LogSeverity::VERBOSE;
    STREAM_LOG(severity) << "writing reply " << reply.toString();

    if (!mContext->getReplyMQ()->writeBlocking(&reply, 1)) {
        HAL_LOGE << "writing of reply " << reply.toString() << " to MQ failed";
        mState = StreamDescriptor::State::ERROR;
        return Status::ABORT;
    }
    return Status::CONTINUE;
}

bool StreamInWorkerLogic::read(size_t clientSize, StreamDescriptor::Reply* reply) {
    ATRACE_CALL();
    StreamContext::DataMQ* const dataMQ = mContext->getDataMQ();
    const size_t byteCount = std::min({clientSize, dataMQ->availableToWrite(), mDataBufferSize});
    const bool isConnected = mIsConnected;
    const size_t frameSize = mContext->getFrameSize();
    size_t actualFrameCount = 0;
    bool fatal = false;
    int32_t latency = mContext->getNominalLatencyMs();
    if (isConnected) {
        if (::android::status_t status = mDriver->transfer(mDataBuffer.get(), byteCount / frameSize,
                                                           &actualFrameCount, &latency);
            status != ::android::OK) {
            fatal = true;
            HAL_LOGE << "read failed: " << status;
        }
    } else {
        usleep(3000); // Simulate blocking transfer delay.
        for (size_t i = 0; i < byteCount; ++i) mDataBuffer[i] = 0;
        actualFrameCount = byteCount / frameSize;
    }
    const size_t actualByteCount = actualFrameCount * frameSize;
    if (bool success = actualByteCount > 0 ? dataMQ->write(&mDataBuffer[0], actualByteCount) : true;
        success) {
#ifdef VERY_VERBOSE_LOGGING
        HAL_LOGV << "writing of " << actualByteCount << " bytes into data MQ"
                 << " succeeded; connected? " << isConnected;
#endif
        // Frames are provided and counted regardless of connection status.
        reply->fmqByteCount += actualByteCount;
        mContext->advanceFrameCount(actualFrameCount);
        populateReply(reply, isConnected);
    } else {
        HAL_LOGW << "writing of " << actualByteCount << " bytes of data to MQ failed";
        reply->status = STATUS_NOT_ENOUGH_DATA;
    }
    reply->latencyMs = latency;
    return !fatal;
}

bool StreamInWorkerLogic::readMmap(StreamDescriptor::Reply* reply) {
    void* buffer = nullptr;
    size_t frameCount = 0;
    size_t actualFrameCount = 0;
    int32_t latency = mContext->getNominalLatencyMs();
    // use default-initialized parameter values for mmap stream.
    if (::android::status_t status =
                mDriver->transfer(buffer, frameCount, &actualFrameCount, &latency);
        status == ::android::OK) {
        populateReply(reply, mIsConnected);
        reply->latencyMs = latency;
        return true;
    } else {
        HAL_LOGE << "transfer failed: " << status;
        return false;
    }
}

bool StreamOutAsyncWorkerLogic::handleTransferReady() {
    if (mState == StreamDescriptor::State::TRANSFERRING) {
        mState = StreamDescriptor::State::ACTIVE;
        mContext->getAsyncCallback()->onTransferReady();
        HAL_LOGV << "sent transfer ready to client";
        return true;
    } else if (mState == StreamDescriptor::State::DRAINING && mDrainInternalState &&
               mDrainInternalState.value() == DrainInternalState::DRAINING_en_sent) {
        mContext->getAsyncCallback()->onTransferReady();
        HAL_LOGV << "sent transfer ready to client with drain internal state:"
                 << toString(mDrainInternalState.value());
        return true;
    }
    return false;
}

void StreamOutAsyncWorkerLogic::publishTransferReady() {
    if (!mContext->getAsyncCallback()) {
        return;
    }
    std::unique_lock lock{mAsyncMutex};
    mPendingCallBack = {};
    if (handleTransferReady()) {
        return;
    } else if (mState == StreamDescriptor::State::TRANSFER_PAUSED) {
        mPendingCallBack = StreamCallbackType::TR;
        HAL_LOGV << "pending transfer ready";
    } else {
        HAL_LOGW << "shouldn't happen !!";
    }
}

bool StreamOutAsyncWorkerLogic::handleDrainReady() {
    if (mState == StreamDescriptor::State::DRAINING) {
        if (mDrainInternalState) {
            if (mDrainInternalState.value() == DrainInternalState::DRAINING_en) {
                mDrainInternalState = DrainInternalState::DRAINING_en_sent;
            } else if (mDrainInternalState.value() == DrainInternalState::DRAINING_en_sent) {
                mDrainInternalState = {};
                if (mIsClipTransitionDataBurstsAvailable) {
                    mState = StreamDescriptor::State::TRANSFERRING;
                    mPendingCallBack = StreamCallbackType::TR;
                } else {
                    mState = StreamDescriptor::State::IDLE;
                }
                mIsClipTransitionDataBurstsAvailable = false;
            } else {
                HAL_LOGW << "shouldn't happen "
                         << ", drain internal state:" << toString(mDrainInternalState.value());
                return false;
            }
        } else {
            mState = StreamDescriptor::State::IDLE;
        }
        mContext->getAsyncCallback()->onDrainReady();
        HAL_LOGV << "sent drain ready to client";
        return true;
    }
    return false;
}

void StreamOutAsyncWorkerLogic::publishDrainReady() {
    if (!mContext->getAsyncCallback()) {
        return;
    }
    std::unique_lock lock{mAsyncMutex};
    mPendingCallBack = std::nullopt;
    if (handleDrainReady()) {
        return;
    } else if (mState == StreamDescriptor::State::DRAIN_PAUSED) {
        mPendingCallBack = StreamCallbackType::DR;
        HAL_LOGV << "pending drain ready";
    } else {
        HAL_LOGW << "shouldn't happen !!";
    }
}

bool StreamOutAsyncWorkerLogic::handleError() {
    if (!mContext->getAsyncCallback()) {
        return false;
    }
    mContext->getAsyncCallback()->onError();
    mState = StreamDescriptor::State::ERROR;
    HAL_LOGE << "sent Error to the client";
    return true;
}

void StreamOutAsyncWorkerLogic::publishError() {
    handleError();
}

StreamOutWorkerLogic::Status StreamOutAsyncWorkerLogic::cycle() {
    StreamDescriptor::Command command{};
    if (!mContext->getCommandMQ()->readBlocking(&command, 1)) {
        HAL_LOGE << "reading of command from MQ failed";
        mState = StreamDescriptor::State::ERROR;
        return Status::ABORT;
    }

    HAL_LOGV << "received " << command.toString() << " in state "
             << ::aidl::android::hardware::audio::core::toString(mState);

    StreamDescriptor::Reply reply{};
    reply.status = STATUS_BAD_VALUE;

    std::unique_lock asyncLock{mAsyncMutex};

    using Tag = StreamDescriptor::Command::Tag;
    switch (command.getTag()) {
        case Tag::halReservedExit: {
            if (const int32_t cookie = command.get<Tag::halReservedExit>();
                cookie == mContext->getInternalCommandCookie()) {
                // callback are suppressed with STANDBY state
                mState = StreamDescriptor::State::STANDBY;
                asyncLock.unlock(); // unlock as stream is going to destroy.
                mDriver->shutdown();
                setClosed();
                // This is an internal command, no need to reply.
                return Status::EXIT;
            } else {
                HAL_LOGW << "EXIT command has a bad cookie: " << cookie;
            }
        } break;
        case Tag::getStatus: {
            populateReply(&reply, mIsConnected);
        } break;
        case Tag::start: {
            std::optional<StreamDescriptor::State> nextState;
            std::optional<DrainInternalState> nextDrainInternalState;
            switch (mState) {
                case StreamDescriptor::State::STANDBY:
                    nextState = StreamDescriptor::State::IDLE;
                    break;
                case StreamDescriptor::State::PAUSED:
                    nextState = StreamDescriptor::State::ACTIVE;
                    break;
                case StreamDescriptor::State::DRAIN_PAUSED:
                    nextState = StreamDescriptor::State::DRAINING;
                    if (mDrainInternalState) {
                        if (mDrainInternalState.value() == DrainInternalState::DRAIN_PAUSED_en) {
                            nextDrainInternalState = DrainInternalState::DRAINING_en;
                        } else if (mDrainInternalState.value() ==
                                   DrainInternalState::DRAIN_PAUSED_en_sent) {
                            nextDrainInternalState = DrainInternalState::DRAINING_en_sent;
                        } else {
                            nextState = {};
                            HAL_LOGE << "bad drain internal state: "
                                     << toString(mDrainInternalState.value());
                        }
                    }
                    break;
                case StreamDescriptor::State::TRANSFER_PAUSED:
                    nextState = StreamDescriptor::State::TRANSFERRING;
                    break;
                default:
                    break;
            }
            if (nextState.has_value()) {
                if (::android::status_t status = mDriver->start(); status == ::android::OK) {
                    populateReply(&reply, mIsConnected);
                    if (*nextState == StreamDescriptor::State::IDLE ||
                        *nextState == StreamDescriptor::State::ACTIVE) {
                        mState = *nextState;
                    } else {
                        switchToTransientState(*nextState);
                    }
                    if (nextDrainInternalState) {
                        mDrainInternalState = nextDrainInternalState.value();
                    }
                } else {
                    HAL_LOGE << "start failed: " << status;
                    // uncomment below, to treat the failure as HARD error, stream not recoverable
                    // mState = StreamDescriptor::State::ERROR;
                }
            } else {
                populateReplyWrongState(&reply, command);
                break;
            }
        } break;
        case Tag::burst: {
            if (const int32_t fmqByteCount = command.get<Tag::burst>(); fmqByteCount >= 0) {
                HAL_LOGV << "burst with bytes:" << fmqByteCount;
                if (mState != StreamDescriptor::State::ERROR &&
                    mState != StreamDescriptor::State::TRANSFERRING &&
                    mState != StreamDescriptor::State::TRANSFER_PAUSED) {
                    if (!write(fmqByteCount, &reply)) {
                        HAL_LOGE << "write failed";
                        break;
                    }
                    if (mState == StreamDescriptor::State::STANDBY ||
                        mState == StreamDescriptor::State::PAUSED) {
                        mState = StreamDescriptor::State::PAUSED;
                    } else if (mState == StreamDescriptor::State::DRAIN_PAUSED) {
                        if (mDrainInternalState &&
                            mDrainInternalState.value() ==
                                    DrainInternalState::DRAIN_PAUSED_en_sent) {
                            mDrainInternalState = DrainInternalState::DRAIN_PAUSED_en_sent;
                            mState = StreamDescriptor::State::DRAIN_PAUSED;
                            mIsClipTransitionDataBurstsAvailable = true;
                        } else if (mDrainInternalState &&
                                   mDrainInternalState.value() ==
                                           DrainInternalState::DRAIN_PAUSED_en) {
                            mDrainInternalState = {};
                            mState = StreamDescriptor::State::TRANSFER_PAUSED;
                        } else if (!mDrainInternalState) {
                            mState = StreamDescriptor::State::TRANSFER_PAUSED;
                        } else {
                            HAL_LOGE << "bad drain internal state: "
                                     << toString(mDrainInternalState.value());
                            populateReplyWrongState(&reply, command);
                            break;
                        }
                    } else if (mState == StreamDescriptor::State::IDLE ||
                               mState == StreamDescriptor::State::ACTIVE) {
                        if (reply.fmqByteCount == fmqByteCount) {
                            mState = StreamDescriptor::State::ACTIVE;
                        } else {
                            // If write status is not ok, then dont put state in transferring
                            if (reply.status == STATUS_OK) {
                                switchToTransientState(StreamDescriptor::State::TRANSFERRING);
                            } else {
                                HAL_LOGE << "write failed, but dont put in error state ";
                                populateReplyWrongState(&reply, command);
                                break;
                            }
                        }
                    } else if (mState == StreamDescriptor::State::DRAINING) {
                        if (mDrainInternalState &&
                            mDrainInternalState.value() == DrainInternalState::DRAINING_en_sent) {
                            mDrainInternalState = DrainInternalState::DRAINING_en_sent;
                            mIsClipTransitionDataBurstsAvailable = true;
                            mState = StreamDescriptor::State::DRAINING;
                        } else {
                            if (reply.fmqByteCount == fmqByteCount) {
                                mState = StreamDescriptor::State::ACTIVE;
                                mDrainInternalState = {};
                            } else {
                                // If write status is not ok, then dont put state in transferring
                                if (reply.status == STATUS_OK) {
                                    switchToTransientState(StreamDescriptor::State::TRANSFERRING);
                                    mDrainInternalState = {};
                                } else {
                                    HAL_LOGE << "write failed, but dont put in error state ";
                                    populateReplyWrongState(&reply, command);
                                    break;
                                }
                            }
                        }
                    } else {
                        populateReplyWrongState(&reply, command);
                        break;
                    }
                } else {
                    populateReplyWrongState(&reply, command);
                    break;
                }
            } else {
                HAL_LOGW << "invalid burst bytes: " << fmqByteCount;
                populateReplyWrongState(&reply, command);
                break;
            }
        } break;
        case Tag::drain: {
            auto issueDrain = [&](auto drainMode) -> ::android::status_t {
                if (::android::status_t status = mDriver->drain(drainMode);
                    status == ::android::OK) {
                    populateReply(&reply, mIsConnected);
                    return status;
                } else {
                    HAL_LOGE << "issueDrain drain failed: " << status;
                    return status;
                }
            };
            if (auto currentMode = command.get<Tag::drain>();
                currentMode != StreamDescriptor::DrainMode::DRAIN_UNSPECIFIED) {
                if (currentMode == StreamDescriptor::DrainMode::DRAIN_EARLY_NOTIFY) {
                    if (mState == StreamDescriptor::State::ACTIVE ||
                        mState == StreamDescriptor::State::TRANSFERRING) {
                        if (auto status = issueDrain(currentMode); status != ::android::OK) {
                            break;
                        }
                        switchToTransientState(StreamDescriptor::State::DRAINING);
                        mDrainInternalState = DrainInternalState::DRAINING_en;
                    } else {
                        populateReplyWrongState(&reply, command);
                        break;
                    }
                } else {  // StreamDescriptor::DrainMode::DRAIN_ALL
                    if (mState == StreamDescriptor::State::ACTIVE ||
                        mState == StreamDescriptor::State::TRANSFERRING) {
                        if (auto status = issueDrain(currentMode); status != ::android::OK) {
                            break;
                        }
                        switchToTransientState(StreamDescriptor::State::DRAINING);
                        mDrainInternalState = {};
                    } else if (mState == StreamDescriptor::State::TRANSFER_PAUSED) {
                        if (auto status = issueDrain(currentMode); status != ::android::OK) {
                            break;
                        }
                        mState = StreamDescriptor::State::DRAIN_PAUSED;
                        mDrainInternalState = {};
                    } else {
                        populateReplyWrongState(&reply, command);
                        break;
                    }
                }
            } else {
                break;
            }
        } break;
        case Tag::standby: {
            if (mState == StreamDescriptor::State::IDLE) {
                asyncLock.unlock();
                if (auto status = mDriver->standby(); status == ::android::OK) {
                    mState = StreamDescriptor::State::STANDBY;
                    populateReply(&reply, mIsConnected);
                } else {
                    HAL_LOGE << "standby failed: " << status;
                    // uncomment below, to treat the failure as HARD error, stream not recoverable
                    // mState = StreamDescriptor::State::ERROR;
                }
                asyncLock.lock();
            } else {
                populateReplyWrongState(&reply, command);
            }
        } break;
        case Tag::pause: {
            std::optional<StreamDescriptor::State> nextState;
            std::optional<DrainInternalState> nextInternalState;
            switch (mState) {
                case StreamDescriptor::State::ACTIVE:
                    nextState = StreamDescriptor::State::PAUSED;
                    break;
                case StreamDescriptor::State::DRAINING:
                    nextState = StreamDescriptor::State::DRAIN_PAUSED;
                    break;
                case StreamDescriptor::State::TRANSFERRING:
                    nextState = StreamDescriptor::State::TRANSFER_PAUSED;
                    break;
                default:
                    populateReplyWrongState(&reply, command);
                    break;
            }
            if (nextState && nextState.value() == StreamDescriptor::State::DRAIN_PAUSED) {
                if (mDrainInternalState) {
                    if (mDrainInternalState.value() == DrainInternalState::DRAINING_en) {
                        nextInternalState = DrainInternalState::DRAIN_PAUSED_en;
                    } else if (mDrainInternalState.value() ==
                               DrainInternalState::DRAINING_en_sent) {
                        nextInternalState = DrainInternalState::DRAIN_PAUSED_en_sent;
                    } else {
                        HAL_LOGE << "bad drain internal state: "
                                 << toString(mDrainInternalState.value());
                        populateReplyWrongState(&reply, command);
                        break;
                    }
                }
            }
            if (nextState.has_value()) {
                if (::android::status_t status = mDriver->pause(); status == ::android::OK) {
                    mState = nextState.value();
                    mDrainInternalState = nextInternalState;
                    populateReply(&reply, mIsConnected);
                } else {
                    HAL_LOGE << "pause failed: " << status;
                    // uncomment below, to treat the failure as HARD error, stream not recoverable
                    // mState = StreamDescriptor::State::ERROR;
                }
            }
        } break;
        case Tag::flush: {
            std::optional<StreamDescriptor::State> nextState;
            if (mState == StreamDescriptor::State::PAUSED ||
                mState == StreamDescriptor::State::DRAIN_PAUSED ||
                mState == StreamDescriptor::State::TRANSFER_PAUSED) {
                if (auto status = mDriver->flush(); status == ::android::OK) {
                    mState = StreamDescriptor::State::IDLE;
                    mIsClipTransitionDataBurstsAvailable = false;
                    mDrainInternalState = {};
                    populateReply(&reply, mIsConnected);
                } else {
                    HAL_LOGE << "flush failed: " << status;
                    // uncomment below, to treat the failure as HARD error, stream not recoverable
                    // mState = StreamDescriptor::State::ERROR;
                }
            } else {
                populateReplyWrongState(&reply, command);
                break;
            }
        } break;
        case Tag::flushFromFrame: {
            populateReplyUnsupportedCommand(&reply, command);
            break;
        }
    }
    reply.state = mState;

    using LogSeverity = ::android::base::LogSeverity;
    const LogSeverity severity =
            (reply.status != STATUS_OK) ? LogSeverity::ERROR : LogSeverity::VERBOSE;
    STREAM_LOG(severity) << "writing reply " << reply.toString()
                         << (mDrainInternalState ? (": drain internal state:" +
                                                    toString(mDrainInternalState.value()))
                                                 : "");

    if (!mContext->getReplyMQ()->writeBlocking(&reply, 1)) {
        HAL_LOGE << "writing of reply " << reply.toString() << " to MQ failed ";
        mState = StreamDescriptor::State::ERROR;
        return Status::ABORT;
    }

    if (mPendingCallBack) {
        if (mPendingCallBack == StreamCallbackType::TR && handleTransferReady()) {
            mPendingCallBack = {};
        } else if (mPendingCallBack == StreamCallbackType::DR && handleDrainReady()) {
            mPendingCallBack = {};
        } else if (mPendingCallBack == StreamCallbackType::ER && handleError()) {
            mPendingCallBack = {};
        } else {
            if (command.getTag() != Tag::getStatus) {
                HAL_LOGE << "pending callback not handled, callback:" << *mPendingCallBack;
            }
        }
    }

    return Status::CONTINUE;
}

StreamOutWorkerLogic::Status StreamOutWorkerLogic::cycle() {
    StreamDescriptor::Command command{};
    if (!mContext->getCommandMQ()->readBlocking(&command, 1)) {
        HAL_LOGE << "reading of command from MQ failed";
        mState = StreamDescriptor::State::ERROR;
        return Status::ABORT;
    }

    if (mState == StreamDescriptor::State::DRAINING) {
        if (auto stateDurationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - mTransientStateStart);
            stateDurationMs >= mTransientStateDelayMs) {
            // In blocking mode, after some duration, expecting, hardware is drained.
            mState = StreamDescriptor::State::IDLE;
        }
    }

    HAL_LOGV << "received " << command.toString() << " in state " << toString(mState);

    StreamDescriptor::Reply reply{};
    reply.status = STATUS_BAD_VALUE;
    using Tag = StreamDescriptor::Command::Tag;
    switch (command.getTag()) {
        case Tag::halReservedExit: {
            if (const int32_t cookie = command.get<Tag::halReservedExit>();
                cookie == mContext->getInternalCommandCookie()) {
                mDriver->shutdown();
                setClosed();
                // This is an internal command, no need to reply.
                return Status::EXIT;
            } else {
                HAL_LOGW << "EXIT command has a bad cookie: " << cookie;
            }
        } break;
        case Tag::getStatus: {
            populateReply(&reply, mIsConnected);
        } break;
        case Tag::start: {
            std::optional<StreamDescriptor::State> nextState;
            switch (mState) {
                case StreamDescriptor::State::STANDBY:
                    nextState = StreamDescriptor::State::IDLE;
                    break;
                case StreamDescriptor::State::PAUSED:
                    nextState = StreamDescriptor::State::ACTIVE;
                    break;
                case StreamDescriptor::State::DRAIN_PAUSED:
                    nextState = StreamDescriptor::State::DRAINING;
                    break;
                default:
                    populateReplyWrongState(&reply, command);
            }
            if (nextState.has_value()) {
                if (::android::status_t status = mDriver->start(); status == ::android::OK) {
                    populateReply(&reply, mIsConnected);
                    if (*nextState == StreamDescriptor::State::IDLE ||
                        *nextState == StreamDescriptor::State::ACTIVE) {
                        mState = *nextState;
                    } else {
                        switchToTransientState(*nextState);
                    }
                } else {
                    HAL_LOGE << "start failed: " << status;
                    // uncomment below, to treat the failure as HARD error, stream not recoverable
                    // mState = StreamDescriptor::State::ERROR;
                }
            }
        } break;
        case Tag::burst: {
            if (const int32_t fmqByteCount = command.get<Tag::burst>(); fmqByteCount >= 0) {
                HAL_LOGV << "'" << toString(command.getTag()) << "' command for " << fmqByteCount
                         << " bytes";
                if (mState != StreamDescriptor::State::ERROR) {
                    std::optional<StreamDescriptor::State> nextState;
                    if (mState == StreamDescriptor::State::STANDBY ||
                        mState == StreamDescriptor::State::DRAIN_PAUSED ||
                        mState == StreamDescriptor::State::PAUSED) {
                        nextState = StreamDescriptor::State::PAUSED;
                    } else if (mState == StreamDescriptor::State::IDLE ||
                               mState == StreamDescriptor::State::DRAINING ||
                               mState == StreamDescriptor::State::ACTIVE) {
                        nextState = StreamDescriptor::State::ACTIVE;
                    } else {
                        populateReplyWrongState(&reply, command);
                        break;
                    }

                    bool isMmap = mContext->isMmap();
                    if (isMmap) {
                        if (!writeMmap(&reply)) {
                            HAL_LOGE << "mmap write failed";
                            break;
                        }
                    } else if (!write(fmqByteCount, &reply)) {
                        HAL_LOGE << "write failed, but dont put in error state ";
                    }

                    if (nextState && reply.fmqByteCount == fmqByteCount) {
                        mState = nextState.value();
                    } else {
                        HAL_LOGE << "couldn't write all data bytes: " << fmqByteCount
                                 << " != " << reply.fmqByteCount;
                    }
                } else {
                    populateReplyWrongState(&reply, command);
                }
            } else {
                HAL_LOGW << "invalid burst byte count: " << fmqByteCount;
            }
        } break;
        case Tag::drain: {
            if (auto currentMode = command.get<Tag::drain>();
                currentMode == StreamDescriptor::DrainMode::DRAIN_ALL) {
                if (mState == StreamDescriptor::State::ACTIVE) {
                    if (::android::status_t status = mDriver->drain(currentMode);
                        status == ::android::OK) {
                        populateReply(&reply, mIsConnected);
                        switchToTransientState(StreamDescriptor::State::DRAINING);
                    } else {
                        HAL_LOGE << "drain failed: " << status;
                        // uncomment below, to treat the failure as HARD error, stream not
                        // recoverable mState = StreamDescriptor::State::ERROR;
                    }
                } else {
                    populateReplyWrongState(&reply, command);
                }
            } else {
                HAL_LOGW << "invalid drain mode: " << toString(currentMode);
            }
        } break;
        case Tag::standby: {
            if (mState == StreamDescriptor::State::IDLE) {
                if (::android::status_t status = mDriver->standby(); status == ::android::OK) {
                    populateReply(&reply, mIsConnected);
                    mState = StreamDescriptor::State::STANDBY;
                } else {
                    HAL_LOGE << "standby failed: " << status;
                    // uncomment below, to treat the failure as HARD error, stream not recoverable
                    // mState = StreamDescriptor::State::ERROR;
                }
            } else {
                populateReplyWrongState(&reply, command);
            }
        } break;
        case Tag::pause: {
            std::optional<StreamDescriptor::State> nextState;
            switch (mState) {
                case StreamDescriptor::State::ACTIVE:
                    nextState = StreamDescriptor::State::PAUSED;
                    break;
                case StreamDescriptor::State::DRAINING:
                    nextState = StreamDescriptor::State::DRAIN_PAUSED;
                    break;
                default:
                    populateReplyWrongState(&reply, command);
            }
            if (nextState.has_value()) {
                if (::android::status_t status = mDriver->pause(); status == ::android::OK) {
                    populateReply(&reply, mIsConnected);
                    mState = nextState.value();
                } else {
                    HAL_LOGE << "pause failed: " << status;
                    // uncomment below, to treat the failure as HARD error, stream not recoverable
                    // mState = StreamDescriptor::State::ERROR;
                }
            }
        } break;
        case Tag::flush: {
            if (mState == StreamDescriptor::State::PAUSED ||
                mState == StreamDescriptor::State::DRAIN_PAUSED) {
                if (::android::status_t status = mDriver->flush(); status == ::android::OK) {
                    populateReply(&reply, mIsConnected);
                    mState = StreamDescriptor::State::IDLE;
                } else {
                    HAL_LOGE << "flush failed: " << status;
                    // uncomment below, to treat the failure as HARD error, stream not recoverable
                    // mState = StreamDescriptor::State::ERROR;
                }
            } else {
                populateReplyWrongState(&reply, command);
            }
        } break;
        case Tag::flushFromFrame: {
            populateReplyUnsupportedCommand(&reply, command);
            break;
        }
    }
    reply.state = mState;

    using LogSeverity = ::android::base::LogSeverity;
    const LogSeverity severity =
            (reply.status != STATUS_OK) ? LogSeverity::ERROR : LogSeverity::VERBOSE;
    STREAM_LOG(severity) << "writing reply " << reply.toString();

    if (!mContext->getReplyMQ()->writeBlocking(&reply, 1)) {
        HAL_LOGE << "writing of reply " << reply.toString() << " to MQ failed";
        mState = StreamDescriptor::State::ERROR;
        return Status::ABORT;
    }

    return Status::CONTINUE;
}

bool StreamOutWorkerLogic::write(size_t clientSize, StreamDescriptor::Reply* reply) {
    ATRACE_CALL();
    StreamContext::DataMQ* const dataMQ = mContext->getDataMQ();
    const size_t readByteCount = dataMQ->availableToRead();
    const size_t frameSize = mContext->getFrameSize();
    bool fatal = false;
    int32_t latency = mContext->getNominalLatencyMs();
    if (bool success = readByteCount > 0 ? dataMQ->read(&mDataBuffer[0], readByteCount) : true) {
        const bool isConnected = mIsConnected;
#ifdef VERY_VERBOSE_LOGGING
        HAL_LOGV << "reading of " << readByteCount << " bytes from data MQ"
                 << " succeeded; connected? " << isConnected;
#endif
        // Amount of data that the HAL module is going to actually use.
        size_t byteCount = std::min({clientSize, readByteCount, mDataBufferSize});

        size_t actualFrameCount = 0;
        // No need to check for connected device, if there is issue, write returns failure
        if (::android::status_t status = mDriver->transfer(mDataBuffer.get(), byteCount / frameSize,
                                                           &actualFrameCount, &latency);
            status != ::android::OK) {
            reply->status = STATUS_DEAD_OBJECT;
            fatal = true;
            HAL_LOGE << "write failed: " << status;
        }

        const size_t actualByteCount = actualFrameCount * frameSize;
        // Frames are consumed and counted regardless of the connection status.
        reply->fmqByteCount += actualByteCount;
        mContext->advanceFrameCount(actualFrameCount);
        populateReply(reply, isConnected);
    } else {
        HAL_LOGW << "reading of " << readByteCount << " bytes of data from MQ failed";
        reply->status = STATUS_NOT_ENOUGH_DATA;
    }
    return !fatal;
}

bool StreamOutWorkerLogic::writeMmap(StreamDescriptor::Reply* reply) {
    void* buffer = nullptr;
    size_t frameCount = 0;
    size_t actualFrameCount = 0;
    int32_t latency = mContext->getNominalLatencyMs();

    //  use default-initialized parameter values for mmap stream.
    if (::android::status_t status =
                mDriver->transfer(buffer, frameCount, &actualFrameCount, &latency);
        status == ::android::OK) {
        populateReply(reply, mIsConnected);
        reply->latencyMs = latency;
        return true;
    } else {
        HAL_LOGE << "transfer failed: " << status;
        return false;
    }
}

StreamCommonImpl::~StreamCommonImpl() {
    // It is responsibility of the class that implements 'DriverInterface' to call 'cleanupWorker'
    // in the destructor. Note that 'cleanupWorker' can not be properly called from this destructor
    // because any subclasses have already been destroyed and thus the 'DriverInterface'
    // implementation is not valid. Thus, here it can only be asserted whether the subclass has done
    // its job.
    if (!mWorkerStopIssued && !isClosed()) {
        HAL_LOGF << "the stream implementation must call 'cleanupWorker' "
                 << "in order to clean up the worker thread.";
    }
    HAL_LOGV << "destroy " << std::hex << this;
}

ndk::ScopedAStatus StreamCommonImpl::initInstance(
        const std::shared_ptr<StreamCommonInterface>& delegate) {
    mCommon = ndk::SharedRefBase::make<StreamCommonDelegator>(delegate);
    if (!mWorker->start()) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    if (auto flags = getContext().getFlags();
        (flags.getTag() == AudioIoFlags::Tag::input &&
         isBitPositionFlagSet(flags.template get<AudioIoFlags::Tag::input>(),
                              AudioInputFlags::FAST)) ||
        (flags.getTag() == AudioIoFlags::Tag::output &&
         isBitPositionFlagSet(flags.template get<AudioIoFlags::Tag::output>(),
                              AudioOutputFlags::FAST))) {
        // FAST workers should be run with a SCHED_FIFO scheduler, however the host process
        // might be lacking the capability to request it, thus a failure to set is not an error.
        pid_t workerTid = mWorker->getTid();
        if (workerTid > 0) {
            struct sched_param param;
            param.sched_priority = 3;  // Must match SchedulingPolicyService.PRIORITY_MAX (Java).
            HAL_LOGD << "increase scheduling for tid : " << workerTid;
            if (sched_setscheduler(workerTid, SCHED_FIFO | SCHED_RESET_ON_FORK, &param) != 0) {
                HAL_LOGW << "failed to set FIFO scheduler for a fast thread";
            }
        } else {
            HAL_LOGW << "invalid worker tid: " << workerTid;
        }
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus StreamCommonImpl::getStreamCommonCommon(
        std::shared_ptr<IStreamCommon>* _aidl_return) {
    if (!mCommon) {
        HAL_LOGF << "the common interface was not created";
    }
    *_aidl_return = mCommon.getInstance();
    HAL_LOGV << "returning " << _aidl_return->get()->asBinder().get();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus StreamCommonImpl::updateHwAvSyncId(int32_t in_hwAvSyncId) {
    HAL_LOGV << "id " << in_hwAvSyncId;
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus StreamCommonImpl::getVendorParameters(
        const std::vector<std::string>& in_ids, std::vector<VendorParameter>* _aidl_return) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus StreamCommonImpl::setVendorParameters(
        const std::vector<VendorParameter>& in_parameters, bool in_async) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus StreamCommonImpl::addEffect(
        const std::shared_ptr<::aidl::android::hardware::audio::effect::IEffect>& in_effect) {
    if (in_effect == nullptr) {
        HAL_LOGD << "null effect";
    } else {
        HAL_LOGD << "effect Binder" << in_effect->asBinder().get();
    }
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus StreamCommonImpl::removeEffect(
        const std::shared_ptr<::aidl::android::hardware::audio::effect::IEffect>& in_effect) {
    if (in_effect == nullptr) {
        HAL_LOGD << "null effect";
    } else {
        HAL_LOGD << "effect Binder" << in_effect->asBinder().get();
    }
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus StreamCommonImpl::close() {
    Module::outListMutex.lock();
    HAL_LOGD;
    if (!isClosed()) {
        stopAndJoinWorker();
        onClose();
        mWorker->setClosed();
        Module::outListMutex.unlock();
        return ndk::ScopedAStatus::ok();
    } else {
        HAL_LOGE << "stream was already closed";
        Module::outListMutex.unlock();
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
}

ndk::ScopedAStatus StreamCommonImpl::prepareToClose() {
    HAL_LOGD;
    if (!isClosed()) {
        return ndk::ScopedAStatus::ok();
    }
    HAL_LOGE << "stream was closed";
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
}

void StreamCommonImpl::cleanupWorker() {
    if (!isClosed()) {
        HAL_LOGE << "stream was not closed prior to destruction, resource leak";
        stopAndJoinWorker();
    }
}

void StreamCommonImpl::stopAndJoinWorker() {
    stopWorker();
    HAL_LOGD << "joining the worker thread...";
    mWorker->join();
    HAL_LOGD << "worker thread joined";
}

void StreamCommonImpl::stopWorker() {
    if (auto commandMQ = mContextRef.getCommandMQ(); commandMQ != nullptr) {
        HAL_LOGD << "asking the worker to exit...";
        auto cmd = StreamDescriptor::Command::make<StreamDescriptor::Command::Tag::halReservedExit>(
                mContextRef.getInternalCommandCookie());
        // Note: never call 'pause' and 'resume' methods of StreamWorker
        // in the HAL implementation. These methods are to be used by
        // the client side only. Preventing the worker loop from running
        // on the HAL side can cause a deadlock.
        if (!commandMQ->writeBlocking(&cmd, 1)) {
            HAL_LOGE << "failed to write exit command to the MQ";
        }
        HAL_LOGD << "done";
    }
    mWorkerStopIssued = true;
}

ndk::ScopedAStatus StreamCommonImpl::updateMetadataCommon(const Metadata& metadata) {
    HAL_LOGV;
    if (!isClosed()) {
        if (metadata.index() != mMetadata.index()) {
            HAL_LOGF << "changing metadata variant is not allowed";
        }
        mMetadata = metadata;
        return ndk::ScopedAStatus::ok();
    }
    HAL_LOGE << "stream was closed";
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
}

ndk::ScopedAStatus StreamCommonImpl::setConnectedDevices(
        const std::vector<::aidl::android::media::audio::common::AudioDevice>& devices) {
    mWorker->setIsConnected(!devices.empty());
    mConnectedDevices = devices;
    return ndk::ScopedAStatus::ok();
}

void StreamCommonImpl::setStreamMicMute(const bool muted) {
    return;
}

ndk::ScopedAStatus StreamCommonImpl::configureMMapStream(MmapBufferDescriptor* desc,
                                                         int32_t* bufferSizeFrames) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus StreamCommonImpl::createMmapBuffer(MmapBufferDescriptor* desc) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

namespace {
static std::map<AudioDevice, std::string> transformMicrophones(
        const std::vector<MicrophoneInfo>& microphones) {
    std::map<AudioDevice, std::string> result;
    std::transform(microphones.begin(), microphones.end(), std::inserter(result, result.begin()),
                   [](const auto& mic) { return std::make_pair(mic.device, mic.id); });
    return result;
}
} // namespace

StreamIn::StreamIn(StreamContext&& context, const std::vector<MicrophoneInfo>& microphones)
    : mContext(std::move(context)), mMicrophones(transformMicrophones(microphones)) {
    HAL_LOGV;
}

void StreamIn::defaultOnClose() {
    mContext.reset();
}

ndk::ScopedAStatus StreamIn::getActiveMicrophones(
        std::vector<MicrophoneDynamicInfo>* _aidl_return) {
    std::vector<MicrophoneDynamicInfo> result;
    std::vector<MicrophoneDynamicInfo::ChannelMapping> channelMapping{
            getChannelCount(getContext().getChannelLayout()),
            MicrophoneDynamicInfo::ChannelMapping::DIRECT};
    for (auto it = getConnectedDevices().begin(); it != getConnectedDevices().end(); ++it) {
        if (auto micIt = mMicrophones.find(*it); micIt != mMicrophones.end()) {
            MicrophoneDynamicInfo dynMic;
            dynMic.id = micIt->second;
            dynMic.channelMapping = channelMapping;
            result.push_back(std::move(dynMic));
        }
    }
    *_aidl_return = std::move(result);
    HAL_LOGD << "returning " << ::android::internal::ToString(*_aidl_return);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus StreamIn::getMicrophoneDirection(MicrophoneDirection* _aidl_return) {
    HAL_LOGV;
    (void)_aidl_return;
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus StreamIn::setMicrophoneDirection(MicrophoneDirection in_direction) {
    HAL_LOGV << "direction " << toString(in_direction);
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus StreamIn::getMicrophoneFieldDimension(float* _aidl_return) {
    HAL_LOGV;
    (void)_aidl_return;
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus StreamIn::setMicrophoneFieldDimension(float in_zoom) {
    HAL_LOGV << "zoom " << in_zoom;
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus StreamIn::getHwGain(std::vector<float>* _aidl_return) {
    HAL_LOGD;
    (void)_aidl_return;
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus StreamIn::setHwGain(const std::vector<float>& in_channelGains) {
    HAL_LOGD << "gains " << ::android::internal::ToString(in_channelGains);
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

StreamOut::StreamOut(StreamContext&& context, const std::optional<AudioOffloadInfo>& offloadInfo)
    : mContext(std::move(context)), mOffloadInfo(offloadInfo) {
    HAL_LOGV;
}

void StreamOut::defaultOnClose() {
    mContext.reset();
}

ndk::ScopedAStatus StreamOut::updateOffloadMetadata(
        const AudioOffloadMetadata& in_offloadMetadata) {
    if (isClosed()) {
        HAL_LOGE << "stream was closed";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    if (!mOffloadInfo.has_value()) {
        HAL_LOGE << "not a compressed offload stream";
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    if (in_offloadMetadata.sampleRate < 0) {
        HAL_LOGE << "invalid sample rate value: " << in_offloadMetadata.sampleRate;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (in_offloadMetadata.averageBitRatePerSecond < 0) {
        HAL_LOGE << "invalid average BPS value: " << in_offloadMetadata.averageBitRatePerSecond;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (in_offloadMetadata.delayFrames < 0) {
        HAL_LOGE << "invalid delay frames value: " << in_offloadMetadata.delayFrames;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (in_offloadMetadata.paddingFrames < 0) {
        HAL_LOGE << "invalid padding frames value: " << in_offloadMetadata.paddingFrames;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    mOffloadMetadata = in_offloadMetadata;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus StreamOut::getHwVolume(std::vector<float>* _aidl_return) {
    HAL_LOGD;
    (void)_aidl_return;
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus StreamOut::setHwVolume(const std::vector<float>& in_channelVolumes) {
    HAL_LOGD << "gains " << ::android::internal::ToString(in_channelVolumes);
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus StreamOut::getAudioDescriptionMixLevel(float* _aidl_return) {
    HAL_LOGD;
    (void)_aidl_return;
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus StreamOut::setAudioDescriptionMixLevel(float in_leveldB) {
    HAL_LOGD << "description mix level " << in_leveldB;
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus StreamOut::getDualMonoMode(AudioDualMonoMode* _aidl_return) {
    HAL_LOGD;
    (void)_aidl_return;
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus StreamOut::setDualMonoMode(AudioDualMonoMode in_mode) {
    HAL_LOGD << "dual mono mode " << toString(in_mode);
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus StreamOut::getRecommendedLatencyModes(
        std::vector<AudioLatencyMode>* _aidl_return) {
    HAL_LOGD;
    (void)_aidl_return;
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus StreamOut::setLatencyMode(AudioLatencyMode in_mode) {
    HAL_LOGD << "latency mode " << toString(in_mode);
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus StreamOut::getPlaybackRateParameters(
        ::aidl::android::media::audio::common::AudioPlaybackRate* _aidl_return) {
    if (!supportsPlaybackRate()) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    HAL_LOGD << mPlaybackRate.toString();
    *_aidl_return = mPlaybackRate;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus StreamOut::setPlaybackRateParameters(
        const ::aidl::android::media::audio::common::AudioPlaybackRate& in_playbackRate) {
    if (!supportsPlaybackRate()) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    auto status = setPlaybackRateImpl(in_playbackRate);
    if (status.isOk()) {
        mPlaybackRate = in_playbackRate;
        HAL_LOGD << mPlaybackRate.toString();
    } else {
        HAL_LOGE << "failed " << status <<" for " << mPlaybackRate.toString();
    }
    return status;
}

ndk::ScopedAStatus StreamOut::selectPresentation(int32_t in_presentationId, int32_t in_programId) {
    HAL_LOGD << "presentationId " << in_presentationId << ", programId " << in_programId;
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus StreamOut::setPlaybackRateImpl(
        const ::aidl::android::media::audio::common::AudioPlaybackRate& in_playbackRate) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

void StreamOut::applyPlaybackRateIfNonDefault() {
    if (!supportsPlaybackRate()) return;

    if (mPlaybackRate != sDefaultPlaybackRate) {
        HAL_LOGD << mPlaybackRate.toString();
        setPlaybackRateImpl(mPlaybackRate);
    }
}
}  // namespace qti::audio::core
