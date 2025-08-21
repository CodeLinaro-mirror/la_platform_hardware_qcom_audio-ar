/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL_StreamIn_QTI"

#include <cmath>

#include <aidl/android/hardware/audio/effect/IEffect.h>

#include <android-base/logging.h>
#include <audio_utils/clock.h>
#include <hardware/audio.h>
#include <qti-audio-core/Module.h>
#include <qti-audio-core/ModulePrimary.h>
#include <qti-audio-core/StreamInPrimary.h>
#include <system/audio.h>

using aidl::android::hardware::audio::common::AudioOffloadMetadata;
using aidl::android::hardware::audio::common::getChannelCount;
using aidl::android::hardware::audio::common::getFrameSizeInBytes;
using aidl::android::hardware::audio::common::SinkMetadata;
using aidl::android::hardware::audio::common::SourceMetadata;
using aidl::android::media::audio::common::AudioDevice;
using aidl::android::media::audio::common::AudioDeviceAddress;
using aidl::android::media::audio::common::AudioDualMonoMode;
using aidl::android::media::audio::common::AudioLatencyMode;
using aidl::android::media::audio::common::AudioOffloadInfo;
using aidl::android::media::audio::common::AudioPortExt;
using aidl::android::media::audio::common::AudioSource;
using aidl::android::media::audio::common::MicrophoneDynamicInfo;
using aidl::android::media::audio::common::MicrophoneInfo;

using ::aidl::android::hardware::audio::common::getChannelCount;
using ::aidl::android::hardware::audio::common::getFrameSizeInBytes;
using ::aidl::android::hardware::audio::core::IStreamCallback;
using ::aidl::android::hardware::audio::core::IStreamCommon;
using ::aidl::android::hardware::audio::core::StreamDescriptor;
using ::aidl::android::hardware::audio::core::VendorParameter;
using ::aidl::android::hardware::audio::effect::getEffectTypeUuidAcousticEchoCanceler;
using ::aidl::android::hardware::audio::effect::getEffectTypeUuidNoiseSuppression;
using ::aidl::android::media::audio::common::AudioDeviceType;
using ::aidl::android::media::audio::common::AudioDeviceDescription;

// uncomment this to enable logging of very verbose logs like burst commands.
// #define VERY_VERBOSE_LOGGING 1

namespace qti::audio::core {

#define READ_RETRY_COUNT 10

std::mutex StreamInPrimary::sinkMetadata_mutex_;

StreamInPrimary::StreamInPrimary(StreamContext&& context, const SinkMetadata& sinkMetadata,
                                 const std::vector<MicrophoneInfo>& microphones,
                                 std::vector<AudioDevice> AudioDevices)
    : StreamIn(std::move(context), microphones),
      StreamCommonImpl(&(StreamIn::mContext), sinkMetadata),
      mTag(getUsecaseTag(getContext().getMixPortConfig(), AudioDevices)),
      mTagName(getName(mTag)),
      mFrameSizeBytes(getContext().getFrameSize()),
      mMixPortConfig(getContext().getMixPortConfig()) {
    if (mTag == Usecase::PCM_RECORD) {
        mExt.emplace<PcmRecord>();
    } else if (mTag == Usecase::COMPRESS_CAPTURE) {
        mExt.emplace<CompressCapture>(mMixPortConfig.format.value(),
                                      mMixPortConfig.sampleRate.value().value,
                                      mMixPortConfig.channelMask.value());
    } else if (mTag == Usecase::VOIP_RECORD) {
        mExt.emplace<VoipRecord>();
    } else if (mTag == Usecase::MMAP_RECORD) {
        mExt.emplace<MMapRecord>();
    } else if (mTag == Usecase::VOICE_CALL_RECORD) {
        mExt.emplace<VoiceCallRecord>();
    } else if (mTag == Usecase::FAST_RECORD) {
        mExt.emplace<FastRecord>();
    } else if (mTag == Usecase::ULTRA_FAST_RECORD) {
        mExt.emplace<UltraFastRecord>();
    } else if (mTag == Usecase::HOTWORD_RECORD) {
        mExt.emplace<HotwordRecord>();
    }

    /**
     * In HIDL after open input stream there is subsequent call to
     *  update metadata, in AIDL its not present , so doing here.
     */
    updateMetadata(sinkMetadata);
    if (property_get_bool("persist.vendor.audio.hw_ts_enable",false)) {
        LOG(DEBUG) << "hw_ts_enable is true";
        hw_ts_enable = true;
    }
    else
        hw_ts_enable = false;
    std::ostringstream os;
    os << " : usecase: " << mTagName;
    os << " IoHandle:" << mMixPortConfig.ext.get<AudioPortExt::Tag::mix>().handle;
    mLogPrefix = os.str();
    readAt = {0};
    LOG(DEBUG) << __func__ << mLogPrefix;
}

StreamInPrimary::~StreamInPrimary() {
    shutdown_I();
    LOG(DEBUG) << __func__ << mLogPrefix;
}

ndk::ScopedAStatus StreamInPrimary::getActiveMicrophones(
        std::vector<MicrophoneDynamicInfo>* _aidl_return) {
    *_aidl_return = mPlatform.getMicrophoneDynamicInfo(mConnectedDevices);
    LOG(VERBOSE) << __func__ << mLogPrefix << " " << ::android::internal::ToString(*_aidl_return);
    return ndk::ScopedAStatus::ok();
}

// start of methods called from IModule
ndk::ScopedAStatus StreamInPrimary::setConnectedDevices(
        const std::vector<::aidl::android::media::audio::common::AudioDevice>& devices) {
    mWorker->setIsConnected(!devices.empty());
    mConnectedDevices = devices;
    return configureConnectedDevices_I();
}

ndk::ScopedAStatus StreamInPrimary::reconfigureConnectedDevices() {
    return configureConnectedDevices_I();
}

ndk::ScopedAStatus StreamInPrimary::configureConnectedDevices_I() {
    auto connectedPalDevices =
            mPlatform.configureAndFetchPalDevices(mMixPortConfig, mTag, mConnectedDevices);
    if (mTag == Usecase::PCM_RECORD || mTag == Usecase::COMPRESS_CAPTURE) {
        mPlatform.configurePalDevices(mMixPortConfig, connectedPalDevices);
        if (mPlatform.getTranslationRecordState()) {
            mPlatform.configurePalDevicesCustomKey(connectedPalDevices, "translate_record");
            LOG(INFO) << __func__ << "setting custom key as translate_record";
        }
    } else if (mTag == Usecase::ULTRA_FAST_RECORD) {
        auto countProxyDevices = std::count_if(mConnectedDevices.cbegin(), mConnectedDevices.cend(),
                                               isInputAFEProxyDevice);
        if (countProxyDevices > 0) {
            std::get<UltraFastRecord>(mExt).mIsWFDCapture = true;
            LOG(INFO) << __func__ << mLogPrefix
                      << ": ultra fast record on input AFE proxy (WFD client AHAL CAPTURE)";
        } else {
            std::get<UltraFastRecord>(mExt).mIsWFDCapture = false;
            auto channelCount = getChannelCount(mMixPortConfig.channelMask.value());
            if (channelCount == 2) {
                mPlatform.configurePalDevicesCustomKey(connectedPalDevices, "dual-mic");
            }
        }
    }

    if (this->mPalHandle != nullptr && connectedPalDevices.size() > 0) {
        if (int32_t ret = ::pal_stream_set_device(this->mPalHandle, connectedPalDevices.size(),
                                                  connectedPalDevices.data());
            ret) {
            LOG(ERROR) << __func__ << mLogPrefix << " failed pal_stream_set_device, ret:" << ret;
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
        }
    }

    auto devicesString = [](std::string prev, const auto& device) {
        return std::move(prev) + ';' + device.toString();
    };

    LOG(DEBUG) << __func__ << mLogPrefix << " stream is connected to devices:"
               << std::accumulate(mConnectedDevices.cbegin(), mConnectedDevices.cend(),
                                  std::string(""), devicesString);

    return ndk::ScopedAStatus::ok();
}

void StreamInPrimary::setStreamMicMute(const bool muted) {
    if (mPalHandle == nullptr) {
        return;
    }
    if (!mPlatform.setStreamMicMute(mPalHandle, muted)) {
        LOG(ERROR) << __func__ << mLogPrefix << " failed";
        return;
    }
}

struct BufferConfig StreamInPrimary::getBufferConfig() {
    return mPlatform.getBufferConfig(mMixPortConfig, mTag);
}

ndk::ScopedAStatus StreamInPrimary::configureMMapStream(int32_t* fd, int64_t* burstSizeFrames,
                                                        int32_t* flags, int32_t* bufferSizeFrames) {
    if (AudioExtension::getInstance().in_power_policy == POWER_POLICY_STATUS_OFFLINE) {
        LOG(ERROR) << "POWER POLICY OFFLINE please try again\n";
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    if (mTag != Usecase::MMAP_RECORD) {
        LOG(ERROR) << __func__ << mLogPrefix << " cannot call on non-MMAP stream types";
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    auto attr = mPlatform.getPalStreamAttributes(mMixPortConfig, true);
    if (!attr) {
        LOG(ERROR) << __func__ << mLogPrefix << " no pal attributes";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    attr->type = PAL_STREAM_ULTRA_LOW_LATENCY;
    auto palDevices = mPlatform.configureAndFetchPalDevices(mMixPortConfig, mTag, mConnectedDevices,
                                                            true /*dummyDevice*/);
    if (!palDevices.size()) {
        LOG(ERROR) << __func__ << mLogPrefix << " no connected devices on stream";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    uint64_t cookie = reinterpret_cast<uint64_t>(this);
    pal_stream_callback palFn = nullptr;
    attr->flags = static_cast<pal_stream_flags_t>(PAL_STREAM_FLAG_MMAP_NO_IRQ);

    if (int32_t ret = ::pal_stream_open(attr.get(), palDevices.size(), palDevices.data(), 0,
                                        nullptr, palFn, cookie, &(this->mPalHandle));
        ret) {
        LOG(ERROR) << __func__ << mLogPrefix << " pal_stream_open failed, ret:" << std::to_string(ret);
        mPalHandle = nullptr;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    auto bufConfig = getBufferConfig();
    const size_t ringBufSizeInBytes = bufConfig.bufferSize;
    const size_t ringBufCount = bufConfig.bufferCount;

    auto palBufferConfig = mPlatform.getPalBufferConfig(ringBufSizeInBytes, ringBufCount);
    LOG(DEBUG) << __func__ << mLogPrefix << " set pal_stream_set_buffer_size to "
                 << std::to_string(ringBufSizeInBytes) << " with count "
                 << std::to_string(ringBufCount);
    if (int32_t ret =
                ::pal_stream_set_buffer_size(this->mPalHandle, palBufferConfig.get(), nullptr);
        ret) {
        LOG(ERROR) << __func__ << mLogPrefix
                   << " pal_stream_set_buffer_size failed, ret:" << std::to_string(ret);
        ::pal_stream_close(mPalHandle);
        mPalHandle = nullptr;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    std::get<MMapRecord>(mExt).setPalHandle(mPalHandle);

    const auto frameSize = ::aidl::android::hardware::audio::common::getFrameSizeInBytes(
            mMixPortConfig.format.value(), mMixPortConfig.channelMask.value());
    int32_t ret = std::get<MMapRecord>(mExt).createMMapBuffer(frameSize, fd, burstSizeFrames, flags,
                                                              bufferSizeFrames);
    if (ret != 0) {
        LOG(ERROR) << __func__ << mLogPrefix << " createMMapBuffer failed";
        ::pal_stream_close(mPalHandle);
        mPalHandle = nullptr;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }


    if (mPlatform.getMicMuteStatus()) {
        setStreamMicMute(true);
    }

    LOG(INFO) << __func__ << mLogPrefix << ": stream is configured";

    return ndk::ScopedAStatus::ok();
}

// end of methods called from IModule

// start of driverInterface methods

::android::status_t StreamInPrimary::init() {
    return ::android::OK;
}

::android::status_t StreamInPrimary::drain(StreamDescriptor::DrainMode mode) {
    if (!mPalHandle) {
        LOG(WARNING) << __func__ << mLogPrefix << " stream is not configured";
        return ::android::OK;
    }
    if (mTag == Usecase::MMAP_RECORD) {
        // drain in MMAP is stop
        return stopMMAP();
    }
    return ::android::OK;
}

::android::status_t StreamInPrimary::flush() {
    if (!mPalHandle) {
        LOG(WARNING) << __func__ << mLogPrefix << " stream is not configured";
        return ::android::OK;
    }
    if (mTag == Usecase::MMAP_RECORD) {
        // Flush in MMAP is stop
        return stopMMAP();
    }
    return ::android::OK;
}

::android::status_t StreamInPrimary::pause() {
    // Todo check whether pause is possible in PAL

    if (mTag == Usecase::MMAP_RECORD) {
        // pause in MMAP is stop
        return stopMMAP();
    }
    shutdown_I();
    return ::android::OK;
}

void StreamInPrimary::resume() {
    // No op
}

::android::status_t StreamInPrimary::standby() {
    if (!mPalHandle) {
        LOG(WARNING) << __func__ << mLogPrefix << ": stream is not configured ";
        return ::android::OK;
    }

    if (mTag == Usecase::MMAP_RECORD) {
       if (AudioExtension::getInstance().in_power_policy == POWER_POLICY_STATUS_OFFLINE) {
            return stopMMAP();
       } else {
            return ::android::OK;
       }
    }

    shutdown_I();
    return ::android::OK;
}

::android::status_t StreamInPrimary::start() {
    LOG(DEBUG) << __func__ << mLogPrefix;
    if (!mPlatform.isSoundCardUp()) {
        LOG(ERROR) << __func__ << mLogPrefix << "Sound card is offline, returning...\n";
        return -EINVAL;
    }

    if (AudioExtension::getInstance().in_power_policy == POWER_POLICY_STATUS_OFFLINE) {
        LOG(ERROR) << "POWER POLICY OFFLINE please try again\n";
        return -EINVAL;
    }
    if (mTag == Usecase::MMAP_RECORD) {
        return startMMAP();
    }
    return ::android::OK;
}

::android::status_t StreamInPrimary::onReadError(const size_t sleepFrameCount) {
    shutdown_I();
    if (mTag == Usecase::COMPRESS_CAPTURE) {
        LOG(ERROR) << __func__ << mLogPrefix << ": cannot afford read failure for compress";
        return ::android::UNEXPECTED_NULL;
    }
    auto& sampleRate = mMixPortConfig.sampleRate.value().value;
    if (sampleRate == 0) {
        LOG(ERROR) << __func__ << mLogPrefix << ": cannot afford read failure, sampleRate is zero";
        return ::android::UNEXPECTED_NULL;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds((sleepFrameCount * 1000) / sampleRate));
    return ::android::OK;
}

int64_t StreamInPrimary::GetSourceLatency() {
    auto attr = mPlatform.getPalStreamAttributes(mMixPortConfig, true);
    switch (attr->type) {
        case PAL_STREAM_DEEP_BUFFER:
            return DEEP_BUFFER_PLATFORM_CAPTURE_DELAY;
        case PAL_STREAM_LOW_LATENCY:
            return LOW_LATENCY_PLATFORM_CAPTURE_DELAY;
        case PAL_STREAM_VOIP_TX:
            return VOIP_TX_PLATFORM_CAPTURE_DELAY;
        case PAL_STREAM_RAW:
            return RAW_STREAM_PLATFORM_CAPTURE_DELAY;
            // TODO: Add more streamtypes if available in pal
        default:
            return 0;
    }
}

::android::status_t StreamInPrimary::transfer(void* buffer, size_t frameCount,
                                              size_t* actualFrameCount, int32_t* latencyMs) {
    if (AudioExtension::getInstance().in_power_policy == POWER_POLICY_STATUS_OFFLINE) {
        LOG(ERROR) << "POWER POLICY OFFLINE please try again\n";
        return android::INVALID_OPERATION;
    }
    if (!mPalHandle) {
        configure();
        if (!mPalHandle) {
            LOG(ERROR) << __func__ << mLogPrefix << ": failed to configure";
            *actualFrameCount = frameCount;
            return onReadError(frameCount);
        }
    }
    if (frameCount == 0) {
        *actualFrameCount = 0;
        return burstZero();
    }
    if (mTag == Usecase::MMAP_RECORD) {
        return startMMAP();
    }

    pal_buffer palBuffer{};
    palBuffer.buffer = static_cast<uint8_t*>(buffer);
    palBuffer.size = frameCount * mFrameSizeBytes;
    palBuffer.ts = nullptr;
    uint64_t signed_frames = 0;
    uint64_t read_frames = 0;
    uint64_t kernel_frames = 0;
    size_t kernel_buffer_size = 0;
#ifdef HARDWARE_TIMESTAMP
    palBuffer.ts = (struct timespec *) calloc(1, 1 * sizeof(struct timespec));
    if (!palBuffer.ts) {
        LOG(ERROR) << "calloc failed for size: " << sizeof(struct timespec) << "for palBuffer.ts";
        return ::android::NO_MEMORY;
    }
#endif
#ifdef VERY_VERBOSE_LOGGING
    LOG(VERBOSE) << __func__ << mLogPrefix << ": framecount " << frameCount << " mFrameSizeBytes "
                 << mFrameSizeBytes;
#endif
    int32_t bytesRead = ::pal_stream_read(mPalHandle, &palBuffer);
    /* AudioFlinger will call Pause/flush and read again upon receiving 0 bytes.
     * This results VA buffering stop in PAL. Add retry mechanism to get valid data
     * for HOTWORD read stream
     */
    if (mTag == Usecase::HOTWORD_RECORD && bytesRead <= 0) {
        if (bytesRead == 0) {
            int32_t retryCnt = 0;
            do {
                bytesRead = ::pal_stream_read(mPalHandle, &palBuffer);
            } while (bytesRead == 0 && ++retryCnt < READ_RETRY_COUNT);
        } else {
            /* Send silence buffer to let app know to stop capture session.
             * This would avoid continous read from Audioflinger to HAL.
             */
            memset(palBuffer.buffer, 0, palBuffer.size);
            bytesRead = palBuffer.size;
        }
    } else if (bytesRead < 0) {
        LOG(ERROR) << __func__ << mLogPrefix << " read failed, ret:" << std::to_string(bytesRead);
        return ::android::NOT_ENOUGH_DATA;
    }

    if (mTag == Usecase::COMPRESS_CAPTURE) {
        auto& compressCapture = std::get<CompressCapture>(mExt);
        compressCapture.advanceReadCount();
        *latencyMs = compressCapture.getLatencyMs();
    } else if (mTag == Usecase::PCM_RECORD || mTag == Usecase::HOTWORD_RECORD) {
        *latencyMs = PcmRecord::kCaptureDurationMs;
    }
#ifndef HARDWARE_TIMESTAMP
    clock_gettime(CLOCK_MONOTONIC, &readAt);
#else
    if(palBuffer.ts) {
        readAt.tv_sec = palBuffer.ts->tv_sec;
        readAt.tv_nsec = palBuffer.ts->tv_nsec;
        free(palBuffer.ts);
    }
    LOG(VERBOSE) << "hw timestamp sec: " << readAt.tv_sec << "  nsec: " << readAt.tv_nsec;
#endif
    if (bytesRead < 0) {
        LOG(ERROR) << __func__ << mLogPrefix << " read failed, ret:" << std::to_string(bytesRead);
        *actualFrameCount = frameCount;
         return onReadError(frameCount);
    }
    else {
        *actualFrameCount = static_cast<size_t>(bytesRead) / mFrameSizeBytes;
    }

#ifdef VERY_VERBOSE_LOGGING
    LOG(VERBOSE) << __func__ << mLogPrefix << ": bytes read " << bytesRead << ", return frame count "
                 << *actualFrameCount;
#endif
    LOG(VERBOSE) << "bytes read: " << bytesRead;
    mBytesRead += bytesRead;
    LOG(VERBOSE) << "mBytes read: " << mBytesRead;
    read_frames = mBytesRead / mFrameSizeBytes;
    struct BufferConfig BufferConfig_ = getBufferConfig();
    kernel_buffer_size = BufferConfig_.bufferSize * BufferConfig_.bufferCount;
    kernel_frames = kernel_buffer_size / mFrameSizeBytes;
    signed_frames = read_frames + kernel_frames;
    if (hw_ts_enable) {
        LOG(DEBUG) << "signed frames " << signed_frames << " read frames " << read_frames <<
        " kernel frames " << kernel_frames << " pal_stream_handle = " << mPalHandle << " timestamp = " << readAt.tv_nsec;
    } else {
        LOG(VERBOSE) << "signed frames " << signed_frames << " read frames " << read_frames <<
        " kernel frames " << kernel_frames << " pal_stream_handle = " << mPalHandle << " timestamp = " << readAt.tv_nsec;
    }
    return ::android::OK;
}

::android::status_t StreamInPrimary::refinePosition(
        ::aidl::android::hardware::audio::core::StreamDescriptor::Reply* reply) {
    if (AudioExtension::getInstance().in_power_policy == POWER_POLICY_STATUS_OFFLINE) {
        LOG(ERROR) << "POWER POLICY OFFLINE please try again\n";
        return android::INVALID_OPERATION;
    }
    if (mTag == Usecase::COMPRESS_CAPTURE) {
        auto& compressCapture = std::get<CompressCapture>(mExt);
        reply->observable.frames = compressCapture.getPositionInFrames();
    } else if (mTag == Usecase::MMAP_RECORD) {
        if (int32_t ret = std::get<MMapRecord>(mExt).getMMapPosition(&(reply->hardware.frames),
                                                                     &(reply->hardware.timeNs));
            ret != 0) {
            return android::INVALID_OPERATION;
        }
    }
    return ::android::OK;
}

void StreamInPrimary::shutdown() {
    return shutdown_I();
}

// end of driverInterface methods

// start of StreamCommonInterface methods

::android::status_t StreamInPrimary::getHwTimeStamp(::aidl::android::hardware::audio::core::StreamDescriptor::Reply* reply) {
    if (!reply) {
        LOG(ERROR) << "Null in reply - " << "Failed to get hw timestamp";
        return ::android::INVALID_OPERATION;
    }
    int64_t dsp_latency = 0;
    dsp_latency = GetSourceLatency();
#ifndef HARDWARE_TIMESTAMP
    reply->observable.timeNs = (readAt.tv_sec * 1000000000LL) + readAt.tv_nsec - (dsp_latency * 1000LL);
#else
    reply->observable.timeNs = (readAt.tv_sec * 1000000000LL) + readAt.tv_nsec;
#endif
    return ::android::OK;
}

void StreamInPrimary::checkHearingAidRoutingForVoice(const Metadata& metadata, bool voiceActive) {

    if (!voiceActive) {
        return;
    }

    std::vector<AudioDevice> devices;
    AudioDevice device;

    device.type.type = AudioDeviceType::OUT_HEARING_AID;
    device.type.connection = AudioDeviceDescription::CONNECTION_WIRELESS;
    devices.push_back(device);

    ::aidl::android::hardware::audio::common::SinkMetadata sinkMetadata =
            std::get<::aidl::android::hardware::audio::common::SinkMetadata>(metadata);

    for (auto& item : sinkMetadata.tracks) {
         if (item.destinationDevice.has_value()) {
             if (item.destinationDevice.value().type.type == AudioDeviceType::OUT_HEARING_AID) {
                 if (auto telephony = mContext.getTelephony().lock()) {
                     LOG(DEBUG) << __func__ << " Hearing aid device , calling voice routing";
                     telephony->setDevices(devices, true);
                 }
                 break;
             }
         }
    }
}

ndk::ScopedAStatus StreamInPrimary::updateMetadataCommon(const Metadata& metadata) {
    if (!isClosed()) {
        if (metadata.index() != mMetadata.index()) {
            LOG(FATAL) << __func__ << mLogPrefix << ": changing metadata variant is not allowed";
        }
        mMetadata = metadata;
    }
    int callState = mPlatform.getCallState();
    int callMode = mPlatform.getCallMode();
    bool voiceActive = ((callState == 2) || (callMode == 2));

    /**
      * Based on sink metadata of recording session, we might need
      * to update the voice routing if dest device in metadata is
      * hearing aid.
      */
    checkHearingAidRoutingForVoice(metadata, voiceActive);

    StreamInPrimary::sinkMetadata_mutex_.lock();
    setAggregateSinkMetadata(voiceActive);
    StreamInPrimary::sinkMetadata_mutex_.unlock();
    LOG(ERROR) << __func__ << mLogPrefix << ": stream was closed";
    // return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    return ndk::ScopedAStatus::ok();
}

int32_t StreamInPrimary::setAggregateSinkMetadata(bool voiceActive) {
    ssize_t track_count_total = 0;

    std::vector<record_track_metadata_t> total_tracks;
    sink_metadata_t btSinkMetadata;

    ModulePrimary::inListMutex.lock();
    std::vector<std::weak_ptr<StreamIn>>& inStreams = ModulePrimary::getInStreams();
    // Dont send metadata if voice is active
    if (voiceActive || inStreams.empty()) {
        ModulePrimary::inListMutex.unlock();
        return 0;
    }
    auto removeStreams = [&](std::weak_ptr<StreamIn> streamIn) -> bool {
        if (!streamIn.lock()) return true;
        return streamIn.lock()->isClosed();
    };

    inStreams.erase(std::remove_if(inStreams.begin(), inStreams.end(), removeStreams),
                    inStreams.end());

    for (auto it = inStreams.begin(); it < inStreams.end(); it++) {
        if (it->lock() && !it->lock()->isClosed()) {
            ::aidl::android::hardware::audio::common::SinkMetadata sinkMetadata;
            it->lock()->getMetadata(sinkMetadata);
            track_count_total += sinkMetadata.tracks.size();
        } else {
        }
    }
    LOG(VERBOSE) << __func__ << mLogPrefix << " trackCount " << track_count_total <<
                " streamSize " << inStreams.size();
    if (track_count_total == 0) {
        ModulePrimary::inListMutex.unlock();
        return 0;
    }

    total_tracks.resize(track_count_total);
    btSinkMetadata.track_count = track_count_total;
    btSinkMetadata.tracks = total_tracks.data();

    for (auto it = inStreams.begin(); it != inStreams.end(); it++) {
        ::aidl::android::hardware::audio::common::SinkMetadata sinkMetadata;
        if (it->lock()) {
            it->lock()->getMetadata(sinkMetadata);
            for (auto& item : sinkMetadata.tracks) {
                btSinkMetadata.tracks->source = static_cast<audio_source_t>(item.source);
                ++btSinkMetadata.tracks;
            }
        }
    }

    btSinkMetadata.tracks = total_tracks.data();
    pal_set_param(PAL_PARAM_ID_SET_SINK_METADATA, (void*)&btSinkMetadata, 0);
    ModulePrimary::inListMutex.unlock();
    return 0;
}

ndk::ScopedAStatus StreamInPrimary::getVendorParameters(
        const std::vector<std::string>& in_ids, std::vector<VendorParameter>* _aidl_return) {
    if (mTag == Usecase::COMPRESS_CAPTURE) {
        auto& compressCapture = std::get<CompressCapture>(mExt);
        return compressCapture.getVendorParameters(in_ids, _aidl_return);
    }
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus StreamInPrimary::setVendorParameters(
        const std::vector<VendorParameter>& in_parameters, bool in_async) {
    if (mTag == Usecase::COMPRESS_CAPTURE) {
        auto& compressCapture = std::get<CompressCapture>(mExt);
        return compressCapture.setVendorParameters(in_parameters, in_async);
    }
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus StreamInPrimary::addEffect(
        const std::shared_ptr<::aidl::android::hardware::audio::effect::IEffect>& in_effect) {
    if (in_effect == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    ::aidl::android::hardware::audio::effect::Descriptor desc;
    auto status = in_effect->getDescriptor(&desc);
    if (!status.isOk()) {
        LOG(ERROR) << __func__ << mLogPrefix << "error fetching descriptor";
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    const auto& typeUUID = desc.common.id.type;

    if (typeUUID == getEffectTypeUuidAcousticEchoCanceler()) {
        if (!mAECEnabled) {
            mAECEnabled = true;
            applyEffects();
        }
    } else if (typeUUID == getEffectTypeUuidNoiseSuppression()) {
        if (!mNSEnabled) {
            mNSEnabled = true;
            applyEffects();
        }
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus StreamInPrimary::removeEffect(
        const std::shared_ptr<::aidl::android::hardware::audio::effect::IEffect>& in_effect) {
    if (in_effect == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    ::aidl::android::hardware::audio::effect::Descriptor desc;
    auto status = in_effect->getDescriptor(&desc);
    if (!status.isOk()) {
        LOG(ERROR) << __func__ << mLogPrefix << "error fetching descriptor";
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    const auto& typeUUID = desc.common.id.type;

    if (typeUUID == getEffectTypeUuidAcousticEchoCanceler()) {
        if (mAECEnabled) {
            mAECEnabled = false;
            applyEffects();
        }
    } else if (typeUUID == getEffectTypeUuidNoiseSuppression()) {
        if (mNSEnabled) {
            mNSEnabled = false;
            applyEffects();
        }
    }

    return ndk::ScopedAStatus::ok();
}

// end of StreamCommonInterface methods

size_t StreamInPrimary::getPlatformDelay() const noexcept {
    return 0;
}

void StreamInPrimary::updatePalDeviceForBusAddr(struct pal_device* devices, uint32_t numDevices,
                                                std::string busAddressString) {
    if (!devices) {
        LOG(ERROR) << __func__ << "invalid input paramter";
        return;
    }
    LOG(DEBUG) << "device: " << devices << "num devices: " << numDevices;
    std::unordered_map<std::string, pal_device_id_t> busAddressMap = {
            {"BUS04_INPUT", PAL_DEVICE_IN_HANDSET_MIC},
            {"BUS09_INPUT_FRONT_PASSENGER", PAL_DEVICE_IN_A2B_MIC},
            {"BUS17_INPUT_REAR_SEAT", PAL_DEVICE_IN_A2B2_MIC}};

    for (uint32_t devIdx = 0; devIdx < numDevices; devIdx++) {
        auto it = busAddressMap.find(busAddressString);
        if (it != busAddressMap.end()) {
            devices[devIdx].id = it->second;
        } else {
            LOG(ERROR) << __func__ << "unknown bus address: " << busAddressString;
            return;  // or some other default value
        }
    }
    return;
}

void StreamInPrimary::configure() {

    if(hasInputMMapFlag(mMixPortConfig.flags.value())){
        // this API doesn't handle for MMAP
        return;
    }
    std::string deviceAddress = "";
    const auto startTime = std::chrono::steady_clock::now();
    auto attr = mPlatform.getPalStreamAttributes(mMixPortConfig, true);
    LOG(INFO) << __func__ << " : configure : Enter";
    if (AudioExtension::getInstance().in_power_policy == POWER_POLICY_STATUS_OFFLINE) {
        LOG(ERROR) << "POWER POLICY OFFLINE please try again\n";
        return;
    }
    auto palDevices = mPlatform.configureAndFetchPalDevices(mMixPortConfig, mTag, mConnectedDevices);
    if (!attr) {
        LOG(ERROR) << __func__ << mLogPrefix << " no pal attributes";
        return;
    }
    if (!mConnectedDevices.empty()) {
        deviceAddress =  mConnectedDevices[0].address.get<AudioDeviceAddress::Tag::id>();
        LOG(INFO) << __func__ << "configure(): deviceAddress " << deviceAddress;
        attr->bus_addr = new char[deviceAddress.length() + 1];
        strlcpy(attr->bus_addr, deviceAddress.c_str(), deviceAddress.length() + 1);
    } else {
        LOG(DEBUG) << __func__ << mLogPrefix << ": connected device empty";
    }
    if (mTag == Usecase::PCM_RECORD) {
        LOG(DEBUG) << __func__ << " : PCM_RECORD usecase" << "Bus Address: " << attr->bus_addr;
        if ((std::strcmp(attr->bus_addr, "BUS04_INPUT") == 0) || (std::strcmp(attr->bus_addr, "BUS09_INPUT_FRONT_PASSENGER") == 0) || (std::strcmp(attr->bus_addr, "BUS17_INPUT_REAR_SEAT") == 0)) {
            attr->type = PAL_STREAM_CAPTURE_BUS;
        }
        else {
        attr->type = PAL_STREAM_DEEP_BUFFER;
        }
        const auto& source = getAudioSource(mMixPortConfig);
        if (source) {
            if (source.value() == AudioSource::ECHO_REFERENCE) {
                attr->type = PAL_STREAM_RAW;
                LOG(INFO) << __func__ << mLogPrefix << ": echo reference capture";
            } else if (source.value() == AudioSource::UNPROCESSED) {
                attr->type = PAL_STREAM_RAW;
                LOG(INFO) << __func__ << mLogPrefix << ": unprocessed capture";
            } else if (source.value() == AudioSource::VOICE_RECOGNITION) {
                attr->type = PAL_STREAM_DEEP_BUFFER;
                LOG(INFO) << __func__ << mLogPrefix << ": voice recognition capture";
            } else {
                auto countTelephonyRxDevices =
                     std::count_if(mConnectedDevices.cbegin(), mConnectedDevices.cend(),
                                   isTelephonyRXDevice);
                if (countTelephonyRxDevices > 0) {
                    attr->type = PAL_STREAM_PROXY;
                    attr->info.opt_stream_info.tx_proxy_type = PAL_STREAM_PROXY_TX_TELEPHONY_RX;
                    LOG(DEBUG) << __func__ << mLogPrefix << ": proxy capture for telephony rx";
                }
           }
        }
        if (mPlatform.getTranslationRecordState()) {
            mPlatform.configurePalDevicesCustomKey(palDevices, "translate_record");
            LOG(INFO) << __func__ << ": setting custom key as translate_record";
        }
    } else if (mTag == Usecase::COMPRESS_CAPTURE) {
        attr->type = PAL_STREAM_COMPRESSED;
    } else if (mTag == Usecase::VOIP_RECORD) {
        attr->type = PAL_STREAM_VOIP_TX;
    } else if (mTag == Usecase::VOICE_CALL_RECORD) {
        attr->type = PAL_STREAM_VOICE_CALL_RECORD;
        attr->info.voice_rec_info.record_direction =
                std::get<VoiceCallRecord>(mExt).getRecordDirection(mMixPortConfig);
    } else if (mTag == Usecase::FAST_RECORD) {
        attr->type = PAL_STREAM_LOW_LATENCY;
    } else if (mTag == Usecase::ULTRA_FAST_RECORD) {
        if (std::get<UltraFastRecord>(mExt).mIsWFDCapture) {
            attr->type = PAL_STREAM_PROXY;
            attr->info.opt_stream_info.tx_proxy_type = PAL_STREAM_PROXY_TX_WFD;
        } else {
            attr->type = PAL_STREAM_ULTRA_LOW_LATENCY;
            attr->flags = PAL_STREAM_FLAG_MMAP;
        }
    } else if (mTag == Usecase::HOTWORD_RECORD) {
        mPalHandle = std::get<HotwordRecord>(mExt).getPalHandle(mMixPortConfig);
        return;
    } else {
        LOG(ERROR) << __func__ << mLogPrefix << " invalid usecase to configure";
        return;
    }

    LOG(VERBOSE) << __func__ << mLogPrefix << " assigned pal stream type:" << attr->type;

    updatePalDeviceForBusAddr(palDevices.data(), palDevices.size(), deviceAddress);
    if (!palDevices.size()) {
        LOG(ERROR) << __func__ << mLogPrefix << " no connected devices on stream!!";
        return;
    }

    if (mTag == Usecase::PCM_RECORD || mTag == Usecase::COMPRESS_CAPTURE) {
        LOG(DEBUG) << __func__ << mLogPrefix << " PalDevices is config";
        mPlatform.configurePalDevices(mMixPortConfig, palDevices);
    }

    uint64_t cookie = reinterpret_cast<uint64_t>(this);
    pal_stream_callback palFn = nullptr;

    const auto palOpenApiStartTime = std::chrono::steady_clock::now();
    if (int32_t ret = ::pal_stream_open(attr.get(), palDevices.size(), palDevices.data(), 0,
                                        nullptr, palFn, cookie, &(mPalHandle));
        ret) {
        LOG(ERROR) << __func__ << mLogPrefix << " pal_stream_open failed!!! ret:" << ret;
        mPalHandle = nullptr;
        return;
    }
    const auto palOpenApiEndTime = std::chrono::steady_clock::now();

    auto bufConfig = getBufferConfig();
    const size_t ringBufSizeInBytes = bufConfig.bufferSize;
    const size_t ringBufCount = bufConfig.bufferCount;
    auto palBufferConfig = mPlatform.getPalBufferConfig(ringBufSizeInBytes, ringBufCount);
    LOG(DEBUG) << __func__ << mLogPrefix << " set pal_stream_set_buffer_size to " << ringBufSizeInBytes
                 << " with count " << ringBufCount;
    if (int32_t ret = ::pal_stream_set_buffer_size(mPalHandle, palBufferConfig.get(), nullptr);
        ret) {
        LOG(ERROR) << __func__ << mLogPrefix << " pal_stream_set_buffer_size failed!!! ret:" << ret;
        ::pal_stream_close(mPalHandle);
        mPalHandle = nullptr;
        return;
    }
    LOG(VERBOSE) << __func__ << mLogPrefix << " pal_stream_set_buffer_size successful";

    if (mTag == Usecase::COMPRESS_CAPTURE) {
        std::get<CompressCapture>(mExt).setPalHandle(mPalHandle);
        if (bool isConfigured = std::get<CompressCapture>(mExt).configureCodecInfo();
            !isConfigured) {
            ::pal_stream_close(mPalHandle);
            mPalHandle = nullptr;
            return;
        }
    }

    const auto palStartApiStartTime = std::chrono::steady_clock::now();
    if (int32_t ret = ::pal_stream_start(this->mPalHandle); ret) {
        LOG(ERROR) << __func__ << mLogPrefix << " pal_stream_start failed!! ret:" << ret;
        ::pal_stream_close(mPalHandle);
        mPalHandle = nullptr;
        return;
    }

    if (mPlatform.getMicMuteStatus()) {
        setStreamMicMute(true);
    }

    const auto palStartApiEndTime = std::chrono::steady_clock::now();

    if (!mEffectsApplied)
        applyEffects();

    LOG(DEBUG) << __func__ << mLogPrefix << " : stream is configured";

    const auto endTime = std::chrono::steady_clock::now();
    using FloatMillis = std::chrono::duration<float, std::milli>;
    const float palStreamOpenTimeTaken =
            std::chrono::duration_cast<FloatMillis>(palOpenApiEndTime - palOpenApiStartTime)
                    .count();
    const float palStreamStartTimeTaken =
            std::chrono::duration_cast<FloatMillis>(palStartApiEndTime - palStartApiStartTime)
                    .count();
    const float timeTaken = std::chrono::duration_cast<FloatMillis>(endTime - startTime).count();
    LOG(INFO) << __func__ << mLogPrefix << ": completed in " << timeTaken
              << " ms [pal_stream_open: " << palStreamOpenTimeTaken
              << ", ms pal_stream_start: " << palStreamStartTimeTaken << " ms]";
}

void StreamInPrimary::applyEffects() {
    if (mPalHandle == nullptr) {
        // try to applyEffects after pal_stream_start
        mEffectsApplied = false;
        return;
    }

    /*
     * While enabling/ disabling AEC or NS check the other effect is enabled or not.
     * Take action accordingly. In case AEC is disabled, but NS is still
     * enabled then NS has to be enabled alone, otherwise disable both ECNS
     * This can be decided based on the state of other effect.
     */

    pal_audio_effect_t type = PAL_AUDIO_EFFECT_ECNS;
    bool enable = mAECEnabled || mNSEnabled;

    if (mAECEnabled && mNSEnabled) {
        type = PAL_AUDIO_EFFECT_ECNS;
    } else if (mAECEnabled) {
        type = PAL_AUDIO_EFFECT_EC;
    } else if (mNSEnabled) {
        type = PAL_AUDIO_EFFECT_NS;
    }

    LOG(DEBUG) << __func__ << mLogPrefix << " apply effects aec " << mAECEnabled << " ns "
               << mNSEnabled << " type " << type << " enable " << enable;
    int ret = pal_add_remove_effect(mPalHandle, type, enable);
    mEffectsApplied = (ret == 0);
}

void StreamInPrimary::shutdown_I() {
    LOG(DEBUG) << __func__ << mLogPrefix;
    if (mTag == Usecase::MMAP_RECORD) {
        std::get<MMapRecord>(mExt).setPalHandle(nullptr);
    }
    mPalHandleMutex.lock();
    LOG(DEBUG) << " stream handle: " << mPalHandle;
    mEffectsApplied = true;
    if (mPalHandle != nullptr) {
        if (mTag == Usecase::HOTWORD_RECORD) {
            ::pal_stream_set_param(mPalHandle, PAL_PARAM_ID_STOP_BUFFERING, nullptr);
        } else {
            ::pal_stream_stop(mPalHandle);
            ::pal_stream_close(mPalHandle);
        }
    }
    mPalHandle = nullptr;
    mPalHandleMutex.unlock();
}

::android::status_t StreamInPrimary::burstZero() {
    LOG(VERBOSE) << __func__ << mLogPrefix;
    if (mTag == Usecase::MMAP_RECORD) {
        return startMMAP();
    }

    return ::android::OK;
}

::android::status_t StreamInPrimary::startMMAP() {
    auto& mmap = std::get<MMapRecord>(mExt);
    if (auto ret = mmap.start(); ret) {
        LOG(ERROR) << __func__ << mLogPrefix << ": failed";
        return ret;
    }
    return ::android::OK;
}

::android::status_t StreamInPrimary::stopMMAP() {
    auto& mmap = std::get<MMapRecord>(mExt);
    if (auto ret = mmap.stop(); ret) {
        LOG(ERROR) << __func__ << mLogPrefix << ": failed";
        return ret;
    }
    return ::android::OK;
}

} // namespace qti::audio::core
