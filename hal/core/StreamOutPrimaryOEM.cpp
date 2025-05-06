/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <cmath>

#define LOG_TAG "AHAL_StreamOutOEM_QTI"

#include <android-base/logging.h>
#include <audio_utils/clock.h>
#include <hardware/audio.h>
#include <qti-audio-core/Module.h>
#include <qti-audio-core/ModulePrimary.h>
#include <qti-audio-core/StreamOutPrimaryOEM.h>
#include <qti-audio/PlatformConverter.h>
#include <qti-audio-core/Parameters.h>

using aidl::android::hardware::audio::common::AudioOffloadMetadata;
using aidl::android::hardware::audio::common::getFrameSizeInBytes;
using aidl::android::hardware::audio::common::SinkMetadata;
using aidl::android::hardware::audio::common::SourceMetadata;
using aidl::android::media::audio::common::AudioDevice;
using aidl::android::media::audio::common::AudioDeviceAddress;
using aidl::android::media::audio::common::AudioDualMonoMode;
using aidl::android::media::audio::common::AudioLatencyMode;
using aidl::android::media::audio::common::AudioOffloadInfo;
using aidl::android::media::audio::common::AudioPlaybackRate;
using aidl::android::media::audio::common::MicrophoneDynamicInfo;
using aidl::android::media::audio::common::MicrophoneInfo;
using aidl::android::media::audio::common::AudioLatencyMode;
using aidl::android::media::audio::common::AudioChannelLayout;
using ::aidl::android::hardware::audio::common::getChannelCount;

using ::aidl::android::hardware::audio::common::getFrameSizeInBytes;
using ::aidl::android::hardware::audio::common::getPcmSampleSizeInBytes;
using ::aidl::android::hardware::audio::core::IStreamCallback;
using ::aidl::android::hardware::audio::core::IStreamCommon;
using ::aidl::android::hardware::audio::core::StreamDescriptor;
using ::aidl::android::hardware::audio::core::VendorParameter;

using aidl::android::media::audio::common::AudioPortExt;

// uncomment this to enable logging of very verbose logs like burst commands.
//#define VERY_VERBOSE_LOGGING 1


namespace qti::audio::core {

int VoipPlaybackECNR::kSampleRate = 48000;

StreamOutPrimaryOEM::StreamOutPrimaryOEM(StreamContext&& context, const SourceMetadata& sourceMetadata,
                                   const std::optional<AudioOffloadInfo>& offloadInfo)
    : StreamOutPrimary(std::move(context),sourceMetadata,offloadInfo) {

    std::ostringstream os;
    os << " : usecase: " << mTagName;
    os << " IoHandle: " << mMixPortConfig.ext.get<AudioPortExt::Tag::mix>().handle << " ";
    mLogPrefixOEM = os.str();
    LOG(INFO) << __func__ << mLogPrefixOEM;
}

StreamOutPrimaryOEM::~StreamOutPrimaryOEM() {
    shutdown_I();
    LOG(DEBUG) << __func__ << mLogPrefixOEM;
}

::android::status_t StreamOutPrimaryOEM::standby() {
    if (!mPalHandle) {
        LOG(WARNING) << __func__ << mLogPrefixOEM << ": stream is not configured ";
        return ::android::OK;
    }

    if (mTag == Usecase::MMAP_PLAYBACK) {
        return ::android::OK;
    }

    shutdown_I();
    LOG(DEBUG) << __func__ << mLogPrefixOEM;
    return ::android::OK;
}
void StreamOutPrimaryOEM::shutdown() {
    shutdown_I();

    if (hasOutputVoipRxFlag(mMixPortConfig.flags.value())) {
        if (auto telephony = mContext.getTelephony().lock()) {
            telephony->onVoipPlaybackClose();
        }
    }
}
::android::status_t StreamOutPrimaryOEM::onWriteError(const size_t sleepFrameCount) {
    shutdown_I();
    if (mTag == Usecase::COMPRESS_OFFLOAD_PLAYBACK) {
        // return error for offload, so that FW sends data again
        LOG(ERROR) << __func__ << mLogPrefixOEM << ": cannot afford write failure";
        return ::android::DEAD_OBJECT;
    }
    auto& sampleRate = mMixPortConfig.sampleRate.value().value;
    if (sampleRate == 0) {
        LOG(ERROR) << __func__ << mLogPrefixOEM << ": cannot afford write failure, sampleRate is zero";
        return ::android::UNEXPECTED_NULL;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds((sleepFrameCount * 1000) / sampleRate));
    LOG(WARNING) << __func__ << mLogPrefixOEM << ": ignoring this write";
    return ::android::OK;
}
::android::status_t StreamOutPrimaryOEM::transfer(void* buffer, size_t frameCount,
                                               size_t* actualFrameCount, int32_t* latencyMs) {

    int ret = 0 ;
    if (!mPalHandle) {
        // configure on first transfer or after stand by
        configure();
        if (!mPalHandle) {
            LOG(ERROR) << __func__ << mLogPrefixOEM << ": failed to configure";
            *actualFrameCount = frameCount;
            return     onWriteError(frameCount);
        }
    }

    if (!bECNR_Enable) {
        return StreamOutPrimary::transfer(buffer, frameCount, actualFrameCount, latencyMs);
    } else {
        pal_buffer palBuffer{};
        palBuffer.buffer = static_cast<uint8_t*>(buffer);
        palBuffer.size = frameCount * mFrameSizeBytes;
        if (palBuffer.size == 0) {
            // resume comes with 0 frameCount
            return ::android::OK;
        }
        ssize_t bytesWritten;
#ifdef ECNR_HAL_SRC_CP
        uint32_t rbWritten = 0;
        uint32_t rbRead = 0;
        int processItr = 1;
#endif
        int16_t *deint_in_buffer = reinterpret_cast<int16_t*>(ecnr_in_buffer.get());
        int16_t *deint_out_buffer = reinterpret_cast<int16_t*>(ecnr_out_buffer.get());
#ifdef ECNR_HAL_SRC_CP
        int16_t *rsp_in_buffer = reinterpret_cast<int16_t*>(buffer);
        if (resampler != NULL && ecnr_src_buffer && ecnr_src_buffer_size > 0) {
            palBuffer.buffer = reinterpret_cast<uint8_t*>(ecnr_src_buffer.get());
            palBuffer.size = std::min(ecnr_out_buffer_size, ecnr_src_buffer_size);
        }
#endif
        int16_t *src_buffer = reinterpret_cast<int16_t*>(palBuffer.buffer);
        int16_t *dst_buffer = reinterpret_cast<int16_t*>(palBuffer.buffer);
#ifdef ECNR_HAL_TUNE
        int getTuneIOBuffer = 0;
#endif
#ifdef PCM_DUMP_HAL_ENABLE
        if (pcm_dump) {
            if (fp_in_dump) {
                fwrite(buffer,sizeof(char), frameCount * mFrameSizeBytes ,fp_in_dump);
            }
        }
#endif
#ifdef ECNR_HAL_SRC_CP
        if ((resampler != NULL) && (rsp_in_buffer != src_buffer)) {
            size_t inFrameCount = (size_t)frameCount;
            size_t outFrameCount = (size_t)(frameCount*ecnrSampleRate/mMixPortConfig.sampleRate.value().value);
            resampler->resample_from_input(resampler, rsp_in_buffer,
                                                 &inFrameCount, src_buffer,
                                                 &outFrameCount);
//            LOG(DEBUG) << __func__ << mLogPrefixOEM << " inFrameCount " << inFrameCount << " frameCount " << frameCount;
//            LOG(DEBUG) << __func__ << mLogPrefixOEM << " outFrameCount " << outFrameCount << " ecnrPeriodSize " << ecnrPeriodSize;
            rbWritten = mOutHalRingBuffer->get()->write(src_buffer, outFrameCount*mChannels);
            if (outFrameCount*mChannels != rbWritten ) {
               LOG(DEBUG) << __func__ << mLogPrefixOEM << " Dropping samples : " << (rbWritten - outFrameCount*mChannels );
            }
            processItr = mOutHalRingBuffer->get()->avaiableWrittenBufferSize()/(ecnrPeriodSize*mChannels);
        } else {
            processItr = 1;
        }

        while (processItr > 0) {
            processItr--;
            if ((resampler != NULL) && (rsp_in_buffer != src_buffer)) {
                rbRead = mOutHalRingBuffer->get()->read(src_buffer, ecnrPeriodSize*mChannels);
                if (rbRead == 0) {
                    LOG(DEBUG) << __func__ << mLogPrefixOEM << " couldn't get data from ringbuffer";
                    goto skip_write;
                }
            }
#endif
            if (pECNR_ProcessData.audioIO.RcvInBufferCnt== 1) {
//                    LOG(DEBUG) << __func__ << "channel size 1, just copy";
                memcpy(deint_in_buffer, src_buffer, ecnr_in_buffer_size);
            } else if (pECNR_ProcessData.audioIO.RcvInBufferCnt > 1) {
//                    LOG(DEBUG) << __func__ << " change format";
                mAudExt.mHalExtension->audio_extn_cvtformat16_lnterleave_to_deinterleave(src_buffer,deint_in_buffer,ecnrPeriodSize,pECNR_ProcessData.audioIO.RcvInBufferCnt);
            }
#ifdef ECNR_HAL_DUMP_ENABLE
            if (property_get_bool("vendor.audio.feature.ecnr.dump", false)) {
//                LOG(DEBUG) << __func__ << "DUMP enabled for ecnr deinterleaved data of ecnr input";
                char dump_file[128];
                FILE *fp_pcm_dump = NULL;
                for (unsigned int i =0 ; i < pECNR_ProcessData.audioIO.RcvInBufferCnt; i++) {
                    snprintf(dump_file, sizeof(dump_file), "%s/%s_ecnr_test_deinterleave_%d_in.raw",AUDIO_HAL_DUMP_PATH,mTagName.c_str(),i);
                    fp_pcm_dump=fopen(dump_file,"a+");
                    if (fp_pcm_dump) {
                        fwrite(deint_in_buffer + i*ecnrPeriodSize,sizeof(char),ecnrPeriodSize*getPcmSampleSizeInBytes(mMixPortConfig.format.value().pcm),fp_pcm_dump);
                        fclose(fp_pcm_dump);
                    } else {
                        LOG(ERROR) << __func__ << " error in opening dump file " << dump_file;
                    }
                }
            }
#endif
#ifdef ECNR_HAL_TUNE
            getTuneIOBuffer = mAudExt.mHalExtension->audio_extn_get_TuneIO_buffer(&pECNR_TuneIFData, &(pECNR_ProcessData.sECNRTuneIO));
            if (getTuneIOBuffer >= 0)
                ret = mAudExt.mHalExtension->audio_extn_ecnrProcess(pECNR_ProcessData.pMain, pECNR_ProcessData.audioIO, &(pECNR_ProcessData.sECNRTuneIO));
            else
#endif
            ret = mAudExt.mHalExtension->audio_extn_ecnrProcess(pECNR_ProcessData.pMain, pECNR_ProcessData.audioIO, NULL);
#ifdef ECNR_HAL_TUNE
            mAudExt.mHalExtension->audio_extn_feedback_TuneIO_buffer(&pECNR_TuneIFData, &(pECNR_ProcessData.sECNRTuneIO));
#endif
#ifdef ECNR_HAL_SRC_CP
            if (property_get_bool("vendor.audio.feature.ecnr.force_error", false)) {
                ret = -1;
            }
#endif
            if (!ret) {
#ifdef ECNR_HAL_DUMP_ENABLE
                 if (property_get_bool("vendor.audio.feature.ecnr.dump", false)) {
//                    LOG(DEBUG) << __func__ << "DUMP enabled for ecnr deinterleaved data of ecnr output";
                    char dump_file[128];
                    FILE *fp_pcm_dump = NULL;
                    for (unsigned int i =0 ; i < pECNR_ProcessData.audioIO.RcvProcBufferCnt; i++) {
                        snprintf(dump_file, sizeof(dump_file), "%s/%s_ecnr_test_deinterleave_%d_out.raw",AUDIO_HAL_DUMP_PATH,mTagName.c_str(),i);
                        fp_pcm_dump=fopen(dump_file,"a+");
                        if (fp_pcm_dump) {
                            fwrite(deint_out_buffer + i*ecnrPeriodSize,sizeof(char),ecnrPeriodSize*getPcmSampleSizeInBytes(mMixPortConfig.format.value().pcm),fp_pcm_dump);
                            fclose(fp_pcm_dump);
                        } else {
                            LOG(ERROR) << __func__ << mLogPrefixOEM << " error in opening dump file " << dump_file;
                        }
                    }
                }
#endif
                if (pECNR_ProcessData.audioIO.RcvProcBufferCnt== 1) {
//                        LOG(DEBUG) << __func__ << "channel size 1, just copy";
                    memcpy(dst_buffer, deint_out_buffer, ecnr_out_buffer_size);
                } else if (pECNR_ProcessData.audioIO.RcvProcBufferCnt > 1) {
//                        LOG(DEBUG) << __func__ << " change format";
                    mAudExt.mHalExtension->audio_extn_cvtformat16_delnterleave_to_interleave(deint_out_buffer,dst_buffer,ecnrPeriodSize,pECNR_ProcessData.audioIO.RcvProcBufferCnt);
                }
            }
write_without_process:
            bytesWritten = ::pal_stream_write(mPalHandle, &palBuffer);
            if (bytesWritten < 0) {
                LOG(ERROR) << __func__ << mLogPrefixOEM << " write failed, ret: " << bytesWritten;
                *actualFrameCount = frameCount;
                return onWriteError(frameCount);
            }
#ifdef PCM_DUMP_HAL_ENABLE
            if (pcm_dump) {
                if (fp_out_dump) {
                    fwrite(palBuffer.buffer,sizeof(char),palBuffer.size ,fp_out_dump);
                }
            }
#endif
#ifdef ECNR_HAL_SRC_CP
        }
skip_write :
#endif
        *actualFrameCount = frameCount;
#ifdef VERY_VERBOSE_LOGGING
        LOG(VERBOSE) << __func__ << mLogPrefixOEM << ": byteswritten: " << bytesWritten;
#endif

        // Todo findout write latency
        *latencyMs = mContext.getNominalLatencyMs();
        if (hasBluetoothDevice(mConnectedDevices)) {
            const auto& btlatencyMs = mPlatform.getBluetoothLatencyMs(mConnectedDevices);
            *latencyMs += btlatencyMs;
        }
    }
    return ::android::OK;
}

void StreamOutPrimaryOEM::configure() {
    int ret = 0, vocoder_rate, conn_type, cp_type;
    ecnrSampleRate = mMixPortConfig.sampleRate.value().value;
    ecnrPeriodSize = mPlatform.getFrameCount(mMixPortConfig, mTag);
    mChannels = getChannelCount(mMixPortConfig.channelMask.value());
    bECNRprop_Enable = property_get_bool(ECNR_FEATURE_PROP, false);
    vocoder_rate =  mAudExt.mHalExtension->get_vocoder_rate();
    conn_type = mAudExt.mHalExtension->get_conn_type();
    cp_type = mAudExt.mHalExtension->get_cp_type();
    LOG(DEBUG) << __func__ << " Vocoder_samplerate : " << vocoder_rate << " connection_type : " << conn_type << " carplay_type : " << cp_type;

    const auto startTime = std::chrono::steady_clock::now();
    auto attr = mPlatform.getPalStreamAttributes(mMixPortConfig, false);

    if (!attr) {
        LOG(ERROR) << __func__ << mLogPrefixOEM << " no pal attributes found";
        return;
    }
    attr->bus_addr = "";
    if (!mConnectedDevices.empty()) {
         std::string deviceAddress =  mConnectedDevices[0].address.get<AudioDeviceAddress::Tag::id>();
         LOG(INFO) << __func__ << " : deviceAddress " << deviceAddress;
         bool isBusType = (std::string::npos != deviceAddress.find("BUS")) ? true : false;
         if (isBusType) {
             attr->bus_addr = new char[deviceAddress.length() + 1];
             strlcpy(attr->bus_addr,deviceAddress.c_str(),deviceAddress.length() + 1);
         }
    } else {
         LOG(DEBUG) << __func__ << mLogPrefixOEM << ": connected device empty";
    }

    if (!(mTag == Usecase::VOIP_PLAYBACK && mAudExt.mHalExtension->audio_extn_getEnablement() && bECNRprop_Enable && (vocoder_rate > 0 || conn_type >= 0))) {
        bECNR_Enable = false;
    } else {
        bECNR_Enable = true;
        LOG(INFO) << __func__ << mLogPrefixOEM << " : bECNR_Enable " << bECNR_Enable;
        attr->type = PAL_STREAM_VOIP_RX;

#ifdef ECNR_HAL_SRC_CP
        if((vocoder_rate == 8000) || (vocoder_rate == 16000) || (vocoder_rate == 24000)) {
            ecnrSampleRate = 24000;
        } else if(vocoder_rate == 32000) {
            ecnrSampleRate = 32000;
        } else if(vocoder_rate == 48000) {
            ecnrSampleRate = 48000;
        }
        VoipPlaybackECNR::kSampleRate = ecnrSampleRate;
        ecnrPeriodSize = UsecaseConfig<VoipPlaybackECNR>::getDLECNRPeriodSize(VoipPlaybackECNR::kSampleRate);
        LOG(INFO) << __func__ << mLogPrefixOEM << " ecnrSampleRate:  " << ecnrSampleRate << " encrPeriodSize: "<< ecnrPeriodSize;
#endif
        if (vocoder_rate > 0 && conn_type >= 0) {
            pECNR_ProcessData.ecnr_type = ECNR_TYPE_TEL;
        } else if (conn_type >=0 && cp_type == FACETIME) {
            pECNR_ProcessData.ecnr_type = ECNR_TYPE_FACETIME;
        } else {
            LOG(ERROR) << __func__ << mLogPrefixOEM << " Invalid ECNR type";
            pECNR_ProcessData.ecnr_type = INVALID;
            bECNR_Enable = false;
            goto skip_ecnr_configuration;
        }
        pECNR_ProcessData.scd_type = mAudExt.mHalExtension->audio_extn_getSCDtype( ecnrSampleRate, vocoder_rate, pECNR_ProcessData.ecnr_type, conn_type, DIR_DL);
        if (mAudExt.mHalExtension->audio_extn_getSCDdata(&pECNR_ProcessData, DIR_DL)) {
            LOG(ERROR) << __func__ << mLogPrefixOEM << " failed to get scd information, disabling ecnr processing";
            bECNR_Enable = false;
            goto skip_ecnr_configuration;
        }

        ecnr_out_buffer_size = ecnrPeriodSize * mFrameSizeBytes ;
//            LOG(DEBUG) << __func__ << mLogPrefixOEM <<  "buffer alloc for OUT proc deinterleved data) for output");
        LOG(INFO) << __func__ << mLogPrefixOEM << " buffer alloc " << ecnr_out_buffer_size << " for output ch "<< mChannels;

        ecnr_out_buffer = std::make_unique<uint8_t[]>(ecnr_out_buffer_size);
        if (!ecnr_out_buffer) {
            LOG(ERROR) << __func__ << mLogPrefixOEM << " failed to allocate ecnr_out_buffer";
            bECNR_Enable = false;
            goto skip_ecnr_configuration;
        }
        ecnr_in_buffer_size = ecnr_out_buffer_size;
        ecnr_in_buffer = std::make_unique<uint8_t[]>(ecnr_in_buffer_size);
        if (!ecnr_in_buffer) {
            LOG(ERROR) << __func__ << mLogPrefixOEM << " failed to allocate ecnr_in_buffer";
            bECNR_Enable = false;
            goto skip_ecnr_configuration;
        }
        mAudExt.mHalExtension->audio_extn_setupIOBuffer(&pECNR_ProcessData, DIR_DL, attr->out_media_config.ch_info.channels, attr->out_media_config.ch_info.channels, ecnrPeriodSize, ecnr_in_buffer.get(), ecnr_out_buffer.get());
        ret = mAudExt.mHalExtension->audio_extn_setupECNR(&pECNR_ProcessData);
        if (ret) {
            LOG(ERROR) << __func__ << mLogPrefixOEM << " audio_extn_setupECNR ret" << ret;
            bECNR_Enable = false;
            goto skip_ecnr_configuration;
        }
#ifdef ECNR_HAL_TUNE
        mAudExt.mHalExtension->audio_extn_setupECNR_TuneIF(&pECNR_TuneIFData, ECNR_PORT_ID_VOIP_RX_2015);
#endif
#ifdef ECNR_HAL_SRC_CP
        if (mMixPortConfig.sampleRate.value().value != ecnrSampleRate) {
            ret = create_resampler(mMixPortConfig.sampleRate.value().value,
                                   ecnrSampleRate,
                                   mChannels,
                                   RESAMPLER_QUALITY_DEFAULT,
                                   NULL,
                                   &resampler);
            if (ret != 0) {
                resampler = NULL;
                LOG(ERROR) << __func__ << " failure to create resampler " << ret;
                bECNR_Enable = false;
                goto skip_ecnr_configuration;
            }
        } else {
            resampler = NULL;
            LOG(ERROR) << __func__ << " resampler is not required";
        }
        if (resampler) {
            ecnr_src_buffer_size = (mPlatform.getFrameCount(mMixPortConfig, mTag)*ecnrSampleRate)/mMixPortConfig.sampleRate.value().value * mFrameSizeBytes;
            LOG(INFO) << __func__ << mLogPrefixOEM << " ecnr_src_buffer_size alloc " << ecnr_src_buffer_size;
            ecnr_src_buffer = std::make_unique<uint8_t[]>(ecnr_src_buffer_size);
            if (!ecnr_src_buffer) {
                LOG(ERROR) << __func__ << mLogPrefixOEM << " failed to allocate ecnr_src_buffer";
                bECNR_Enable = false;
                goto skip_ecnr_configuration;
            }

            mOutHalRingBuffer = std::make_unique<HalRingBuffer<int16_t>>(ecnrPeriodSize*3*mChannels);
            if (!mOutHalRingBuffer)
            {
                LOG(ERROR) << __func__ << " Pointer is null after allocation attempt";
                ret = ENOMEM_BUFFER;
                bECNR_Enable = false;
                goto skip_ecnr_configuration;
            }
        }
        attr->out_media_config.sample_rate = ecnrSampleRate;
        LOG(ERROR) << __func__ << " sample rate attr : " << attr->out_media_config.sample_rate;
#endif
    }
skip_ecnr_configuration :
    LOG(VERBOSE) << __func__ << mLogPrefixOEM << " assigned pal stream type:" << attr->type;
    if (bECNR_Enable) {
        auto palDevices =
        mPlatform.configureAndFetchPalDevices(mMixPortConfig, mTag, mConnectedDevices);
        if (!palDevices.size()) {
            LOG(ERROR) << __func__ << mLogPrefixOEM << " no connected devices on stream!!";
            return;
        }

        uint64_t cookie = reinterpret_cast<uint64_t>(this);
        pal_stream_callback palFn = nullptr;

        const auto palOpenApiStartTime = std::chrono::steady_clock::now();
        if (int32_t ret = ::pal_stream_open(attr.get(), palDevices.size(), palDevices.data(), 0,
                                        nullptr, palFn, cookie, &(this->mPalHandle));
        ret) {
            LOG(ERROR) << __func__ << mLogPrefixOEM << " pal stream open failed!!! ret:" << ret;
            mPalHandle = nullptr;
            return;
        }

        if (mUseCachedVolume) {
            setHwVolume(mVolumes);
        }

        const auto palOpenApiEndTime = std::chrono::steady_clock::now();

        auto bufConfig = getBufferConfigOEM();
        size_t ringBufSizeInBytes = ecnr_out_buffer_size;
        const size_t ringBufCount = bufConfig.bufferCount;

        auto palBufferConfig = mPlatform.getPalBufferConfig(ringBufSizeInBytes, ringBufCount);
        LOG(DEBUG) << __func__ << mLogPrefixOEM << "set pal_stream_set_buffer_size to "
               << std::to_string(ringBufSizeInBytes) << " with count "
               << std::to_string(ringBufCount);
        if (int32_t ret =
                ::pal_stream_set_buffer_size(this->mPalHandle, nullptr, palBufferConfig.get());
        ret) {
            LOG(ERROR) << __func__ << mLogPrefixOEM << " pal_stream_set_buffer_size failed!!! ret:" << ret;
            ::pal_stream_close(mPalHandle);
            mPalHandle = nullptr;
            return;
        }
        const auto palStartApiStartTime = std::chrono::steady_clock::now();
        if (int32_t ret = ::pal_stream_start(this->mPalHandle); ret) {
            LOG(ERROR) << __func__ << mLogPrefixOEM << " pal_stream_start failed, ret:" << ret;
            ::pal_stream_close(mPalHandle);
            mPalHandle = nullptr;
            return;
        }

        const auto palStartApiEndTime = std::chrono::steady_clock::now();

        LOG(VERBOSE) << __func__ << mLogPrefixOEM << " pal_stream_start successful";

        if (mPlaybackRate != sDefaultPlaybackRate) {
            LOG(DEBUG) << __func__ << mLogPrefixOEM << ": using playspeed " << mPlaybackRate.speed;
            mPlatform.setPlaybackRate(mPalHandle, mTag, mPlaybackRate);
        }

        LOG(INFO) << __func__ << mLogPrefixOEM << ": stream is configured";
        const auto endTime = std::chrono::steady_clock::now();
        using FloatMillis = std::chrono::duration<float, std::milli>;
        const float palStreamOpenTimeTaken =
            std::chrono::duration_cast<FloatMillis>(palOpenApiEndTime - palOpenApiStartTime)
                    .count();
        const float palStreamStartTimeTaken =
            std::chrono::duration_cast<FloatMillis>(palStartApiEndTime - palStartApiStartTime)
                    .count();
        const float timeTaken = std::chrono::duration_cast<FloatMillis>(endTime - startTime).count();
        LOG(INFO) << __func__ << mLogPrefixOEM << ": completed in " << timeTaken
              << " ms [pal_stream_open: " << palStreamOpenTimeTaken
              << ", ms pal_stream_start: " << palStreamStartTimeTaken << " ms]";

#ifdef PCM_DUMP_HAL_ENABLE
        if (property_get_bool("vendor.audio.feature.pcm_dump.out", false)) {
            pcm_dump = true;
            LOG(DEBUG) <<"pcm_dump enabled";
            char dump_file[128];
            snprintf(dump_file, sizeof(dump_file), "%s/%s_%s_in.raw",AUDIO_HAL_DUMP_PATH,mTagName.c_str(),attr->bus_addr);
            if (fp_in_dump) {
                fclose(fp_in_dump);
            }
            fp_in_dump=fopen(dump_file,"wb+");
            if (!fp_in_dump) {
                LOG(DEBUG) <<"opening error "<< dump_file;
                fp_in_dump = NULL;
            }
            if (fp_out_dump) {
                fclose(fp_in_dump);
            }
            snprintf(dump_file, sizeof(dump_file), "%s/%s_%s_out.raw",AUDIO_HAL_DUMP_PATH,mTagName.c_str(),attr->bus_addr);
            fp_out_dump=fopen(dump_file,"wb+");
            if (!fp_in_dump) {
                LOG(DEBUG) <<"opening error "<< dump_file;
                fp_out_dump = NULL;
            }
        }
#endif
    } else {
        return StreamOutPrimary::configure();
    }
}
void StreamOutPrimaryOEM::shutdown_I() {

    StreamOutPrimary::shutdown_I();

    if (bECNR_Enable && (mTag == Usecase::VOIP_PLAYBACK)) {
        mAudExt.mHalExtension->set_vocoder_rate(INVALID);
        mAudExt.mHalExtension->set_conn_type(INVALID);
        mAudExt.mHalExtension->set_cp_type(INVALID);
    }
    bECNR_Enable = false;
    property_set("vendor.audio.ecnr.scd.dl", "");

    if (ecnr_out_buffer) {
        ecnr_out_buffer.reset();
    }
    ecnr_out_buffer_size = 0;
    if (ecnr_in_buffer) {
        ecnr_in_buffer.reset();
    }
    ecnr_in_buffer_size = 0;

#ifdef ECNR_HAL_SRC_CP
    if (ecnr_src_buffer) {
        ecnr_src_buffer.reset();
    }
    ecnr_src_buffer_size = 0;

    if (mOutHalRingBuffer) {
        mOutHalRingBuffer.reset();
    }
#endif
    if (pECNR_ProcessData.scd_buffer[0]) {
        LOG(INFO) << __func__ << mLogPrefixOEM << " free scd_buffer[0]";
        free(pECNR_ProcessData.scd_buffer[0]);
        pECNR_ProcessData.scd_buffer[0] = NULL;
    }
    pECNR_ProcessData.scd_buffer_size[0] = 0;
    if (pECNR_ProcessData.scd_buffer[1]) {
        LOG(INFO) << __func__ << mLogPrefixOEM << " free scd_buffer[1]";
        free(pECNR_ProcessData.scd_buffer[1]);
        pECNR_ProcessData.scd_buffer[1] = NULL;
    }
    pECNR_ProcessData.scd_buffer_size[1] = 0;

#ifdef ECNR_HAL_TUNE
    mAudExt.mHalExtension->audio_extn_close_TuneIF(&pECNR_TuneIFData);
#endif
    mAudExt.mHalExtension->audio_extn_resetIOBuffer(&pECNR_ProcessData);
    memset(&(pECNR_ProcessData.audioIO), 0, sizeof(tECNR_AudioIO));
    memset(&(pECNR_ProcessData.sECNRTuneIO), 0, sizeof(tECNR_TuneIO));
    mAudExt.mHalExtension->audio_extn_ecnrDestroy(&(pECNR_ProcessData.pMain));
    pECNR_ProcessData.pMain = NULL;
#ifdef ECNR_HAL_SRC_CP
    if (resampler != NULL) {
        release_resampler(resampler);
    }
    resampler = NULL;
#endif
#ifdef PCM_DUMP_HAL_ENABLE
    pcm_dump = false;
    if (fp_in_dump)
        fclose(fp_in_dump);
    if (fp_out_dump)
        fclose(fp_out_dump);
    fp_in_dump = NULL;
    fp_out_dump = NULL;
#endif

}
struct BufferConfig StreamOutPrimaryOEM::getBufferConfigOEM() {
    return mPlatform.getBufferConfig(mMixPortConfig, mTag);
}

} // namespace qti::audio::core
