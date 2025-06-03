/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL_StreamInOEM_QTI"

#include <cmath>

#include <aidl/android/hardware/audio/effect/IEffect.h>

#include <android-base/logging.h>
#include <audio_utils/clock.h>
#include <hardware/audio.h>
#include <qti-audio-core/Module.h>
#include <qti-audio-core/ModulePrimary.h>
#include <qti-audio/PlatformConverter.h>
#include <qti-audio-core/StreamInPrimaryOEM.h>
#include <system/audio.h>

using aidl::android::hardware::audio::common::AudioOffloadMetadata;
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

using aidl::android::hardware::audio::common::getPcmSampleSizeInBytes;
// uncomment this to enable logging of very verbose logs like burst commands.
// #define VERY_VERBOSE_LOGGING 1

namespace qti::audio::core {

#define READ_RETRY_COUNT 10
int VoipRecordECNR::kSampleRate = 48000;


StreamInPrimaryOEM::StreamInPrimaryOEM(StreamContext&& context, const SinkMetadata& sinkMetadata,
                                 const std::vector<MicrophoneInfo>& microphones)
    : StreamInPrimary(std::move(context),sinkMetadata, microphones) {

    std::ostringstream os;
    os << " : usecase: " << mTagName;
    os << " IoHandle:" << mMixPortConfig.ext.get<AudioPortExt::Tag::mix>().handle;
    mLogPrefixOEM = os.str();
    mScd_file_path_index_ul = INVALID_PATH;

    LOG(INFO) << __func__ << mLogPrefixOEM;
}

StreamInPrimaryOEM::~StreamInPrimaryOEM() {
    shutdown_I();
    LOG(INFO) << __func__ << mLogPrefixOEM;
}

::android::status_t StreamInPrimaryOEM::pause() {
    // Todo check whether pause is possible in PAL
    shutdown_I();
    return ::android::OK;
}
::android::status_t StreamInPrimaryOEM::standby() {
    shutdown_I();
    return ::android::OK;
}
void StreamInPrimaryOEM::shutdown() {
    return shutdown_I();
}
::android::status_t StreamInPrimaryOEM::transfer(void* buffer, size_t frameCount,
                                              size_t* actualFrameCount, int32_t* latencyMs) {
    if (!mPalHandle) {
        configure();
        if (!mPalHandle) {
            LOG(ERROR) << __func__ << mLogPrefixOEM << ": failed to configure";
            *actualFrameCount = frameCount;
            return onReadErrorOEM(frameCount);
        }
    }
    if (!bECNR_Enable) {
        return StreamInPrimary::transfer(buffer, frameCount, actualFrameCount, latencyMs);
    } else {
        pal_buffer palBuffer{};
        palBuffer.buffer = static_cast<uint8_t*>(buffer);
        palBuffer.size = frameCount * mFrameSizeBytes;
        if (ecnr_ecmx_buffer && ecnr_ecmx_buffer_size > 0) {
//            LOG(DEBUG) << __func__ << mLogPrefixOEM << " raplace buffer for the ECMX";
            palBuffer.buffer = reinterpret_cast<uint8_t*>(ecnr_ecmx_buffer.get());
            palBuffer.size = ecnr_ecmx_buffer_size;
        }
        int32_t bytesRead = 0;
#ifdef ECNR_HAL_SRC_CP
        size_t rbWritten = 0;
        size_t rbRead = 0;
        int preparedBuffer = 0;
#endif
        int16_t *deint_in_buffer = reinterpret_cast<int16_t*>(ecnr_in_buffer.get());
        int16_t *deint_out_buffer = reinterpret_cast<int16_t*>(ecnr_out_buffer.get());
        int16_t *src_buffer = reinterpret_cast<int16_t*>(palBuffer.buffer);
        int16_t *dst_buffer = reinterpret_cast<int16_t*>(buffer);
        int16_t *rsp_in_buffer = dst_buffer;
#ifdef ECNR_HAL_SRC_CP
        if (resampler != NULL) {
            rsp_in_buffer = reinterpret_cast<int16_t*>(palBuffer.buffer);
        }
#endif
        uint8_t bytesPerSample = getPcmSampleSizeInBytes(mMixPortConfig.format.value().pcm);
        int ret = 0;
#ifdef ECNR_HAL_TUNE
        int getTuneIOBuffer = 0;
#endif

#ifdef VERY_VERBOSE_LOGGING
        LOG(VERBOSE) << __func__ << mLogPrefixOEM << ": framecount " << frameCount << " mFrameSizeBytes "
                     << mFrameSizeBytes;
#endif
#ifdef ECNR_HAL_SRC_CP
        while ( preparedBuffer < 1) {
            if ((resampler != NULL) && (rsp_in_buffer != dst_buffer)) {
                preparedBuffer = mInHalRingBuffer->get()->avaiableWrittenBufferSize()/(frameCount*ecnrSampleRate/mMixPortConfig.sampleRate.value().value);
                if (preparedBuffer > 0) {
                    size_t inFrameCount = (size_t) (frameCount*ecnrSampleRate/mMixPortConfig.sampleRate.value().value);
                    size_t outFrameCount = (size_t)frameCount;
                    rbRead = mInHalRingBuffer->get()->read(rsp_in_buffer, inFrameCount*mChannels);
                    resampler->resample_from_input(resampler, rsp_in_buffer,
                                                         &inFrameCount, dst_buffer,
                                                         &outFrameCount);
//                    LOG(DEBUG) << __func__ << mLogPrefixOEM << " inFrameCount " << inFrameCount << " ecnrPeriodSize " << ecnrPeriodSize;
//                    LOG(DEBUG) << __func__ << mLogPrefixOEM << " outFrameCount " << outFrameCount << " frameCount " << frameCount;

                    if (outFrameCount != frameCount) {
                        LOG(DEBUG) << __func__ << mLogPrefixOEM << " framecount is not matched requested : " << frameCount << " output : " << outFrameCount;
                    }
                    bytesRead = frameCount * mFrameSizeBytes;
                    break;
                }
            }
#endif

            bytesRead = ::pal_stream_read(mPalHandle, &palBuffer);
            if (mTag == Usecase::PCM_RECORD) {
                *latencyMs = PcmRecordECNR::getLatency();
            } else if (mTag == Usecase::FAST_RECORD) {
                *latencyMs = FastRecordECNR::getLatency();
            } else if (mTag == Usecase::VOIP_RECORD) {
                *latencyMs = VoipRecordECNR::getLatency();
            }
#ifdef ECNR_HAL_SRC_CP
            if (bytesRead < 0)
                break;
#endif
#ifdef PCM_DUMP_HAL_ENABLE
            if (pcm_dump) {
                if (fp_in_dump) {
                    fwrite(palBuffer.buffer,sizeof(char),palBuffer.size,fp_in_dump);
                }
            }
#endif
            if (bytesRead > 0) {
                mAudExt.mHalExtension->audio_extn_cvtformat16_lnterleave_to_deinterleave(src_buffer,deint_in_buffer,ecnrPeriodSize, ECNR_MIC_EC_CH);
#ifdef ECNR_HAL_DUMP_ENABLE
                if (property_get_bool("vendor.audio.feature.ecnr.dump", false)) {
//                LOG(DEBUG) << __func__ << "DUMP enabled for ecnr deinterleaved data of ecnr input";
                    char dump_file[128];
                    FILE *fp_pcm_dump = NULL;
                    for (int i =0 ; i < ECNR_MIC_EC_CH; i++) {
                        snprintf(dump_file, sizeof(dump_file), "%s/%s_ecnr_test_deinterleave_%d_in.raw",AUDIO_HAL_DUMP_PATH,mTagName.c_str(),i);
                        fp_pcm_dump=fopen(dump_file,"a+");
                        if (fp_pcm_dump) {
                            fwrite(deint_in_buffer + i*ecnrPeriodSize,sizeof(char),ecnrPeriodSize*bytesPerSample,fp_pcm_dump);
                            fclose(fp_pcm_dump);
                        } else {
                            LOG(ERROR) << __func__ << mLogPrefixOEM << " error in opening dump file " << dump_file;
                        }
                    }
                }
#endif
#ifdef ECNR_HAL_TUNE
                getTuneIOBuffer = mAudExt.mHalExtension->audio_extn_get_TuneIO_buffer(&pECNR_TuneIFData, &(pECNR_ProcessData.sECNRTuneIO));
                if (getTuneIOBuffer >= 0)
                    ret = mAudExt.mHalExtension->audio_extn_ecnrProcess(pECNR_ProcessData.pMain, pECNR_ProcessData.audioIO, &(pECNR_ProcessData.sECNRTuneIO));
                else {
#endif
                    ret = mAudExt.mHalExtension->audio_extn_ecnrProcess(pECNR_ProcessData.pMain, pECNR_ProcessData.audioIO, NULL);
                    if (ret) {
                        for(int path_index = mScd_file_path_index_ul; path_index < MAX_SCD_PATH_INDEX; path_index++) {
                            if(mScd_file_path_index_ul == DEFAULT_PATH) {
                                LOG(ERROR) << __func__ << mLogPrefixOEM << " ECNR_Process failed ret = " << ret;
                                shutdown_I();
                            } else {
                                //Increamenting file path index to skip the used path
                                mScd_file_path_index_ul++;
                                if (mAudExt.mHalExtension->audio_extn_getSCDdata(&pECNR_ProcessData, DIR_UL, &mScd_file_path_index_ul)) {
                                    LOG(ERROR) << __func__ << mLogPrefixOEM << " failed to get scd information, disabling ecnr processing";
                                    shutdown_I();
                                }
                                LOG(DEBUG) << __func__ << " path: " << mScd_file_path_index_ul;
                                mAudExt.mHalExtension->audio_extn_setupIOBuffer(&pECNR_ProcessData, DIR_UL, ECNR_MIC_EC_CH, mChannels, ecnrPeriodSize, ecnr_in_buffer.get(), ecnr_out_buffer.get());
                                ret = mAudExt.mHalExtension->audio_extn_setupECNR(&pECNR_ProcessData);
                                if (ret) {
                                    LOG(ERROR) << __func__ << mLogPrefixOEM << " audio_extn_setupECNR failed ret" << ret;
                                    shutdown_I();
                                }
#ifdef ECNR_HAL_TUNE
                                mAudExt.mHalExtension->audio_extn_setupECNR_TuneIF(&pECNR_TuneIFData, portid);
#endif
                            }
                            ret = mAudExt.mHalExtension->audio_extn_ecnrProcess(pECNR_ProcessData.pMain, pECNR_ProcessData.audioIO, NULL);
                            if(!ret) {
                                LOG(DEBUG) << __func__ << " ret = "<< ret;
                                break;
                            }
                        }
                    }
#ifdef ECNR_HAL_TUNE
                }
                mAudExt.mHalExtension->audio_extn_feedback_TuneIO_buffer(&pECNR_TuneIFData, &(pECNR_ProcessData.sECNRTuneIO));
#endif
#ifdef ECNR_HAL_SRC_CP
                if (property_get_bool("vendor.audio.feature.ecnr.force_error", false)) {
                    ret = -1;
                }
#endif
#ifdef ECNR_HAL_DUMP_ENABLE
                if (property_get_bool("vendor.audio.feature.ecnr.dump", false)) {
//                LOG(DEBUG) << __func__ << "DUMP enabled for ecnr deinterleaved data of ecnr output";
                    char dump_file[128];
                    FILE *fp_pcm_dump = NULL;
                    for (unsigned int i =0 ; i < pECNR_ProcessData.audioIO.MicProcBufferCnt; i++) {
                        snprintf(dump_file, sizeof(dump_file), "%s/%s_ecnr_test_deinterleave_%d_out.raw",AUDIO_HAL_DUMP_PATH,mTagName.c_str(),i);
                        fp_pcm_dump=fopen(dump_file,"a+");
                        if (fp_pcm_dump) {
                            fwrite(deint_out_buffer + i*ecnrPeriodSize,sizeof(char),ecnrPeriodSize*bytesPerSample,fp_pcm_dump);
                            fclose(fp_pcm_dump);
                        } else {
                            LOG(ERROR) << __func__ << mLogPrefixOEM << " error in opening dump file " << dump_file;
                        }
                    }
                }
#endif
                if (!ret) {
                    if (pECNR_ProcessData.audioIO.MicProcBufferCnt== 1) {
//                        LOG(DEBUG) << __func__ << "channel size 1, just copy";
                        memcpy(rsp_in_buffer, deint_out_buffer, ecnr_out_buffer_size);
                    } else if (pECNR_ProcessData.audioIO.MicProcBufferCnt > 1) {
//                        LOG(DEBUG) << __func__ << " change format";
                         mAudExt.mHalExtension->audio_extn_cvtformat16_delnterleave_to_interleave(deint_out_buffer,rsp_in_buffer,ecnrPeriodSize,pECNR_ProcessData.audioIO.MicProcBufferCnt);
                    }
                } else {
                    LOG(ERROR) << __func__ << mLogPrefixOEM << " ecnr processing error ";
                    if (mChannels == 1) {
                        memcpy(rsp_in_buffer,deint_in_buffer,ecnrPeriodSize*bytesPerSample);
                    } else {
                        for (int i = 0; i < ecnrPeriodSize; i++) {
                            for (int j = 0 ; j < mChannels ; j++) {
                                rsp_in_buffer[(i * mChannels) +j] = deint_in_buffer[(j%ECNR_MIC_CH)*ecnrPeriodSize+i];
                            }
                        }
                    }
                }
#ifdef ECNR_HAL_SRC_CP
                if ((resampler != NULL) && (rsp_in_buffer != dst_buffer)) {
                    rbWritten = mInHalRingBuffer->get()->write(rsp_in_buffer, ecnrPeriodSize*mChannels);
                    if ((size_t)(ecnrPeriodSize*mChannels) != rbWritten) {
                        LOG(DEBUG) << __func__ << mLogPrefixOEM << " Dropping samples : " << (rbWritten - ecnrPeriodSize*mChannels );
                    }
                } else {
                    preparedBuffer = 1;
                }
#endif
            }
#ifdef ECNR_HAL_SRC_CP
        }
#endif

        if (bytesRead < 0) {
            LOG(ERROR) << __func__ << mLogPrefixOEM << " read failed, ret:" << std::to_string(bytesRead);
            *actualFrameCount = frameCount;
             return onReadErrorOEM(frameCount);
        } else {
#ifdef PCM_DUMP_HAL_ENABLE
            if (pcm_dump) {
                if (fp_out_dump) {
                    fwrite(buffer,sizeof(char),frameCount * mFrameSizeBytes,fp_out_dump);
                }
            }
#endif
            *actualFrameCount = frameCount;
        }

#ifdef VERY_VERBOSE_LOGGING
        LOG(VERBOSE) << __func__ << mLogPrefixOEM << ": bytes read " << bytesRead << ", return frame count "
                     << *actualFrameCount;
#endif
    }
    return ::android::OK;
}

void StreamInPrimaryOEM::configure() {
    LOG(INFO) << __func__ << mLogPrefixOEM;
    int ret = 0, vocoder_rate, conn_type, cp_type;
    ecnrSampleRate = mMixPortConfig.sampleRate.value().value;
    ecnrPeriodSize = mPlatform.getFrameCount(mMixPortConfig, mTag);
    mChannels = getChannelCount(mMixPortConfig.channelMask.value());
    bECNRprop_Enable = property_get_bool(ECNR_FEATURE_PROP, false);
    vocoder_rate =  mAudExt.mHalExtension->get_vocoder_rate();
    conn_type = mAudExt.mHalExtension->get_conn_type();
    cp_type = mAudExt.mHalExtension->get_cp_type();
    LOG(DEBUG) << __func__ << " Vocoder_samplerate : " << vocoder_rate << " connection_type : " << conn_type << " carplay_type : " << cp_type;

#ifdef ECNR_HAL_TUNE
    portid = ECNR_PORT_ID_VR_TX_2016;
#endif
    const auto startTime = std::chrono::steady_clock::now();
    auto attr = mPlatform.getPalStreamAttributes(mMixPortConfig, true);
    auto palDevices = mPlatform.configureAndFetchPalDevices(mMixPortConfig, mTag, mConnectedDevices);
    if (!attr) {
        LOG(ERROR) << __func__ << mLogPrefixOEM << " no pal attributes";
        return;
    }
    if (!mConnectedDevices.empty()) {
        std::string deviceAddress =  mConnectedDevices[0].address.get<AudioDeviceAddress::Tag::id>();
        LOG(INFO) << __func__ << "configure(): deviceAddress " << deviceAddress;
        attr->bus_addr = new char[deviceAddress.length() + 1];
        strlcpy(attr->bus_addr, deviceAddress.c_str(), deviceAddress.length() + 1);
    } else {
        LOG(DEBUG) << __func__ << mLogPrefixOEM << ": connected device empty";
    }

    auto bufConfig = getBufferConfigOEM();
    if (mAudExt.mHalExtension->audio_extn_getEnablement() && bECNRprop_Enable &&
        ((strcmp(attr->bus_addr, "BUS_INPUT_3rdpartyvr0") == 0) || (mTag == Usecase::VOIP_RECORD))) {
        if (strcmp(attr->bus_addr, "BUS_INPUT_3rdpartyvr0") == 0) {
            bECNR_Enable = true;
            LOG(INFO) << __func__ << " bECNR_Enable " << bECNR_Enable;
            attr->type = PAL_STREAM_CAPTURE_BUS;
            pECNR_ProcessData.ecnr_type = ECNR_TYPE_VR;
#ifdef ECNR_HAL_TUNE
            portid = ECNR_PORT_ID_VR_TX_2016;
#endif
        } else if (vocoder_rate > 0 && conn_type >= 0 && mTag == Usecase::VOIP_RECORD) {
            bECNR_Enable = true;
            LOG(INFO) << __func__ << " bECNR_Enable " << bECNR_Enable;
            attr->type = PAL_STREAM_VOIP_TX;
            pECNR_ProcessData.ecnr_type = ECNR_TYPE_TEL;
#ifdef ECNR_HAL_SRC_CP
            if((vocoder_rate == 8000) || (vocoder_rate == 16000) || (vocoder_rate == 24000)) {
                ecnrSampleRate = 24000;
            } else if(vocoder_rate == 32000) {
                ecnrSampleRate = 32000;
            } else if(vocoder_rate == 48000) {
                ecnrSampleRate = 48000;
            }
            VoipRecordECNR::kSampleRate = ecnrSampleRate;
            ecnrPeriodSize = UsecaseConfig<VoipRecordECNR>::getULECNRPeriodSize(VoipRecordECNR::kSampleRate);
            LOG(INFO) << __func__ << mLogPrefixOEM << " ecnrSampleRate:  " << ecnrSampleRate << " encrPeriodSize: "<< ecnrPeriodSize;

#endif
#ifdef ECNR_HAL_TUNE
            portid = ECNR_PORT_ID_VOIP_TX_2014;
#endif
        } else if (conn_type >= 0) {
            bECNR_Enable = true;
            LOG(INFO) << __func__ << " bECNR_Enable " << bECNR_Enable;
            attr->type = PAL_STREAM_VOIP_TX;
            if (cp_type == SIRI)
                pECNR_ProcessData.ecnr_type = ECNR_TYPE_LEGACY_SIRI;
            else if (cp_type == FACETIME)
                pECNR_ProcessData.ecnr_type = ECNR_TYPE_FACETIME;
#ifdef ECNR_HAL_TUNE
            portid = ECNR_PORT_ID_VOIP_TX_2014;
#endif
        } else {
            LOG(ERROR) << __func__ << mLogPrefixOEM << " Invalid ECNR type";
            pECNR_ProcessData.ecnr_type = INVALID;
            bECNR_Enable = false;
            goto skip_ecnr_configuration;
        }
        pECNR_ProcessData.scd_type = mAudExt.mHalExtension->audio_extn_getSCDtype(ecnrSampleRate, vocoder_rate, pECNR_ProcessData.ecnr_type, conn_type, DIR_UL);
        if (mAudExt.mHalExtension->audio_extn_getSCDdata(&pECNR_ProcessData, DIR_UL, &mScd_file_path_index_ul)) {
            LOG(ERROR) << __func__ << mLogPrefixOEM << " failed to get scd information, disabling ecnr processing";
            bECNR_Enable = false;
            goto skip_ecnr_configuration;
        }

        size_t channelCount =  ECNR_MIC_EC_CH;
        size_t bytesPerSample = getPcmSampleSizeInBytes(mMixPortConfig.format.value().pcm);
        mECNRFrameSizeBytes = channelCount * bytesPerSample;

        ecnr_out_buffer_size = ecnrPeriodSize * mFrameSizeBytes;
//        LOG(DEBUG) << __func__ << mLogPrefixOEM << " ecnr_out_buffer alloc " << ecnr_out_buffer_size << " for output ch "<< mChannels;
        ecnr_out_buffer = std::make_unique<uint8_t[]>(ecnr_out_buffer_size);
        if (!ecnr_out_buffer) {
            LOG(ERROR) << __func__ << mLogPrefixOEM << " failed to allocate ecnr_out_buffer";
            bECNR_Enable = false;
            goto skip_ecnr_configuration;
        }

        ecnr_in_buffer_size = ecnrPeriodSize * mECNRFrameSizeBytes;
        ecnr_in_buffer = std::make_unique<uint8_t[]>(ecnr_in_buffer_size);
        if (!ecnr_in_buffer) {
            LOG(ERROR) << __func__ << mLogPrefixOEM << " failed to allocate ecnr_in_buffer";
            bECNR_Enable = false;
            goto skip_ecnr_configuration;
        }

        ecnr_ecmx_buffer_size = ecnr_in_buffer_size;
        ecnr_ecmx_buffer = std::make_unique<uint8_t[]>(ecnr_ecmx_buffer_size);
        if (!ecnr_ecmx_buffer) {
            LOG(ERROR) << __func__ << mLogPrefixOEM << " failed to allocate ecnr_ecmx_buffer";
            bECNR_Enable = false;
            goto skip_ecnr_configuration;
        }

        mAudExt.mHalExtension->audio_extn_setupIOBuffer(&pECNR_ProcessData, DIR_UL, ECNR_MIC_EC_CH, mChannels, ecnrPeriodSize, ecnr_in_buffer.get(), ecnr_out_buffer.get());
        ret = mAudExt.mHalExtension->audio_extn_setupECNR(&pECNR_ProcessData);
        if (ret) {
            LOG(ERROR) << __func__ << mLogPrefixOEM << " audio_extn_setupECNR failed ret" << ret;
            bECNR_Enable = false;
            goto skip_ecnr_configuration;
        }
#ifdef ECNR_HAL_TUNE
        else
        mAudExt.mHalExtension->audio_extn_setupECNR_TuneIF(&pECNR_TuneIFData, portid);
#endif
#ifdef ECNR_HAL_SRC_CP
        if (mMixPortConfig.sampleRate.value().value != ecnrSampleRate) {
             ret = create_resampler(ecnrSampleRate,
                                   mMixPortConfig.sampleRate.value().value,
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
            mInHalRingBuffer = std::make_unique<HalRingBuffer<int16_t>>(ecnrPeriodSize*mChannels*3);
            if (!mInHalRingBuffer)
            {
                LOG(ERROR) << __func__ << " Pointer is null after allocation attempt";
                ret = ENOMEM_BUFFER;
                bECNR_Enable = false;
                goto skip_ecnr_configuration;
            }
        }
        attr->in_media_config.sample_rate = ecnrSampleRate;
        LOG(ERROR) << __func__ << " sample rate attr : " << attr->in_media_config.sample_rate;
#endif
        std::unique_ptr<pal_channel_info> palECMXChannelInfo = PlatformConverter::getPalChannelInfoForChannelCount(ECNR_MIC_EC_CH);
        attr->in_media_config.ch_info = *(palECMXChannelInfo);
    }

skip_ecnr_configuration :
    LOG(INFO) << __func__ << mLogPrefixOEM << " assigned pal stream type:" << attr->type;

    if (bECNR_Enable) {
        if (!palDevices.size()) {
            LOG(ERROR) << __func__ << mLogPrefixOEM << " no connected devices on stream!!";
            return;
        }
        uint64_t cookie = reinterpret_cast<uint64_t>(this);
        pal_stream_callback palFn = nullptr;

        mPlatform.configurePalDevicesCustomKey(palDevices, "ecmx");
        LOG(DEBUG) << __func__ << ": setting custom key as ecmx";

        const auto palOpenApiStartTime = std::chrono::steady_clock::now();
        if (int32_t ret = ::pal_stream_open(attr.get(), palDevices.size(), palDevices.data(), 0,
                                            nullptr, palFn, cookie, &(mPalHandle));
            ret) {
            LOG(ERROR) << __func__ << mLogPrefixOEM << " pal_stream_open failed!!! ret:" << ret;
            mPalHandle = nullptr;
            return;
        }
        const auto palOpenApiEndTime = std::chrono::steady_clock::now();

        const size_t ringBufSizeInBytes = ecnr_in_buffer_size;
        const size_t ringBufCount = bufConfig.bufferCount;
        auto palBufferConfig = mPlatform.getPalBufferConfig(ringBufSizeInBytes, ringBufCount);
        LOG(DEBUG) << __func__ << mLogPrefixOEM << " set pal_stream_set_buffer_size to " << ringBufSizeInBytes
                     << " with count " << ringBufCount;
        if (int32_t ret = ::pal_stream_set_buffer_size(mPalHandle, palBufferConfig.get(), nullptr);
            ret) {
            LOG(ERROR) << __func__ << mLogPrefixOEM << " pal_stream_set_buffer_size failed!!! ret:" << ret;
            ::pal_stream_close(mPalHandle);
            mPalHandle = nullptr;
            return;
        }
        LOG(VERBOSE) << __func__ << mLogPrefixOEM << " pal_stream_set_buffer_size successful";

        const auto palStartApiStartTime = std::chrono::steady_clock::now();
        if (int32_t ret = ::pal_stream_start(this->mPalHandle); ret) {
            LOG(ERROR) << __func__ << mLogPrefixOEM << " pal_stream_start failed!! ret:" << ret;
            ::pal_stream_close(mPalHandle);
            mPalHandle = nullptr;
            return;
        }

        if (mPlatform.getMicMuteStatus()) {
            setStreamMicMute(true);
        }

        const auto palStartApiEndTime = std::chrono::steady_clock::now();

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


        LOG(DEBUG) << __func__ << mLogPrefixOEM << " : stream is configured";

#ifdef PCM_DUMP_HAL_ENABLE
        if (property_get_bool("vendor.audio.feature.pcm_dump.in", false)) {
            pcm_dump = true;
            LOG(DEBUG) << __func__ << mLogPrefixOEM << ": pcm_dump enabled";
            char dump_file[128];
            snprintf(dump_file, sizeof(dump_file), "%s/%s_%s_in.raw",AUDIO_HAL_DUMP_PATH,mTagName.c_str(),attr->bus_addr);
            if (fp_in_dump) {
                fclose(fp_in_dump);
            }
            fp_in_dump=fopen(dump_file,"wb+");
            if (!fp_in_dump) {
                LOG(DEBUG) << __func__ << mLogPrefixOEM << ": opening error " << dump_file;
                  fp_in_dump = NULL;
            }
            if (fp_out_dump) {
                fclose(fp_in_dump);
            }
            snprintf(dump_file, sizeof(dump_file), "%s/%s_%s_out.raw",AUDIO_HAL_DUMP_PATH,mTagName.c_str(),attr->bus_addr);
            fp_out_dump=fopen(dump_file,"wb+");
            if (!fp_in_dump) {
                LOG(DEBUG) << __func__ << mLogPrefixOEM << ": opening error " << dump_file;
                 fp_out_dump = NULL;
            }
        }
#endif
    } else {
        StreamInPrimary::configure();
    }
}


void StreamInPrimaryOEM::shutdown_I() {
    StreamInPrimary::shutdown_I();
    LOG(INFO) << __func__ << mLogPrefixOEM << " Enter";

    if (bECNR_Enable && (mTag == Usecase::VOIP_RECORD)) {
        mAudExt.mHalExtension->set_vocoder_rate(INVALID);
        mAudExt.mHalExtension->set_conn_type(INVALID);
        mAudExt.mHalExtension->set_cp_type(INVALID);
    }
    bECNR_Enable = false;
    property_set("vendor.audio.ecnr.scd.ul", "");


    if (ecnr_ecmx_buffer) {
        ecnr_ecmx_buffer.reset();
    }
    ecnr_ecmx_buffer_size = 0;

    if (ecnr_out_buffer) {
        ecnr_out_buffer.reset();
    }
        ecnr_out_buffer_size = 0;
    if (ecnr_in_buffer) {
        ecnr_in_buffer.reset();
    }
    ecnr_in_buffer_size = 0;
#ifdef ECNR_HAL_SRC_CP
    if (mInHalRingBuffer) {
        mInHalRingBuffer.reset();
    }
#endif
    if (pECNR_ProcessData.scd_buffer[0]) {
        free(pECNR_ProcessData.scd_buffer[0]);
        pECNR_ProcessData.scd_buffer[0] = NULL;
    }
    pECNR_ProcessData.scd_buffer_size[0] = 0;
    if (pECNR_ProcessData.scd_buffer[1]) {
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

::android::status_t StreamInPrimaryOEM::onReadErrorOEM(const size_t sleepFrameCount) {
    shutdown_I();
    if (mTag == Usecase::COMPRESS_CAPTURE) {
        LOG(ERROR) << __func__ << mLogPrefixOEM << ": cannot afford read failure for compress";
        return ::android::UNEXPECTED_NULL;
    }
    auto& sampleRate = mMixPortConfig.sampleRate.value().value;
    if (sampleRate == 0) {
        LOG(ERROR) << __func__ << mLogPrefixOEM << ": cannot afford read failure, sampleRate is zero";
        return ::android::UNEXPECTED_NULL;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds((sleepFrameCount * 1000) / sampleRate));
    return ::android::OK;
}

struct BufferConfig StreamInPrimaryOEM::getBufferConfigOEM() {
    return mPlatform.getBufferConfig(mMixPortConfig, mTag);
}


} // namespace qti::audio::core
