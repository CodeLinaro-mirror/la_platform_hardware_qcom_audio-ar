/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once

#include <aidl/android/hardware/audio/core/MmapBufferDescriptor.h>
#include <qti-audio-core/AudioUsecase.h>
#include <qti-audio-core/Stream.h>

namespace qti::audio::core {

class StreamMmapBase : public StreamCommonImpl {
  public:
    /*
     * Applicable for <=3 HAL core interface version.
     * If Exposed, FWK can query for createMmapBuffer at anytime, to get mmap buffer.
     * Without this mmap buffer will be only created open[Output/Input]Stream.
     *
     */
    inline const static std::string kAospCreateMmapBuffer{"aosp.createMmapBuffer"};

    StreamMmapBase(StreamContext* context, const Metadata& metadata, const bool input);
    virtual ~StreamMmapBase();

    // Methods of 'DriverInterface'.
    ::android::status_t init() override;
    ::android::status_t drain(
            ::aidl::android::hardware::audio::core::StreamDescriptor::DrainMode) override;
    ::android::status_t flush() override;
    ::android::status_t pause() override;
    ::android::status_t standby() override;
    ::android::status_t start() override;
    ::android::status_t transfer(void* buffer, size_t frameCount, size_t* actualFrameCount,
                                 int32_t* latencyMs) override;
    ::android::status_t refinePosition(
            ::aidl::android::hardware::audio::core::StreamDescriptor::Reply*
            /*reply*/) override;
    void shutdown() override;

    // methods of StreamCommonInterface
    ndk::ScopedAStatus getVendorParameters(
            const std::vector<std::string>& in_ids,
            std::vector<::aidl::android::hardware::audio::core::VendorParameter>* _aidl_return)
            override;
    ndk::ScopedAStatus setVendorParameters(
            const std::vector<::aidl::android::hardware::audio::core::VendorParameter>&
                    in_parameters,
            bool in_async) override;

    ndk::ScopedAStatus setConnectedDevices(
            const std::vector<::aidl::android::media::audio::common::AudioDevice>& devices)
            override;
    ndk::ScopedAStatus reconfigureConnectedDevices() override;
    ndk::ScopedAStatus configureMMapStream(
            ::aidl::android::hardware::audio::core::MmapBufferDescriptor* desc,
            int32_t* bufferSizeFrames) override;

  protected:
    int32_t getLatencyMs();
    struct BufferConfig getBufferConfig();
    ndk::ScopedAStatus createMmapBuffer(
            ::aidl::android::hardware::audio::core::MmapBufferDescriptor* desc);
    virtual ndk::ScopedAStatus createMMapBuffer(
            int64_t frameSize, ::aidl::android::hardware::audio::core::MmapBufferDescriptor* desc,
            int32_t* bufferSizeFrames);

    Platform& mPlatform{Platform::getInstance()};

    ndk::ScopedAStatus fillDescriptor(
            ::aidl::android::hardware::audio::core::MmapBufferDescriptor* desc);
    ndk::ScopedAStatus getCachedMmapBuffer(
            ::aidl::android::hardware::audio::core::MmapBufferDescriptor* desc);

    ndk::ScopedAStatus createOrGetMmapBuffer(
            ::aidl::android::hardware::audio::core::MmapBufferDescriptor* desc);

    bool isValid();

    // API which are *_I are internal
    ndk::ScopedAStatus configureConnectedDevices_I();

    /* burst zero indicates that burst command with zero bytes issued from framework */
    ::android::status_t burstZero();
    ::android::status_t startMMAP();
    ::android::status_t stopMMAP();

    bool mIsInput;
    const Usecase mTag;
    const std::string mTagName;
    const ::aidl::android::media::audio::common::AudioPortConfig& mMixPortConfig;
    std::string mLogPrefix = "";

    bool mIsStarted = false;
    bool mClosed = true;  // assocaited with pal_stream_close, start moves to closed -> false,
    int64_t mFramesInSession = 0;
    // Total frames processed before the current session started. Rounded up to the
    // nearest buffer boundary since standby -> restart, creates a new shared memory buffer and
    // the DSP position resets to zero.
    int64_t mFramesAtSessionStart = 0;
    int32_t mBufferSizeInFrames = 0;
    ::aidl::android::hardware::audio::core::MmapBufferDescriptor mMmapBufferDesc;

    // All the public must check the validity of this resource, if using
    pal_stream_handle_t* mPalHandle{nullptr};
};

class StreamInMmap final : public StreamIn, public StreamMmapBase {
  public:
    friend class ndk::SharedRefBase;
    StreamInMmap(
            StreamContext&& context,
            const ::aidl::android::hardware::audio::common::SinkMetadata& sinkMetadata,
            const std::vector<::aidl::android::media::audio::common::MicrophoneInfo>& microphones);

    ndk::ScopedAStatus getActiveMicrophones(
            std::vector<::aidl::android::media::audio::common::MicrophoneDynamicInfo>* _aidl_return)
            override;

    ndk::ScopedAStatus configureMMapStream(
            ::aidl::android::hardware::audio::core::MmapBufferDescriptor* desc,
            int32_t* bufferSizeFrames) override;
    ndk::ScopedAStatus updateMetadataCommon(const Metadata& metadata) override;
    int32_t setAggregateSinkMetadata(bool voiceActive) override;

    void setStreamMicMute(const bool muted) override;

  private:
    void onClose() override { defaultOnClose(); }
};

class StreamOutMmap final : public StreamOut, public StreamMmapBase {
  public:
    friend class ndk::SharedRefBase;
    StreamOutMmap(StreamContext&& context,
                  const ::aidl::android::hardware::audio::common::SourceMetadata& sourceMetadata,
                  const std::optional<::aidl::android::media::audio::common::AudioOffloadInfo>&
                          offloadInfo);

    ndk::ScopedAStatus configureMMapStream(
            ::aidl::android::hardware::audio::core::MmapBufferDescriptor* desc,
            int32_t* bufferSizeFrames) override;
    ndk::ScopedAStatus getHwVolume(std::vector<float>* _aidl_return) override;
    ndk::ScopedAStatus setHwVolume(const std::vector<float>& in_channelVolumes) override;
    ndk::ScopedAStatus updateMetadataCommon(const Metadata& metadata) override;
    int32_t setAggregateSourceMetadata(bool voiceActive) override;

  protected:
    bool supportsPlaybackRate() const override;
    ndk::ScopedAStatus setPlaybackRateImpl(
            const ::aidl::android::media::audio::common::AudioPlaybackRate& rate) override;

  private:
    std::vector<float> mVolumes{};
    void onClose() override { defaultOnClose(); }
    void setBluetoothMetadata();
};

}  // namespace qti::audio::core
