/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once

#include <qti-audio-core/AudioUsecase.h>
#include <qti-audio-core/Stream.h>
#include <system/audio_effects/effect_uuid.h>
#include <qti-audio-core/StreamInPrimary.h>

namespace qti::audio::core {

#define INVALID -1

class StreamInPrimaryOEM : public StreamInPrimary{
  public:

    StreamInPrimaryOEM(
            StreamContext&& context,
            const ::aidl::android::hardware::audio::common::SinkMetadata& sinkMetadata,
            const std::vector<::aidl::android::media::audio::common::MicrophoneInfo>& microphones);

    virtual ~StreamInPrimaryOEM() override;
    ::android::status_t pause() override;
    ::android::status_t standby() override;
    ::android::status_t transfer(void* buffer, size_t frameCount, size_t* actualFrameCount,
                                 int32_t* latencyMs) override;
    void shutdown() override;
/*
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
*/
  protected:
    /*
     * This API opens, configures and starts pal stream.
     * also responsible for validity of pal handle.
     */
    void configure();
    void shutdown_I();
    AudioExtension& mAudExt{AudioExtension::getInstance()};
  private:
    ::android::status_t onReadErrorOEM(const size_t sleepFrameCount);
    struct BufferConfig getBufferConfigOEM();
    std::string mLogPrefixOEM = "";
    bool bECNRprop_Enable{false};
    bool bECNR_Enable{false};
    std::unique_ptr<uint8_t[]> ecnr_out_buffer{nullptr};
    size_t ecnr_out_buffer_size{0};
    std::unique_ptr<uint8_t[]> ecnr_in_buffer{nullptr};
    size_t ecnr_in_buffer_size{0};
    std::unique_ptr<uint8_t[]> ecnr_ecmx_buffer{nullptr};
    size_t ecnr_ecmx_buffer_size{0};
    tECNR_ProcessData pECNR_ProcessData;
    int mChannels{0};
    int ecnrPeriodSize{512};
    int ecnrSampleRate{48000};
    size_t mECNRFrameSizeBytes{0};
#ifdef ECNR_HAL_TUNE
    tECNR_TuneIFData pECNR_TuneIFData;
#endif
#ifdef PCM_DUMP_HAL_ENABLE
    FILE * fp_in_dump = NULL;
    FILE * fp_out_dump = NULL;
    bool pcm_dump = false;
#endif
};

} // namespace qti::audio::core
