/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <cmath>

#define LOG_TAG "AHAL_StreamMmap_QTI"

#include <android-base/logging.h>
#include <audio_utils/clock.h>
#include <hardware/audio.h>
#include <qti-audio-core/StreamMmapBase.h>

#include <qti-audio/PlatformConverter.h>
#include <qti-audio-core/PlatformUtils.h>

using aidl::android::hardware::audio::common::AudioOffloadMetadata;
using aidl::android::hardware::audio::common::SinkMetadata;
using aidl::android::hardware::audio::common::SourceMetadata;
using aidl::android::media::audio::common::AudioChannelLayout;
using aidl::android::media::audio::common::AudioDevice;
using aidl::android::media::audio::common::AudioDualMonoMode;
using aidl::android::media::audio::common::AudioLatencyMode;
using aidl::android::media::audio::common::AudioOffloadInfo;
using aidl::android::media::audio::common::AudioPlaybackRate;
using aidl::android::media::audio::common::MicrophoneDynamicInfo;
using aidl::android::media::audio::common::MicrophoneInfo;

using ::aidl::android::hardware::audio::core::IStreamCallback;
using ::aidl::android::hardware::audio::core::IStreamCommon;
using ::aidl::android::hardware::audio::core::MmapBufferDescriptor;
using ::aidl::android::hardware::audio::core::StreamDescriptor;
using ::aidl::android::hardware::audio::core::VendorParameter;

using aidl::android::media::audio::common::AudioPortExt;

namespace qti::audio::core {

// Round up to the next buffer boundary to mark the beginning of the session.
int64_t roundUpToBufferBoundary(int64_t frames, int64_t bufferSize) {
    return ((frames + bufferSize - 1) / bufferSize) * bufferSize;
}

StreamMmapBase::StreamMmapBase(StreamContext* context, const Metadata& metadata, const bool input)
    : StreamCommonImpl(context, metadata),
      mTag(getUsecaseTag(getContext().getMixPortConfig())),
      mTagName(getName(mTag)),
      mMixPortConfig(getContext().getMixPortConfig()),
      mIsInput(input) {
    std::ostringstream os;
    os << "[id:" << mMixPortConfig.id;
    os << ",io:" << mMixPortConfig.ext.get<AudioPortExt::Tag::mix>().handle <<"]";
    os << ": usecase: " << mTagName << " ";
    mLogPrefix = os.str();

    LOG(DEBUG) << __func__ << mLogPrefix <<" created " << mMixPortConfig.toString();
}

StreamMmapBase::~StreamMmapBase() {
    cleanupWorker();
    LOG(DEBUG) << __func__ << mLogPrefix << "destroyed";
}

ndk::ScopedAStatus StreamMmapBase::getVendorParameters(const std::vector<std::string>& in_ids,
                                                       std::vector<VendorParameter>* _aidl_return) {
    for (const auto& id : in_ids) {
        if (id == kAospCreateMmapBuffer) {
            LOG(DEBUG) << __func__ << mLogPrefix << " " << id;
            MmapBufferDescriptor desc;
            auto status = createOrGetMmapBuffer(&desc);
            if (!status.isOk()) {
                LOG(ERROR) << __func__ << mLogPrefix << " failed in" << id;
                return status;
            }
            VendorParameter createMmapBuffer{.id = id};
            createMmapBuffer.ext.setParcelable(desc);
            _aidl_return->push_back(std::move(createMmapBuffer));
        }
    }

    if (in_ids.size() != _aidl_return->size())
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus StreamMmapBase::setVendorParameters(
        const std::vector<::aidl::android::hardware::audio::core::VendorParameter>& in_parameters,
        bool in_async) {
    unsigned long served = 0;
    for (const auto& param : in_parameters) {
        if (param.id == kAospCreateMmapBuffer) {
            LOG(DEBUG) << __func__ << mLogPrefix << " " << param.id;
            // dummy just to indicate FWK that it is supported.
            served++;
        }
    }
    if (served != in_parameters.size())
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    return ndk::ScopedAStatus::ok();
}

bool StreamMmapBase::isValid() {
    if ((mMmapBufferDesc.sharedMemory.fd.get() == -1 || mMmapBufferDesc.sharedMemory.size == 0 ||
         mMmapBufferDesc.burstSizeFrames == 0)) {
        LOG(DEBUG) << __func__ << mLogPrefix << " "
                   << "invalid mmapBuffer" << mMmapBufferDesc.toString();
        return false;
    }
    return true;
}

ndk::ScopedAStatus StreamMmapBase::fillDescriptor(MmapBufferDescriptor* desc) {
    if (!isValid()) {
        LOG(ERROR) << __func__ << mLogPrefix << " mmap desc invalid ";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    desc->sharedMemory.fd = mMmapBufferDesc.sharedMemory.fd.dup();
    desc->sharedMemory.size = mMmapBufferDesc.sharedMemory.size;
    desc->burstSizeFrames = mMmapBufferDesc.burstSizeFrames;
    desc->flags = mMmapBufferDesc.flags;
    LOG(VERBOSE) << __func__ << mLogPrefix << " " << desc->toString();
    return ndk::ScopedAStatus::ok();
}

/*
 * cached MmapBuffer can be done when stream is opened but not started
 */
ndk::ScopedAStatus StreamMmapBase::getCachedMmapBuffer(MmapBufferDescriptor* desc) {
    if (!mClosed) {
        LOG(ERROR) << __func__ << mLogPrefix << " called in invalid state return EX_ILLEGAL_STATE";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    return fillDescriptor(desc);
}

ndk::ScopedAStatus StreamMmapBase::createOrGetMmapBuffer(MmapBufferDescriptor* desc) {
    if (mPalHandle == nullptr) {
        int32_t bufferSizeFrames;
        return configureMMapStream(desc, &bufferSizeFrames);
    } else {
        LOG(DEBUG) << __func__ << mLogPrefix << " use cached buffer";
        return getCachedMmapBuffer(desc);
    }
}

ndk::ScopedAStatus StreamMmapBase::createMMapBuffer(int64_t frameSize, MmapBufferDescriptor* desc,
                                                    int32_t* bufferSizeFrames) {
    if (!mPalHandle) {
        LOG(ERROR) << __func__ << mLogPrefix << ": pal stream handle is null";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    struct pal_mmap_buffer palMMapBuf;
    if (int32_t ret = pal_stream_create_mmap_buffer(mPalHandle, frameSize, &palMMapBuf); ret) {
        LOG(ERROR) << __func__ << mLogPrefix << ": pal stream create mmap buffer failed "
                   << "returned " << ret;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    mMmapBufferDesc.sharedMemory.fd = ndk::ScopedFileDescriptor(palMMapBuf.fd);
    mMmapBufferDesc.sharedMemory.size =
            palMMapBuf.buffer_size_frames *
            getFrameSizeInBytes(mMixPortConfig.format.value(), mMixPortConfig.channelMask.value());
    mMmapBufferDesc.burstSizeFrames = palMMapBuf.burst_size_frames;
    mMmapBufferDesc.flags = palMMapBuf.flags;

    *bufferSizeFrames = mBufferSizeInFrames = palMMapBuf.buffer_size_frames;

    LOG(DEBUG) << __func__ << mLogPrefix << " " << mMmapBufferDesc.toString()
               << " bufferSizeFrames " << *bufferSizeFrames;
    return fillDescriptor(desc);
}

int32_t StreamMmapBase::getLatencyMs() {
    return mPlatform.getLatencyMs(mMixPortConfig, mTag);
}

struct BufferConfig StreamMmapBase::getBufferConfig() {
    return mPlatform.getBufferConfig(mMixPortConfig, mTag);
}

::android::status_t StreamMmapBase::init() {
    LOG(DEBUG) << __func__ << mLogPrefix;
    return ::android::OK;
}

::android::status_t StreamMmapBase::drain(
        ::aidl::android::hardware::audio::core::StreamDescriptor::DrainMode) {
    LOG(DEBUG) << __func__ << mLogPrefix;
    return stopMMAP();
}

::android::status_t StreamMmapBase::flush() {
    LOG(DEBUG) << __func__ << mLogPrefix;
    return stopMMAP();
}

::android::status_t StreamMmapBase::pause() {
    LOG(DEBUG) << __func__ << mLogPrefix;
    return stopMMAP();
}

::android::status_t StreamMmapBase::standby() {
    LOG(DEBUG) << __func__ << mLogPrefix;
    if (mPalHandle != nullptr) {
        ::pal_stream_stop(mPalHandle);
        ::pal_stream_close(mPalHandle);
        mPalHandle = nullptr;
        mFramesAtSessionStart += roundUpToBufferBoundary(mFramesInSession, mBufferSizeInFrames);
        LOG(DEBUG) << __func__ << mLogPrefix << ": position rounded to " << mFramesAtSessionStart
                   << " for bufferCapacity " << mBufferSizeInFrames;
        mMmapBufferDesc.sharedMemory.fd.set(-1);  // reset shared mem fd
        mClosed = true;
    }
    return ::android::OK;
}

::android::status_t StreamMmapBase::start() {
    LOG(DEBUG) << __func__ << mLogPrefix;
    return ::android::OK;
}

::android::status_t StreamMmapBase::transfer(void* buffer, size_t frameCount,
                                             size_t* actualFrameCount, int32_t* latencyMs) {
    if (frameCount == 0) {
        *actualFrameCount = 0;
        return burstZero();
    }
    return ::android::OK;
}

::android::status_t StreamMmapBase::refinePosition(
        ::aidl::android::hardware::audio::core::StreamDescriptor::Reply* reply) {
    static const StreamDescriptor::Position kUnknownPosition = {
            .frames = StreamDescriptor::Position::UNKNOWN,
            .timeNs = StreamDescriptor::Position::UNKNOWN};

    if (!mPalHandle) {
        LOG(ERROR) << __func__ << mLogPrefix << ": pal stream handle is null";
        reply->observable = reply->hardware = kUnknownPosition;
        return 0;
    }
    if (!mIsStarted) {
        LOG(ERROR) << __func__ << mLogPrefix << ": stream not started, position unknown";
        reply->observable = reply->hardware = kUnknownPosition;
        return 0;
    }
    struct pal_mmap_position pal_mmap_pos;
    if (int32_t ret = pal_stream_get_mmap_position(mPalHandle, &pal_mmap_pos); ret) {
        LOG(ERROR) << __func__ << mLogPrefix << ": error from pal_stream_get_mmap_position";
        return ret;
    }

    reply->observable.timeNs = reply->hardware.timeNs = pal_mmap_pos.time_nanoseconds;

    mFramesInSession = pal_mmap_pos.position_frames;

    reply->hardware.frames = mFramesAtSessionStart + mFramesInSession;

    int64_t totalDelayFrames = 0;
    totalDelayFrames = getLatencyMs() * mMixPortConfig.sampleRate.value().value / 1000;
    reply->observable.frames = (reply->hardware.frames > totalDelayFrames)
                                       ? (reply->hardware.frames - totalDelayFrames)
                                       : 0;

    LOG(VERBOSE) << __func__ << mLogPrefix << ": hw_frames:" << reply->hardware.frames
                 << " obs_frames " << reply->observable.frames
                 << ", hw_timeNs:" << reply->hardware.timeNs << " mFramesAtSessionStart "
                 << mFramesAtSessionStart;
    return 0;
}

void StreamMmapBase::shutdown() {
    standby();
}

ndk::ScopedAStatus StreamMmapBase::setConnectedDevices(
        const std::vector<::aidl::android::media::audio::common::AudioDevice>& devices) {
    mWorker->setIsConnected(!devices.empty());
    mConnectedDevices = devices;

    return configureConnectedDevices_I();
}

ndk::ScopedAStatus StreamMmapBase::reconfigureConnectedDevices() {
    return configureConnectedDevices_I();
}

ndk::ScopedAStatus StreamMmapBase::configureConnectedDevices_I() {
    if (mConnectedDevices.empty()) {
        LOG(DEBUG) << __func__ << mLogPrefix << ": stream is not connected";
        return ndk::ScopedAStatus::ok();
    }

    if (!mPalHandle) {
        LOG(WARNING) << __func__ << mLogPrefix << ": stream is not configured";
        return ndk::ScopedAStatus::ok();
    }

    if (int32_t ret = mPlatform.setDevice(mPalHandle, mMixPortConfig, mTag, mConnectedDevices);
        ret) {
        LOG(ERROR) << __func__ << mLogPrefix << " failed to set devices on stream, ret:" << ret;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    LOG(DEBUG) << __func__ << mLogPrefix << " connected to " << mConnectedDevices;

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus StreamMmapBase::configureMMapStream(
        ::aidl::android::hardware::audio::core::MmapBufferDescriptor* desc,
        int32_t* bufferSizeFrames) {
    auto attr = mPlatform.getPalStreamAttributes(mMixPortConfig, mIsInput);
    if (!attr) {
        LOG(ERROR) << __func__ << mLogPrefix << " no pal attributes";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    attr->type = PAL_STREAM_ULTRA_LOW_LATENCY;
    attr->flags = static_cast<pal_stream_flags_t>(PAL_STREAM_FLAG_MMAP_NO_IRQ);
    auto palDevices = mPlatform.configureAndFetchPalDevices(mMixPortConfig, mTag, mConnectedDevices,
                                                            true /*dummyDevice*/);

    LOG(DEBUG) << __func__ << mLogPrefix << "pal_stream_open with " << toString(*attr.get());
    if (int32_t ret = ::pal_stream_open(
                attr.get(), palDevices.size(), palDevices.data(), 0, nullptr, nullptr /*cbfun*/,
                reinterpret_cast<uint64_t>(this) /*cookie*/, &(this->mPalHandle));
        ret) {
        LOG(ERROR) << __func__ << mLogPrefix
                   << " pal_stream_open failed, ret:" << std::to_string(ret);
        mPalHandle = nullptr;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    auto bufConfig = getBufferConfig();
    const size_t ringBufSizeInBytes = bufConfig.bufferSize;
    const size_t ringBufCount = bufConfig.bufferCount;

    auto palBufferConfig = mPlatform.getPalBufferConfig(ringBufSizeInBytes, ringBufCount);
    pal_buffer_config* inBufferConfig = mIsInput ? palBufferConfig.get() : nullptr;
    pal_buffer_config* outBufferConfig = mIsInput ? nullptr : palBufferConfig.get();

    LOG(DEBUG) << __func__ << mLogPrefix << " set pal_stream_set_buffer_size to "
               << std::to_string(ringBufSizeInBytes) << " with count "
               << std::to_string(ringBufCount);
    if (int32_t ret =
                ::pal_stream_set_buffer_size(this->mPalHandle, inBufferConfig, outBufferConfig);
        ret) {
        LOG(ERROR) << __func__ << mLogPrefix
                   << " pal_stream_set_buffer_size failed, ret:" << std::to_string(ret);
        ::pal_stream_close(mPalHandle);
        mPalHandle = nullptr;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    const auto frameSize =
            getFrameSizeInBytes(mMixPortConfig.format.value(), mMixPortConfig.channelMask.value());
    auto ret = createMMapBuffer(frameSize, desc, bufferSizeFrames);
    if (!ret.isOk()) {
        LOG(ERROR) << __func__ << mLogPrefix << " createMMapBuffer failed";
        ::pal_stream_close(mPalHandle);
        mPalHandle = nullptr;
        return ret;
    }

    LOG(INFO) << __func__ << mLogPrefix << ": stream is configured with " << mConnectedDevices;

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus StreamMmapBase::createMmapBuffer(
        ::aidl::android::hardware::audio::core::MmapBufferDescriptor* desc) {
    return createOrGetMmapBuffer(desc);
}

::android::status_t StreamMmapBase::burstZero() {
    return startMMAP();
}

::android::status_t StreamMmapBase::startMMAP() {
    if (!mPalHandle) {
        LOG(ERROR) << __func__ << mLogPrefix << ": pal stream handle is null";
        return -EINVAL;
    }

    if (mIsStarted) {
        LOG(VERBOSE) << __func__ << mLogPrefix << ": MMAP already started";
        return 0;
    }

    if (int32_t ret = ::pal_stream_start(mPalHandle); ret) {
        LOG(ERROR) << __func__ << mLogPrefix << " pal stream start failed, ret:" << ret;
        return ret;
    }

    mIsStarted = true;
    mClosed = false;
    LOG(VERBOSE) << __func__ << mLogPrefix << ": MMAP start success";

    return 0;
}

::android::status_t StreamMmapBase::stopMMAP() {
    if (!mPalHandle) {
        LOG(ERROR) << __func__ << mLogPrefix << ": pal stream handle is null";
        return -EINVAL;
    }

    if (!mIsStarted) {
        LOG(VERBOSE) << __func__ << mLogPrefix << ": MMAP already stopped";
        return 0;
    }

    if (int32_t ret = ::pal_stream_stop(mPalHandle); ret) {
        LOG(ERROR) << __func__ << mLogPrefix << " pal stream stop failed, ret:" << ret;
        return -EINVAL;
    }

    mIsStarted = false;
    LOG(DEBUG) << __func__ << mLogPrefix << ": MMAP stop success";

    return 0;
}

StreamInMmap::StreamInMmap(StreamContext&& context, const SinkMetadata& sinkMetadata,
                           const std::vector<MicrophoneInfo>& microphones)
    : StreamIn(std::move(context), microphones),
      StreamMmapBase(&(StreamIn::mContext), sinkMetadata, true /*input*/) {}

ndk::ScopedAStatus StreamInMmap::configureMMapStream(
        ::aidl::android::hardware::audio::core::MmapBufferDescriptor* desc,
        int32_t* bufferSizeFrames) {
    auto status = StreamMmapBase::configureMMapStream(desc, bufferSizeFrames);
    setStreamMicMute(mPlatform.getMicMuteStatus());
    LOG(INFO) << __func__ << mLogPrefix << ": stream is configured with " << mConnectedDevices;

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus StreamInMmap::getActiveMicrophones(
        std::vector<MicrophoneDynamicInfo>* _aidl_return) {
    *_aidl_return = mPlatform.getMicrophoneDynamicInfo(mConnectedDevices);
    LOG(VERBOSE) << __func__ << mLogPrefix << " " << ::android::internal::ToString(*_aidl_return);
    return ndk::ScopedAStatus::ok();
}

void StreamInMmap::setStreamMicMute(const bool muted) {
    if (mPalHandle == nullptr) {
        return;
    }
    if (!mPlatform.setStreamMicMute(mPalHandle, muted)) {
        LOG(ERROR) << __func__ << mLogPrefix << " failed";
    }
}

StreamOutMmap::StreamOutMmap(StreamContext&& context, const SourceMetadata& sourceMetadata,
                             const std::optional<AudioOffloadInfo>& offloadInfo)
    : StreamOut(std::move(context), offloadInfo),
      StreamMmapBase(&(StreamOut::mContext), sourceMetadata, false /*input*/) {
    mVolumes.resize(getChannelCount(mMixPortConfig.channelMask.value()));
}

ndk::ScopedAStatus StreamOutMmap::configureMMapStream(
        ::aidl::android::hardware::audio::core::MmapBufferDescriptor* desc,
        int32_t* bufferSizeFrames) {
    setBluetoothMetadata();
    auto status = StreamMmapBase::configureMMapStream(desc, bufferSizeFrames);
    setHwVolume(mVolumes);
    LOG(INFO) << __func__ << mLogPrefix << ": stream is configured with " << mConnectedDevices;

    return ndk::ScopedAStatus::ok();
}

// check StreamOutPrimary for origin of the change
void StreamOutMmap::setBluetoothMetadata() {
    if (hasBluetoothLEDevice(mConnectedDevices)) {
        playback_track_metadata_t track = {.usage = AUDIO_USAGE_GAME,
                                           .content_type = AUDIO_CONTENT_TYPE_MUSIC};

        source_metadata_t btSourceMetadata = {.track_count = 1, .tracks = &track};
        pal_set_param(PAL_PARAM_ID_SET_SOURCE_METADATA, (void*)&btSourceMetadata, 0);
    }
}

ndk::ScopedAStatus StreamOutMmap::getHwVolume(std::vector<float>* _aidl_return) {
    *_aidl_return = mVolumes;
    LOG(VERBOSE) << __func__ << mLogPrefix << ::android::internal::ToString(mVolumes);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus StreamOutMmap::setHwVolume(const std::vector<float>& in_channelVolumes) {
    if (mVolumes.size() != in_channelVolumes.size()) {
        LOG(ERROR) << __func__ << mLogPrefix << " channel count mismatch with port, expected "
                   << mVolumes.size() << " got " << in_channelVolumes.size();
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    auto isVolumeInRange = [](const std::vector<float>& volumes) {
        return std::all_of(volumes.begin(), volumes.end(),
                           [](float vol) { return (vol >= 0.0f && vol <= 1.0f); });
    };

    if (!isVolumeInRange(in_channelVolumes)) {
        LOG(ERROR) << __func__ << mLogPrefix << " out of range volume "
                   << ::android::internal::ToString(in_channelVolumes);
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    if (!mPalHandle) {
        mVolumes = in_channelVolumes;
        LOG(DEBUG) << __func__ << mLogPrefix << " cache volume "
                   << ::android::internal::ToString(in_channelVolumes);
        return ndk::ScopedAStatus::ok();
    }

    if (int32_t ret = mPlatform.setVolume(mPalHandle, in_channelVolumes); ret) {
        LOG(ERROR) << __func__ << mLogPrefix << " failed to set volume";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    mVolumes = in_channelVolumes;

    LOG(DEBUG) << __func__ << mLogPrefix << ::android::internal::ToString(mVolumes);
    return ndk::ScopedAStatus::ok();
}

}  // namespace qti::audio::core
