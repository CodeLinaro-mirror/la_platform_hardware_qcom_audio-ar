/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once

#include <memory>
#include <qti-audio-core/AudioUsecase.h>
#include <qti-audio-core/HalOffloadEffects.h>
#include <qti-audio-core/Stream.h>
#include <qti-audio-core/PlatformStreamCallback.h>
#include <android-base/logging.h>

#ifdef ENABLE_QCOM_HAL_AUDIO_FOCUS
#include <extensions/AudioHalFocusManager.h>
#endif
#define LOW_LATENCY_PLATFORM_DELAY (13*1000LL)
#define DEEP_BUFFER_PLATFORM_DELAY (70*1000LL)
#define PCM_OFFLOAD_PLATFORM_DELAY (30*1000LL)
#define MMAP_PLATFORM_DELAY        (3*1000LL)
#define ULL_PLATFORM_DELAY         (4*1000LL)

using ::aidl::android::media::audio::common::AudioDevice;

namespace qti::audio::core {

class StreamOutPrimary : public StreamOut, public StreamCommonImpl, public PlatformStreamCallback {
  public:
    friend class ndk::SharedRefBase;
    StreamOutPrimary(StreamContext&& context,
                     const ::aidl::android::hardware::audio::common::SourceMetadata& sourceMetadata,
                     const std::optional<::aidl::android::media::audio::common::AudioOffloadInfo>&
                             offloadInfo,
                             std::vector<AudioDevice> AudioDevices);

    virtual ~StreamOutPrimary() override;
    int32_t setAggregateSourceMetadata(bool voiceActive) override;
    std::string getAddress() const;

    // Methods of 'DriverInterface'.
    ::android::status_t init() override;
    ::android::status_t drain(
            ::aidl::android::hardware::audio::core::StreamDescriptor::DrainMode) override;
    ::android::status_t flush() override; /* seek = pause + resume */
    ::android::status_t pause() override;
    ::android::status_t standby() override;
    ::android::status_t start() override;
    ::android::status_t transfer(void* buffer, size_t frameCount, size_t* actualFrameCount,
                                 int32_t* latencyMs) override;
    ::android::status_t refinePosition(
            ::aidl::android::hardware::audio::core::StreamDescriptor::Reply*
            /*reply*/) override;
    void shutdown() override;
    ::android::status_t getHwTimeStamp(::aidl::android::hardware::audio::core::StreamDescriptor::Reply*);

    // methods of StreamCommonInterface

    ndk::ScopedAStatus getVendorParameters(
            const std::vector<std::string>& in_ids,
            std::vector<::aidl::android::hardware::audio::core::VendorParameter>* _aidl_return)
            override;
    ndk::ScopedAStatus setVendorParameters(
            const std::vector<::aidl::android::hardware::audio::core::VendorParameter>&
                    in_parameters,
            bool in_async) override;
    ndk::ScopedAStatus addEffect(
            const std::shared_ptr<::aidl::android::hardware::audio::effect::IEffect>& in_effect)
            override;
    ndk::ScopedAStatus removeEffect(
            const std::shared_ptr<::aidl::android::hardware::audio::effect::IEffect>& in_effect)
            override;

    ndk::ScopedAStatus updateMetadataCommon(const Metadata& metadata) override;

    // Methods of IStreamOut
    ndk::ScopedAStatus updateOffloadMetadata(
            const ::aidl::android::hardware::audio::common::AudioOffloadMetadata&
                    in_offloadMetadata) override;

    ndk::ScopedAStatus getHwVolume(std::vector<float>* _aidl_return) override;
    ndk::ScopedAStatus setHwVolume(const std::vector<float>& in_channelVolumes) override;
#ifdef ENABLE_QCOM_AMPERE_AUDIO
    ndk::ScopedAStatus setStreamVolume(const std::vector<float>& in_channelVolumes);
    ndk::ScopedAStatus getStreamVolume(std::vector<float>* _aidl_return);
#endif
    ndk::ScopedAStatus getPlaybackRateParameters(
            ::aidl::android::media::audio::common::AudioPlaybackRate* _aidl_return) override;
    ndk::ScopedAStatus setPlaybackRateParameters(
            const ::aidl::android::media::audio::common::AudioPlaybackRate& in_playbackRate)
            override;
    // Methods called IModule
    ndk::ScopedAStatus setConnectedDevices(
            const std::vector<::aidl::android::media::audio::common::AudioDevice>& devices)
            override;
    ndk::ScopedAStatus reconfigureConnectedDevices() override;
    ndk::ScopedAStatus configureMMapStream(int32_t* fd, int64_t* burstSizeFrames, int32_t* flags,
                                           int32_t* bufferSizeFrames) override;

    void onClose() override { defaultOnClose(); }

    ndk::ScopedAStatus setLatencyMode(
                           ::aidl::android::media::audio::common::AudioLatencyMode in_mode) override;
    ndk::ScopedAStatus getRecommendedLatencyModes(
        std::vector<::aidl::android::media::audio::common::AudioLatencyMode>* _aidl_return) override;

    bool isStreamOutPrimary() { return (mTag == Usecase::PRIMARY_PLAYBACK) ? true : false; }
    bool isStreamOutMedia() { return (mTag == Usecase::MEDIA_PLAYBACK) ? true : false; }
    static std::mutex sourceMetadata_mutex_;

    // Methods from PlatformStreamCallback
    void onTransferReady() override;
    void onDrainReady() override;
    void onError() override;
    uint64_t mBytesWritten; /* total bytes written, not cleared when entering standby */
#ifdef ENABLE_QCOM_HAL_AUDIO_FOCUS
    void requestFocus();
    void abandonFocus();
    FocusSession focusSessionInfo;
#endif
  protected:
    /*
     * opens, configures and starts pal stream, also validates the pal handle.
     */
    void configure();
    void resume();
    void shutdown_I();
    /* burst zero indicates that burst command with zero bytes issued from framework */
    ::android::status_t burstZero();
    ::android::status_t startMMAP();
    ::android::status_t stopMMAP();
    size_t getPlatformDelay() const noexcept;
    ::android::status_t onWriteError(const size_t sleepFrameCount);

    // This API calls startEffect/stopEffect only on offload/pcm offload outputs.
    void enableOffloadEffects(const bool enable);
    int64_t GetRenderLatency(std::string address);

    // API which are *_I are internal
    ndk::ScopedAStatus configureConnectedDevices_I();

    const Usecase mTag;
    const std::string mTagName;
    const size_t mFrameSizeBytes;
    bool mIsPaused{false};
    //0.6 is equivalyent to -3600 which is default value for RNCDC
    std::vector<float> mVolumes{0.6,0.6};
    bool mUseCachedVolume = false;
    bool mHwVolumeSupported = false;
    bool mHwFlushSupported = false;
    bool mHwPauseSupported = false;
    // check validaty of mPalHandle before use
    pal_stream_handle_t* mPalHandle{nullptr};
    pal_stream_handle_t* mHapticsPalHandle{nullptr};
    static constexpr ::aidl::android::media::audio::common::AudioPlaybackRate sDefaultPlaybackRate =
            {.speed = 1.0f,
             .pitch = 1.0f,
             .fallbackMode = ::aidl::android::media::audio::common::AudioPlaybackRate::
                     TimestretchFallbackMode::FAIL};

    ::aidl::android::media::audio::common::AudioPlaybackRate mPlaybackRate;

    //Haptics Usecase
    struct pal_stream_attributes mHapticsStreamAttributes;
    struct pal_device mHapticsDevice;
    std::unique_ptr<uint8_t[]> mHapticsBuffer{nullptr};
    size_t mHapticsBufSize{0};
    ::android::status_t convertBufferAndWrite(const void* buffer, size_t frameCount);
    // This API splits and writes audio and haptics streams
    ::android::status_t hapticsWrite(const void *buffer, size_t frameCount);

    std::variant<std::monostate, PrimaryPlayback, DeepBufferPlayback, CompressPlayback,
                 PcmOffloadPlayback, VoipPlayback, SpatialPlayback, MMapPlayback, UllPlayback,
                 InCallMusic, HapticsPlayback,SysNotificationPlayback, NavGuidancePlayback,
                 PhonePlayback, AlertPlayback, NavGuidance2Playback, MediaPlayback,LowLatencyPlayback>
            mExt;
    // references
    Platform& mPlatform{Platform::getInstance()};
    const ::aidl::android::media::audio::common::AudioPortConfig& mMixPortConfig;
    HalOffloadEffects& mHalEffects{HalOffloadEffects::getInstance()};
    AudioExtension& mAudExt{AudioExtension::getInstance()};
    int64_t GetSourceLatency();

  private:
    std::string mLogPrefix = "";
    bool isHwVolumeSupported();
    bool isHwFlushSupported();
    bool isHwPauseSupported();
    struct BufferConfig getBufferConfig();
    std::string busAddr = "";
    std::mutex mPalHandleMutex;   /* mutex for palhandle */

    // optional buffer format converter, if stream input and output formats are different
    std::optional<std::unique_ptr<BufferFormatConverter>> mBufferFormatConverter;

    std::map<std::string, float> sourceGainTable = {
        {"DEFAULT", 0},
        {"FM", 900},
        {"DAB", 100},
        {"AM", 0}
    };
};


} // namespace qti::audio::core
