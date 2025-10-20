/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once

#include <qti-audio-core/AudioUsecase.h>
#include <qti-audio-core/HalOffloadEffects.h>
#include <qti-audio-core/Stream.h>
#include <qti-audio-core/PlatformStreamCallback.h>
#include <qti-audio-core/StreamOutPrimary.h>
#ifdef ECNR_HAL_SRC_CP
#include <audio_utils/resampler.h>
#include "extensions/hal_ringbuffer.h"
#endif

using ::aidl::android::media::audio::common::AudioDevice;

namespace qti::audio::core {

#define INVALID -1

class StreamOutPrimaryOEM : public StreamOutPrimary {
  public:
    friend class ndk::SharedRefBase;
    StreamOutPrimaryOEM(StreamContext&& context,
                     const ::aidl::android::hardware::audio::common::SourceMetadata& sourceMetadata,
                     const std::optional<::aidl::android::media::audio::common::AudioOffloadInfo>&
                             offloadInfo,std::vector<AudioDevice> AudioDevices);

    virtual ~StreamOutPrimaryOEM() override;
    ::android::status_t standby() override;
    ::android::status_t transfer(void* buffer, size_t frameCount, size_t* actualFrameCount,
                               int32_t* latencyMs) override;
    void shutdown() override;
  protected:
    /*
     * opens, configures and starts pal stream, also validates the pal handle.
     */
    void configure();
    void shutdown_I();
    ::android::status_t onWriteError(const size_t sleepFrameCount);

  private:
    struct BufferConfig getBufferConfigOEM();
    std::string mLogPrefixOEM = "";
    bool bECNRprop_Enable{false};
    bool bECNR_Enable{false};
    std::unique_ptr<uint8_t[]> ecnr_out_buffer{nullptr};
    size_t ecnr_out_buffer_size{0};
    std::unique_ptr<uint8_t[]> ecnr_in_buffer{nullptr};
    size_t ecnr_in_buffer_size{0};
    std::unique_ptr<pal_stream_attributes> attr;
#ifdef ECNR_HAL_SRC_CP
    std::unique_ptr<uint8_t[]> ecnr_src_buffer{nullptr};
    size_t ecnr_src_buffer_size{0};
    std::optional<std::unique_ptr<HalRingBuffer<int16_t>>> mOutHalRingBuffer;
    struct resampler_itfe *resampler{NULL};
#endif
    tECNR_ProcessData pECNR_ProcessData;
    int mChannels{0};
    int ecnrPeriodSize{512};
    int ecnrSampleRate{48000};
    size_t mECNRFrameSizeBytes{0};
    int mScd_file_path_index_dl;
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
