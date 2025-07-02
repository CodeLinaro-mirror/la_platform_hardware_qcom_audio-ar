/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL_HFP_ECNR_QTI"
#define LOG_NDDEBUG 0

#include <android-base/logging.h>
#include <cutils/properties.h>
#include <cutils/str_parms.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "PalApi.h"

#include <pthread.h>
#include <extensions/hal_ecnr.h>
#include <extensions/AudioVolume.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_PARAMETER_HFP_ENABLE "hfp_enable"
#define AUDIO_PARAMETER_HFP_SET_SAMPLING_RATE "hfp_set_sampling_rate"
#define AUDIO_PARAMETER_KEY_HFP_VOLUME "hfp_volume"
#define AUDIO_PARAMETER_HFP_PCM_DEV_ID "hfp_pcm_dev_id"
#define MIN_VOLUME_GAIN_MB -9000
#define MAX_VOLUME_GAIN_MB 0
#define MAX_VOLUME_RANGE 9000

#define AUDIO_PARAMETER_KEY_HFP_MIC_VOLUME "hfp_mic_volume"

#ifdef ECNR_HAL_DUMP_ENABLE
#define FN_HFP_DL_TX AUDIO_HAL_DUMP_PATH"/HFP_DL_TX.raw"
#define FN_HFP_DL_RX AUDIO_HAL_DUMP_PATH"/HFP_DL_RX.raw"
#define FN_HFP_UL_TX AUDIO_HAL_DUMP_PATH"/HFP_UL_TX.raw"
#define FN_HFP_UL_RX AUDIO_HAL_DUMP_PATH"/HFP_UL_RX.raw"

FILE* fp_ul_tx = NULL;
FILE* fp_ul_rx = NULL;
FILE* fp_dl_tx = NULL;
FILE* fp_dl_rx = NULL;
#endif
struct hfp_module {
    bool is_hfp_running;
    float hfp_volume;
    float mic_volume;
    bool mic_mute;
    uint32_t sample_rate;
    pal_stream_handle_t *rx_stream_handle;
    pal_stream_handle_t *tx_stream_handle;
    std::unique_ptr<HalECNRExtension> mHalExtension;
};
struct hfp_loopback_module {
    bool is_hfp_running;
    float hfp_volume;
    float mic_volume;
    bool mic_mute;
    uint32_t sample_rate;
    pal_stream_handle_t *rx_ul_stream_handle;
    pal_stream_handle_t *tx_stream_handle;
    pal_stream_handle_t *rx_stream_handle;
    pal_stream_handle_t *tx_dl_stream_handle;
    pthread_mutex_t ul_thread_lock;
    pthread_mutex_t dl_thread_lock;
    pthread_t loopback_ul_thread;
    pthread_t loopback_dl_thread;
    void * UL_TX_stream_buffer;
    uint32_t UL_TX_stream_buffer_size;
    void * UL_RX_stream_buffer;
    uint32_t UL_RX_stream_buffer_size;
    void * UL_ECMX_stream_buffer;
    uint32_t UL_ECMX_stream_buffer_size;
    void * DL_TX_stream_buffer;
    uint32_t DL_TX_stream_buffer_size;
    void * DL_RX_stream_buffer;
    uint32_t DL_RX_stream_buffer_size;
    bool dl_thread_running;
    bool ul_thread_running;
    tECNR_ProcessData p_UL_ECNR_ProcessData;
    tECNR_ProcessData p_DL_ECNR_ProcessData;
#ifdef ECNR_HAL_TUNE
    tECNR_TuneIFData p_UL_ECNR_TuneIFData;
    tECNR_TuneIFData p_DL_ECNR_TuneIFData;
#endif
    bool bECNR_UL_Enable;
    bool bECNR_DL_Enable;
    bool hfp_pcm_dump;
    std::unique_ptr<HalECNRExtension> mHalExtension;
};
#define PLAYBACK_VOLUME_MAX 0x2000
#define CAPTURE_VOLUME_DEFAULT (15.0)
static struct hfp_loopback_module hfpmod = {
        .is_hfp_running = 0,
        .hfp_volume = 0,
        .mic_volume = CAPTURE_VOLUME_DEFAULT,
        .mic_mute = 0,
        .sample_rate = 16000,
        .mHalExtension = std::make_unique<HalECNRExtension>(),
};

#define HFP_16_BIT_FORMAT 16
#define HFP_16_BIT_FORMAT_BYTES 2
#define HFP_BUFFER_COUNT 4

int scd_file_path_index_dl = INVALID_PATH;
int scd_file_path_index_ul = INVALID_PATH;
int dl_period_size = 0;
int ul_period_size = 0;
static int32_t stop_hfp();
static int hfp_get_sampleRate_period_size_UL_processing(uint32_t sample_rate)
{
   int size = 0;
   switch(sample_rate) {
        case 16000:
            size = 256;
            break;
        case 8000:
            size = 128;
            break;
        default:
            size = 256;
            break;
   }
   return size;
}

static int hfp_get_sampleRate_period_size_DL_processing(uint32_t sample_rate)
{
   int size = 0;
   switch(sample_rate) {

        case 16000:
            size = 128;
            break;
        case 8000:
            size = 64;
            break;
        default:
            size = 128;
            break;
   }
   return size;
}
#ifdef ECNR_HAL_DUMP_ENABLE
static void open_hfp_pcm_dump() {

        if (fp_dl_tx != NULL) {
            fclose(fp_dl_tx);
            fp_dl_tx = NULL;
        }
        LOG(INFO) << __func__ << " opening FN_HFP_DL_TX";
        fp_dl_tx= fopen(FN_HFP_DL_TX, "wb+");
        if (!fp_dl_tx) {
            LOG(INFO) << __func__ << " error opening FN_HFP_DL_TX";
        }
        if (fp_dl_rx != NULL) {
            fclose(fp_dl_rx);
            fp_dl_rx = NULL;
        }
        LOG(INFO) << __func__ << " opening FN_HFP_DL_RX";
        fp_dl_rx= fopen(FN_HFP_DL_RX, "wb+");
        if (!fp_dl_rx) {
            LOG(INFO) << __func__ << " error opening FN_HFP_DL_RX";
        }
        if (fp_ul_tx != NULL) {
            fclose(fp_ul_tx);
            fp_ul_tx = NULL;
        }
        LOG(INFO) << __func__ << " opening FN_HFP_UL_TX";
        fp_ul_tx= fopen(FN_HFP_UL_TX, "wb+");
        if (!fp_ul_tx) {
            LOG(INFO) << __func__ << " error opening FN_HFP_UL_TX";
        }
        if (fp_ul_rx != NULL) {
            fclose(fp_ul_rx);
            fp_ul_rx = NULL;
        }
        LOG(INFO) << __func__ << " opening FN_HFP_UL_RX";
        fp_ul_rx= fopen(FN_HFP_UL_RX, "wb+");
        if (!fp_ul_rx) {
            LOG(INFO) << __func__ << " error opening FN_HFP_UL_RX";
        }
}
static void close_hfp_pcm_dump() {

    if (fp_dl_tx != NULL) {
        fclose(fp_dl_tx);
        fp_dl_tx = NULL;
    }

    if (fp_dl_rx != NULL) {
        fclose(fp_dl_rx);
        fp_dl_rx = NULL;
    }
    if (fp_ul_tx != NULL) {
        fclose(fp_ul_tx);
        fp_ul_tx = NULL;
    }

    if (fp_ul_rx != NULL) {
        fclose(fp_ul_rx);
        fp_ul_rx = NULL;
    }
}
#endif
static void *hfp_dl_thread(void *__unused) {
    int ret = 0;
    struct pal_buffer palBuffer;
    int16_t *src_buffer = (int16_t *)hfpmod.DL_TX_stream_buffer;
    int16_t *dst_buffer = (int16_t *)hfpmod.DL_RX_stream_buffer;
    palBuffer.buffer = (uint8_t *)hfpmod.DL_RX_stream_buffer;
    palBuffer.size = hfpmod.DL_RX_stream_buffer_size;
    palBuffer.offset = 0;

    while (hfpmod.dl_thread_running) {
        if (hfpmod.bECNR_DL_Enable) {
            palBuffer.buffer = (uint8_t *)hfpmod.DL_TX_stream_buffer;
            palBuffer.size = hfpmod.DL_TX_stream_buffer_size;
        }
        if (hfpmod.tx_dl_stream_handle && hfpmod.rx_stream_handle) {
            ret = pal_stream_read(hfpmod.tx_dl_stream_handle, &palBuffer);
            if (ret < 0) {
                LOG(ERROR) << __func__ << " error: pal_stream_read, do not add bytes when read fails";
            }
#ifdef ECNR_HAL_DUMP_ENABLE
            if (hfpmod.hfp_pcm_dump) {
                    if (fp_dl_tx) {
                        fwrite(palBuffer.buffer,sizeof(char),palBuffer.size,fp_dl_tx);
                    }
            }
#endif
            if (hfpmod.bECNR_DL_Enable) {
                //changing format is not required because the number of channel for input and output is 1
#ifdef ECNR_HAL_TUNE
                ret = hfpmod.mHalExtension->audio_extn_get_TuneIO_buffer(&(hfpmod.p_DL_ECNR_TuneIFData), &(hfpmod.p_DL_ECNR_ProcessData.sECNRTuneIO));
                if (ret >= 0)
                    ret = hfpmod.mHalExtension->audio_extn_ecnrProcess(hfpmod.p_DL_ECNR_ProcessData.pMain,hfpmod.p_DL_ECNR_ProcessData.audioIO,&(hfpmod.p_DL_ECNR_ProcessData.sECNRTuneIO));
                else {
#endif
                    ret = hfpmod.mHalExtension->audio_extn_ecnrProcess(hfpmod.p_DL_ECNR_ProcessData.pMain,hfpmod.p_DL_ECNR_ProcessData.audioIO,NULL);
                    if(ret) {
                    for(int path_index = scd_file_path_index_dl; path_index < MAX_SCD_PATH_INDEX; path_index++) {
                        if(scd_file_path_index_dl == DEFAULT_PATH) {
                            LOG(ERROR) << __func__ << " ECNR_Process failed ret = " << ret;
                            stop_hfp();
                        } else {
                           //Increamenting file path index to skip the used path
                            scd_file_path_index_dl++;
                            ret = hfpmod.mHalExtension->audio_extn_getSCDdata(&(hfpmod.p_DL_ECNR_ProcessData), DIR_DL, &scd_file_path_index_dl);
                            if(ret) {
                                LOG(ERROR) << __func__ << " failed to get scd information, ret " << ret;
                                stop_hfp();
                            }
                            hfpmod.mHalExtension->audio_extn_setupIOBuffer(&(hfpmod.p_DL_ECNR_ProcessData),DIR_DL,ECNR_IN_DL_CH,ECNR_OUT_DL_CH,dl_period_size,hfpmod.DL_TX_stream_buffer,hfpmod.DL_RX_stream_buffer);
                            ret = hfpmod.mHalExtension->audio_extn_setupECNR(&(hfpmod.p_DL_ECNR_ProcessData));
                            if (ret) {
                                LOG(ERROR) << __func__ << " audio_extn_setupECNR failed ret" << ret;
                                stop_hfp();
                            }
#ifdef ECNR_HAL_TUNE
                            hfpmod.mHalExtension->audio_extn_setupECNR_TuneIF(&(hfpmod.p_DL_ECNR_TuneIFData), ECNR_PORT_ID_HFP_DL_2012);
#endif
                        }
                            ret = hfpmod.mHalExtension->audio_extn_ecnrProcess(hfpmod.p_DL_ECNR_ProcessData.pMain,hfpmod.p_DL_ECNR_ProcessData.audioIO,NULL);
                            if(!ret) {
                                LOG(DEBUG) << __func__ << " ret = "<< ret;
                                break;
                            }
                        }
                    }
#ifdef ECNR_HAL_TUNE
                }
                hfpmod.mHalExtension->audio_extn_feedback_TuneIO_buffer(&(hfpmod.p_DL_ECNR_TuneIFData), &(hfpmod.p_DL_ECNR_ProcessData.sECNRTuneIO));
#endif
                if (!ret) {
                    palBuffer.buffer = (uint8_t *)hfpmod.DL_RX_stream_buffer;
                    palBuffer.size = hfpmod.DL_RX_stream_buffer_size;
                }
            }
#ifdef ECNR_HAL_DUMP_ENABLE
            if (hfpmod.hfp_pcm_dump) {
                if (fp_dl_rx) {
                    fwrite(palBuffer.buffer,sizeof(char),palBuffer.size,fp_dl_rx);
                }
            }
#endif
            ret = pal_stream_write(hfpmod.rx_stream_handle, &palBuffer);
           if (ret < 0) {
                LOG(ERROR) << __func__ << " error: pal_stream_write failed";
            }
        } else {
            LOG(ERROR) << __func__ << " error: pal_stream handles are not set, Ending DL thread";
            hfpmod.dl_thread_running = 0;
            return (void*)0;
        }
    }

    LOG(DEBUG) <<  __func__ << "End";
    return (void*)0;
}
static void *hfp_ul_thread(void *__unused) {

    int ret = 0;
    struct pal_buffer palBuffer;
    int16_t *src_buffer = (int16_t *)hfpmod.UL_ECMX_stream_buffer;
    int16_t *dst_buffer = (int16_t *)hfpmod.UL_RX_stream_buffer;
    int16_t *deint_buffer = (int16_t *)hfpmod.UL_TX_stream_buffer;

    palBuffer.buffer = (uint8_t *)hfpmod.UL_RX_stream_buffer;
    palBuffer.size = hfpmod.UL_RX_stream_buffer_size;
    palBuffer.offset = 0;

    int channels =1;
    int period_size = hfp_get_sampleRate_period_size_UL_processing(hfpmod.sample_rate);

    while (hfpmod.ul_thread_running) {
        if (hfpmod.bECNR_UL_Enable) {
            palBuffer.buffer = (uint8_t *)hfpmod.UL_ECMX_stream_buffer;
            palBuffer.size = hfpmod.UL_ECMX_stream_buffer_size;
        }
        if (hfpmod.tx_stream_handle && hfpmod.rx_ul_stream_handle) {
            ret = pal_stream_read(hfpmod.tx_stream_handle, &palBuffer);
           if (ret < 0) {
                LOG(ERROR) << __func__ << " error: pal_stream_read failed";
            }
#ifdef ECNR_HAL_DUMP_ENABLE
            if (hfpmod.hfp_pcm_dump) {
                if (fp_ul_tx) {
                    fwrite(palBuffer.buffer,sizeof(char),palBuffer.size,fp_ul_tx);
                }
            }
#endif
            if (hfpmod.bECNR_UL_Enable) {
                hfpmod.mHalExtension->audio_extn_cvtformat16_lnterleave_to_deinterleave(src_buffer,deint_buffer,period_size,ECNR_MIC_EC_CH);
#ifdef ECNR_HAL_TUNE
                ret = hfpmod.mHalExtension->audio_extn_get_TuneIO_buffer(&(hfpmod.p_UL_ECNR_TuneIFData), &(hfpmod.p_UL_ECNR_ProcessData.sECNRTuneIO));
                if (ret >= 0)
                    ret = hfpmod.mHalExtension->audio_extn_ecnrProcess(hfpmod.p_UL_ECNR_ProcessData.pMain,hfpmod.p_UL_ECNR_ProcessData.audioIO,&(hfpmod.p_UL_ECNR_ProcessData.sECNRTuneIO));
                else {
#endif
                    ret = hfpmod.mHalExtension->audio_extn_ecnrProcess(hfpmod.p_UL_ECNR_ProcessData.pMain, hfpmod.p_UL_ECNR_ProcessData.audioIO, NULL);
                    if(ret) {
                        for(int path_index = scd_file_path_index_ul; path_index < MAX_SCD_PATH_INDEX; path_index++) {
                            if(scd_file_path_index_ul == DEFAULT_PATH) {
                                LOG(ERROR) << __func__ << " ECNR_Process failed ret = " << ret;
                                stop_hfp();
                            } else {
                                //Increamenting file path index to skip the used path
                                scd_file_path_index_ul++;
                                ret = hfpmod.mHalExtension->audio_extn_getSCDdata(&(hfpmod.p_UL_ECNR_ProcessData), DIR_UL, &scd_file_path_index_ul);
                                if(ret) {
                                    LOG(ERROR) << __func__ << " failed to get scd information, ret " << ret;
                                    stop_hfp();
                                }
                                hfpmod.mHalExtension->audio_extn_setupIOBuffer(&(hfpmod.p_UL_ECNR_ProcessData),DIR_UL,ECNR_MIC_EC_CH,ECNR_OUT_UL_CH,ul_period_size,hfpmod.UL_TX_stream_buffer,hfpmod.UL_RX_stream_buffer);
                                ret = hfpmod.mHalExtension->audio_extn_setupECNR(&(hfpmod.p_UL_ECNR_ProcessData));
                                if (ret) {
                                    LOG(ERROR) << __func__ << " audio_extn_setupECNR failed ret " << ret;
                                    stop_hfp();
                                }
#ifdef ECNR_HAL_TUNE
                                hfpmod.mHalExtension->audio_extn_setupECNR_TuneIF(&(hfpmod.p_UL_ECNR_TuneIFData),ECNR_PORT_ID_HFP_UL_2013);
#endif
                            }
                            ret = hfpmod.mHalExtension->audio_extn_ecnrProcess(hfpmod.p_UL_ECNR_ProcessData.pMain, hfpmod.p_UL_ECNR_ProcessData.audioIO, NULL);
                            if(!ret) {
                                LOG(DEBUG) << __func__ << " ret = "<< ret;
                                break;
                            }
                        }
                    }
#ifdef ECNR_HAL_TUNE
                }
                hfpmod.mHalExtension->audio_extn_feedback_TuneIO_buffer(&(hfpmod.p_UL_ECNR_TuneIFData), &(hfpmod.p_UL_ECNR_ProcessData.sECNRTuneIO));
#endif

                if (ret) {
                    memcpy(dst_buffer,deint_buffer,period_size*HFP_16_BIT_FORMAT_BYTES);
                }
                palBuffer.buffer = (uint8_t *)hfpmod.UL_RX_stream_buffer;
                palBuffer.size = hfpmod.UL_RX_stream_buffer_size;
#ifdef ECNR_HAL_DUMP_ENABLE
                if (property_get_bool("vendor.audio.feature.ecnr.dump", false)) {
                    //dumping capture data from HAL
                    char dump_file[128];
                    FILE *fp_pcm_dump = NULL;
                    for(int i =0 ; i < ECNR_MIC_EC_CH; i++) {
                        snprintf(dump_file, sizeof(dump_file), "%s/HFP_UL_TX_deinterleave_%d_in.raw",AUDIO_HAL_DUMP_PATH,i);
                        fp_pcm_dump=fopen(dump_file,"a+");
                        if (fp_pcm_dump) {
                            fwrite(deint_buffer + i*period_size,sizeof(char),period_size*HFP_16_BIT_FORMAT_BYTES,fp_pcm_dump);
                            fclose(fp_pcm_dump);
                        } else {
                            LOG(ERROR) << __func__ << " error in opening dump file " << dump_file;
                        }
                    }
                }
#endif
            }

            ret = pal_stream_write(hfpmod.rx_ul_stream_handle, &palBuffer);
#ifdef ECNR_HAL_DUMP_ENABLE
            if (hfpmod.hfp_pcm_dump) {
                    if (fp_ul_rx) {
                        fwrite(palBuffer.buffer,sizeof(char),palBuffer.size,fp_ul_rx);
                    }
            }
#endif
           if (ret < 0) {
                LOG(ERROR) << __func__ << " error: pal_stream_write failed";
            }
        } else {
            LOG(ERROR) << __func__ << " error: pal_stream handles are not set, Ending UL thread";
            hfpmod.ul_thread_running = 0;
            return (void*)0;
        }
    }

    LOG(DEBUG) << __func__ << " End";
    return (void*)0;
 }
static void start_hfp_threads() {
    int err = 0;

    LOG(DEBUG) << __func__ << " Enter";

    pthread_mutex_init(&hfpmod.dl_thread_lock, NULL);
    pthread_mutex_lock(&hfpmod.dl_thread_lock);
    hfpmod.dl_thread_running = 1;
    err = pthread_create(&hfpmod.loopback_dl_thread,
                    (const pthread_attr_t *) NULL,
                    hfp_dl_thread,
                    NULL);
    LOG(DEBUG) << __func__ << " pthread_create loopback_dl_thread = "<< hfpmod.loopback_dl_thread;
    if (err) {
        LOG(ERROR) << __func__ << " error: failed to start DL thread";
        goto dl_thread_start_fail;
    }
    pthread_mutex_unlock(&hfpmod.dl_thread_lock);

    pthread_mutex_init(&hfpmod.ul_thread_lock, NULL);
    pthread_mutex_lock(&hfpmod.ul_thread_lock);
    hfpmod.ul_thread_running = 1;
    err = pthread_create(&hfpmod.loopback_ul_thread,
                    (const pthread_attr_t *) NULL,
                    hfp_ul_thread,
                    NULL);
    LOG(DEBUG) << __func__ << " pthread_create loopback_ul_thread = "<< hfpmod.loopback_ul_thread;
    if (err) {
        LOG(ERROR) << __func__ << " error: failed to start UL thread";
        goto ul_thread_start_fail;
    }
    pthread_mutex_unlock(&hfpmod.ul_thread_lock);
    LOG(DEBUG) << __func__ << " End without error";
    return;

ul_thread_start_fail :
    hfpmod.ul_thread_running = 0;
    pthread_mutex_unlock(&hfpmod.ul_thread_lock);
    err = pthread_join(hfpmod.loopback_ul_thread, NULL);
    if (err) {
        LOG(ERROR) << __func__ << " error: failed to ternimal UL thread";
    }
dl_thread_start_fail :
    hfpmod.ul_thread_running = 0;
    pthread_mutex_unlock(&hfpmod.dl_thread_lock);
    err = pthread_join(hfpmod.loopback_dl_thread, NULL);
    if (err) {
        LOG(ERROR) << __func__ << " error: failed to ternimal DL thread";
    }
    LOG(DEBUG) << __func__ << " End with error";
    return;

}
void stop_hfp_thread() {

    LOG(DEBUG) << __func__ << " Enter";
int err = 0;
    pthread_mutex_lock(&hfpmod.dl_thread_lock);
    if (hfpmod.dl_thread_running) {
        hfpmod.dl_thread_running = 0;
        if (hfpmod.loopback_dl_thread) {
            LOG(DEBUG) << __func__ << " pthread_join loopback_dl_thread = " << hfpmod.loopback_dl_thread;
            err = pthread_join(hfpmod.loopback_dl_thread, NULL);
        } else {
            LOG(ERROR) << __func__ << " loopback_dl_thread is already false";
        }
        if (err) {
            LOG(ERROR) << __func__ << " failed pthread_join loopback_dl_thread = " << hfpmod.loopback_dl_thread;
        }
    }
    pthread_mutex_unlock(&hfpmod.dl_thread_lock);
    pthread_mutex_destroy(&hfpmod.dl_thread_lock);
    pthread_mutex_lock(&hfpmod.ul_thread_lock);
    if (hfpmod.ul_thread_running) {
        hfpmod.ul_thread_running = 0;
        if (hfpmod.loopback_ul_thread) {
            LOG(DEBUG) << __func__ << " pthread_join loopback_ul_thread = " << hfpmod.loopback_ul_thread;
            err = pthread_join(hfpmod.loopback_ul_thread, NULL);
        } else {
            LOG(ERROR) << __func__ << " loopback_ul_thread is already false";
        }
        if (err) {
            LOG(ERROR) << __func__ << " failed pthread_join loopback_ul_thread = " << hfpmod.loopback_ul_thread;
        }
    }
    pthread_mutex_unlock(&hfpmod.ul_thread_lock);
    pthread_mutex_destroy(&hfpmod.ul_thread_lock);
    LOG(DEBUG) << __func__ << " End";
}
static int32_t hfp_set_volume(float value) {
    int32_t vol, ret = 0;
    struct pal_volume_data *pal_volume = NULL;

    LOG(VERBOSE) << __func__ << " entry";

    hfpmod.hfp_volume = ::qti::audio::oem::volume::AudioVolume::getInstance().getNearestAttenuation(value,UNMUTABLE_VOL_CURVE);

    LOG(DEBUG) << __func__ << " VALUE" << hfpmod.hfp_volume;
    if (!hfpmod.is_hfp_running) {
        LOG(VERBOSE) << __func__ << " HFP not active, ignoring set_hfp_volume call";
        return -EIO;
    }

    LOG(DEBUG) << __func__ << " Setting HFP volume to  " << value;

    pal_volume = (struct pal_volume_data *)malloc(sizeof(struct pal_volume_data) +
                                                  sizeof(struct pal_channel_vol_kv));

    if (!pal_volume) return -ENOMEM;

    pal_volume->no_of_volpair = 1;
    pal_volume->volume_pair[0].channel_mask = 0x03;
    pal_volume->volume_pair[0].vol = value;
    ret = pal_stream_set_volume(hfpmod.rx_stream_handle, pal_volume);
    if (ret) LOG(ERROR) << __func__ << " set volume failed:  " << ret;

    free(pal_volume);
    LOG(VERBOSE) << __func__ << " exit";
    return ret;
}

/*Set mic volume to value.
 *
 * This interface is used for mic volume control, set mic volume as value(range 0 ~ 15).
 *
*/
static int hfp_set_mic_volume(float value) {
    int volume, ret = 0;
    struct pal_volume_data *pal_volume = NULL;

    LOG(DEBUG) << __func__ << " enter value= " << value;

    if (!hfpmod.is_hfp_running) {
        LOG(ERROR) << __func__ << " HFP not active, ignoring set_hfp_mic_volume call";
        return -EIO;
    }

    if (value < 0.0) {
        LOG(DEBUG) << __func__ << " " << value << " Under 0.0, assuming 0.0";
        value = 0.0;
    } else if (value > CAPTURE_VOLUME_DEFAULT) {
        value = CAPTURE_VOLUME_DEFAULT;
        LOG(DEBUG) << __func__ << " Volume brought within range " << value;
    }

    value = value / CAPTURE_VOLUME_DEFAULT;

    volume = (int)(value * PLAYBACK_VOLUME_MAX);

    pal_volume = (struct pal_volume_data *)malloc(sizeof(struct pal_volume_data) +
                                                  sizeof(struct pal_channel_vol_kv));
    if (!pal_volume) {
        LOG(ERROR) << __func__ << " Failed to allocate memory for pal_volume";
        return -ENOMEM;
    }
    pal_volume->no_of_volpair = 1;
    pal_volume->volume_pair[0].channel_mask = 0x03;
    pal_volume->volume_pair[0].vol = value;
    if (pal_stream_set_volume(hfpmod.tx_stream_handle, pal_volume) < 0) {
        LOG(ERROR) << __func__ << " Couldn't set HFP Volume " << volume;
        free(pal_volume);
        pal_volume = NULL;
        return -EINVAL;
    }

    free(pal_volume);
    pal_volume = NULL;

    return ret;
}

static float hfp_get_mic_volume(void) {
    return hfpmod.mic_volume;
}

static int32_t start_hfp(struct str_parms *parms __unused) {
    int32_t ret = 0;
    uint32_t no_of_devices = 2;
    struct pal_stream_attributes stream_attr = {};
    struct pal_stream_attributes stream_dl_tx_attr = {};
    struct pal_stream_attributes stream_tx_attr = {};
    struct pal_stream_attributes stream_ul_rx_attr = {};
    struct pal_device devices[2] = {};
    struct pal_channel_info ch_info;
    struct pal_buffer_config outBufCfg = {0, 0, 0};
    struct pal_buffer_config inBufCfg = {0, 0, 0};
    ul_period_size = hfp_get_sampleRate_period_size_UL_processing(hfpmod.sample_rate);
    dl_period_size = hfp_get_sampleRate_period_size_DL_processing(hfpmod.sample_rate);
    LOG(DEBUG) << __func__ << " HFP start enter";
    if (hfpmod.rx_stream_handle || hfpmod.tx_stream_handle) return 0; // hfp already running;

    hfpmod.hfp_pcm_dump = property_get_bool("vendor.audio.feature.pcm_dump.hfp", false);
    if (property_get_bool(ECNR_FEATURE_PROP, false) && hfpmod.mHalExtension->audio_extn_getEnablement()) {
        hfpmod.bECNR_DL_Enable = true;
        hfpmod.bECNR_UL_Enable = true;
    } else {
        hfpmod.bECNR_DL_Enable = false;
        hfpmod.bECNR_UL_Enable = false;
    }

    LOG(DEBUG) << __func__ << " start HFP with ecnr " << hfpmod.bECNR_DL_Enable;
    pal_param_device_connection_t param_device_connection;

    param_device_connection.id = PAL_DEVICE_IN_HFP_DOWNLINK;
    param_device_connection.connection_state = true;
    ret = pal_set_param(PAL_PARAM_ID_DEVICE_CONNECTION, (void *)&param_device_connection,
                        sizeof(pal_param_device_connection_t));
    if (ret != 0) {
        LOG(ERROR) << __func__ << " Set PAL_PARAM_ID_DEVICE_CONNECTION for  "
                   << param_device_connection.id << " failed";
        return ret;
    }

    param_device_connection.id = PAL_DEVICE_OUT_HFP_UPLINK;
    param_device_connection.connection_state = true;
    ret = pal_set_param(PAL_PARAM_ID_DEVICE_CONNECTION, (void *)&param_device_connection,
                        sizeof(pal_param_device_connection_t));
    if (ret != 0) {
        LOG(ERROR) << __func__ << " Set PAL_PARAM_ID_DEVICE_CONNECTION for  "
                   << param_device_connection.id << " failed";
        return ret;
    }

    pal_param_btsco_t param_btsco;

    param_btsco.is_bt_hfp = true;
    param_btsco.bt_sco_on = true;
    ret = pal_set_param(PAL_PARAM_ID_BT_SCO, (void *)&param_btsco, sizeof(pal_param_btsco_t));
    if (ret != 0) {
        LOG(ERROR) << __func__ << " Set PAL_PARAM_ID_BT_SCO failed";
        return ret;
    }

    if (hfpmod.sample_rate == 16000) {
        param_btsco.bt_wb_speech_enabled = true;
    } else {
        param_btsco.bt_wb_speech_enabled = false;
    }

    ret = pal_set_param(PAL_PARAM_ID_BT_SCO_WB, (void *)&param_btsco, sizeof(pal_param_btsco_t));
    if (ret != 0) {
        LOG(ERROR) << __func__ << " Set PAL_PARAM_ID_BT_SCO_WB failed";
        return ret;
    }

    ch_info.channels = 1;
    ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;

    hfpmod.DL_RX_stream_buffer_size= dl_period_size*HFP_16_BIT_FORMAT_BYTES*ch_info.channels;
    hfpmod.DL_RX_stream_buffer = realloc(hfpmod.DL_RX_stream_buffer, hfpmod.DL_RX_stream_buffer_size);
    LOG(DEBUG) << __func__ << " HFP DL RX stream (BT SCO -> Spkr) buffer size " << hfpmod.DL_RX_stream_buffer_size;
    if (!hfpmod.DL_RX_stream_buffer) {
        LOG(ERROR) << __func__ << " failed to allocate DL_TX_stream_buffer";
        hfpmod.DL_RX_stream_buffer_size= 0;
        ret = -EINVAL;
        return ret;
    }
    hfpmod.UL_RX_stream_buffer_size= ul_period_size*HFP_16_BIT_FORMAT_BYTES*ch_info.channels;
    hfpmod.UL_RX_stream_buffer = realloc(hfpmod.UL_RX_stream_buffer, hfpmod.UL_RX_stream_buffer_size);
    LOG(DEBUG) << __func__ << " HFP UL RX stream (Mic -> BT SCO) buffer size " << hfpmod.UL_RX_stream_buffer_size;
    if (!hfpmod.UL_RX_stream_buffer) {
        LOG(ERROR) << __func__ << " failed to allocate UL_RX_stream_buffer";
        hfpmod.UL_RX_stream_buffer_size = 0;
        free(hfpmod.DL_RX_stream_buffer);
        hfpmod.DL_RX_stream_buffer = NULL;
        hfpmod.DL_RX_stream_buffer_size = 0;
        ret = -EINVAL;
        return ret;
    }
    if (hfpmod.bECNR_DL_Enable && hfpmod.bECNR_UL_Enable) {
        int scd_ret = 0;
        if (hfpmod.sample_rate == 8000) {
            hfpmod.p_DL_ECNR_ProcessData.scd_type = TEL_BT_NB_DL;
            scd_ret = hfpmod.mHalExtension->audio_extn_getSCDdata(&(hfpmod.p_DL_ECNR_ProcessData), DIR_DL, &scd_file_path_index_dl);
        } else {
            hfpmod.p_DL_ECNR_ProcessData.scd_type = TEL_BT_WB_DL;
            scd_ret = hfpmod.mHalExtension->audio_extn_getSCDdata(&(hfpmod.p_DL_ECNR_ProcessData), DIR_DL, &scd_file_path_index_dl);
        }
        if (scd_ret) {
            hfpmod.bECNR_DL_Enable = false;
        } else {
            hfpmod.DL_TX_stream_buffer_size= hfpmod.DL_RX_stream_buffer_size;
            hfpmod.DL_TX_stream_buffer = realloc(hfpmod.DL_TX_stream_buffer, hfpmod.DL_TX_stream_buffer_size);
            LOG(DEBUG) << __func__ << " HFP DL TX stream (BT SCO -> Spkr) buffer size " << hfpmod.DL_TX_stream_buffer_size;
            if (!hfpmod.DL_TX_stream_buffer) {
                LOG(ERROR) << __func__ << " failed to allocate DL_TX_stream_buffer";
                hfpmod.bECNR_DL_Enable = false;
                hfpmod.DL_TX_stream_buffer_size = 0;
                property_set("vendor.audio.ecnr.scd.dl", "");
            }
        }
        if (hfpmod.sample_rate == 8000) {
            hfpmod.p_UL_ECNR_ProcessData.scd_type = TEL_BT_NB_UL;
            scd_ret = hfpmod.mHalExtension->audio_extn_getSCDdata(&(hfpmod.p_UL_ECNR_ProcessData), DIR_UL, &scd_file_path_index_ul);
        } else {
            hfpmod.p_UL_ECNR_ProcessData.scd_type = TEL_BT_WB_UL;
            scd_ret = hfpmod.mHalExtension->audio_extn_getSCDdata(&(hfpmod.p_UL_ECNR_ProcessData), DIR_UL, &scd_file_path_index_ul);
        }
        if (scd_ret) {
            hfpmod.bECNR_UL_Enable = false;
        } else {
            hfpmod.UL_TX_stream_buffer_size= ul_period_size*HFP_16_BIT_FORMAT_BYTES*ECNR_MIC_EC_CH;
            hfpmod.UL_TX_stream_buffer = realloc(hfpmod.UL_TX_stream_buffer, hfpmod.UL_TX_stream_buffer_size);
            LOG(DEBUG) << __func__ << " HFP UL TX stream (Mic -> BT SCO) buffer size " << hfpmod.UL_TX_stream_buffer_size;
            if (!hfpmod.UL_TX_stream_buffer) {
                LOG(ERROR) << __func__ << " failed to allocate UL_TX_stream_buffer";
                hfpmod.bECNR_UL_Enable = false;
                hfpmod.UL_TX_stream_buffer_size =0;
                property_set("vendor.audio.ecnr.scd.ul", "");
                goto start_setup_paths;
            }
            hfpmod.UL_ECMX_stream_buffer_size= hfpmod.UL_TX_stream_buffer_size;
            hfpmod.UL_ECMX_stream_buffer = realloc(hfpmod.UL_ECMX_stream_buffer, hfpmod.UL_ECMX_stream_buffer_size);
            LOG(DEBUG) << __func__ << " HFP UL ECMX stream (Mic -> BT SCO) buffer size " << hfpmod.UL_ECMX_stream_buffer_size;
            if (!hfpmod.UL_ECMX_stream_buffer) {
                LOG(ERROR) << __func__ << " failed to allocate UL_ECMX_stream_buffer";
                hfpmod.bECNR_UL_Enable = false;
                hfpmod.UL_ECMX_stream_buffer_size = 0;
                free(hfpmod.UL_TX_stream_buffer);
                hfpmod.UL_TX_stream_buffer = NULL;
                hfpmod.UL_TX_stream_buffer_size = 0;
                property_set("vendor.audio.ecnr.scd.ul", "");
                goto start_setup_paths;
            }
        }
    }
start_setup_paths:
    if (hfpmod.bECNR_DL_Enable) {
        hfpmod.mHalExtension->audio_extn_setupIOBuffer(&(hfpmod.p_DL_ECNR_ProcessData),DIR_DL,ECNR_IN_DL_CH,ECNR_OUT_DL_CH,dl_period_size,hfpmod.DL_TX_stream_buffer,hfpmod.DL_RX_stream_buffer);
        ret = hfpmod.mHalExtension->audio_extn_setupECNR(&(hfpmod.p_DL_ECNR_ProcessData));
        if (ret) {
            hfpmod.bECNR_DL_Enable = false;
            property_set("vendor.audio.ecnr.scd.dl", "");
        }
#ifdef ECNR_HAL_TUNE
        else
        hfpmod.mHalExtension->audio_extn_setupECNR_TuneIF(&(hfpmod.p_DL_ECNR_TuneIFData), ECNR_PORT_ID_HFP_DL_2012);
#endif
    }
    if (hfpmod.bECNR_UL_Enable) {
        hfpmod.mHalExtension->audio_extn_setupIOBuffer(&(hfpmod.p_UL_ECNR_ProcessData),DIR_UL,ECNR_MIC_EC_CH,ECNR_OUT_UL_CH,ul_period_size,hfpmod.UL_TX_stream_buffer,hfpmod.UL_RX_stream_buffer);
        ret = hfpmod.mHalExtension->audio_extn_setupECNR(&(hfpmod.p_UL_ECNR_ProcessData));
        if (ret) {
            hfpmod.bECNR_UL_Enable = false;
            property_set("vendor.audio.ecnr.scd.ul", "");
        }
#ifdef ECNR_HAL_TUNE
        else
        hfpmod.mHalExtension->audio_extn_setupECNR_TuneIF(&(hfpmod.p_UL_ECNR_TuneIFData),ECNR_PORT_ID_HFP_UL_2013);
#endif
    }
    LOG(DEBUG) << __func__ << " start_setup_paths with ecnr DL " << hfpmod.bECNR_DL_Enable << " UL "<< hfpmod.bECNR_UL_Enable ;

    /* BT SCO -> Spkr */
    stream_dl_tx_attr.type = PAL_STREAM_CAPTURE_BUS;
    stream_dl_tx_attr.bus_addr = "BUSnn_HFP_DL_TX";
    stream_dl_tx_attr.direction = PAL_AUDIO_INPUT;
    stream_dl_tx_attr.in_media_config.sample_rate = hfpmod.sample_rate;
    stream_dl_tx_attr.in_media_config.bit_width = HFP_16_BIT_FORMAT;
    stream_dl_tx_attr.in_media_config.ch_info = ch_info;
    stream_dl_tx_attr.in_media_config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    devices[0].id = PAL_DEVICE_IN_HFP_DOWNLINK;
    devices[0].config.sample_rate = hfpmod.sample_rate;
    devices[0].config.bit_width = 16;
    devices[0].config.ch_info = {0, {0}};
    devices[0].config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    ret = pal_stream_open(&stream_dl_tx_attr,
            1, &devices[0],
            0,
            NULL,
            NULL,
            0,
            &hfpmod.tx_dl_stream_handle);
    if (ret != 0) {
        LOG(ERROR) << __func__ << " HFP downlink stream (BT SCO TX ) open failed rc " << ret;
        property_set("vendor.audio.ecnr.scd.dl", "");
        return ret;
    }
    inBufCfg.buf_size = hfpmod.DL_RX_stream_buffer_size;
    inBufCfg.buf_count = HFP_BUFFER_COUNT;
    LOG(DEBUG) << __func__ << " HFP DL tx stream (BT SCO -> Spkr) buffer size " << inBufCfg.buf_size << " count " << inBufCfg.buf_count;

    ret = pal_stream_set_buffer_size(hfpmod.tx_dl_stream_handle, &inBufCfg, NULL);
    if (ret) {
        LOG(ERROR) << __func__ << " Pal Stream set buffer size Error " << ret;
    }
    ret = pal_stream_start(hfpmod.tx_dl_stream_handle);
    if (ret != 0) {
        LOG(ERROR) << __func__ << " HFP downlink stream (BT SCO TX) start failed, rc " << ret;
        pal_stream_close(hfpmod.tx_dl_stream_handle);
        hfpmod.tx_dl_stream_handle=NULL;
        property_set("vendor.audio.ecnr.scd.dl", "");
        return ret;
    }
    stream_attr.type = PAL_STREAM_PLAYBACK_BUS;
    stream_attr.bus_addr = "BUSnn_HFP_DL_RX";
    stream_attr.direction = PAL_AUDIO_OUTPUT;
    stream_attr.out_media_config.sample_rate = hfpmod.sample_rate;
    stream_attr.out_media_config.bit_width = HFP_16_BIT_FORMAT;
    stream_attr.out_media_config.ch_info = ch_info;
    stream_attr.out_media_config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    devices[1].id = PAL_DEVICE_OUT_SPEAKER;
    devices[1].config.sample_rate = 48000;
    devices[1].config.bit_width = 16;
    devices[1].config.ch_info = {0, {0}};
    devices[1].config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    ret = pal_stream_open(&stream_attr,
            1, &devices[1],
            0,
            NULL,
            NULL,
            0,
            &hfpmod.rx_stream_handle);
    if (ret != 0) {
        LOG(ERROR) << __func__ << " HFP dwnlink stream (Spkr RX) open failed rc " << ret;
        pal_stream_stop(hfpmod.tx_dl_stream_handle);
        pal_stream_close(hfpmod.tx_dl_stream_handle);
        hfpmod.tx_dl_stream_handle=NULL;
        property_set("vendor.audio.ecnr.scd.dl", "");
        return ret;
    }
    outBufCfg.buf_size = hfpmod.DL_RX_stream_buffer_size;
    outBufCfg.buf_count = HFP_BUFFER_COUNT;
    LOG(DEBUG) << __func__ << " HFP DL rx stream (BT SCO -> Spkr) buffer size " << outBufCfg.buf_size << " count " << outBufCfg.buf_count;
    ret = pal_stream_set_buffer_size(hfpmod.rx_stream_handle, NULL, &outBufCfg);
    if (ret) {
        LOG(ERROR) << __func__ << " Pal Stream set buffer size Error rc " << ret;
    }
    ret = pal_stream_start(hfpmod.rx_stream_handle);
    if (ret != 0) {
        LOG(ERROR) << __func__ << " HFP dwnlink stream (Spkr RX) start failed, rc " << ret;
        pal_stream_close(hfpmod.rx_stream_handle);
        pal_stream_stop(hfpmod.tx_dl_stream_handle);
        pal_stream_close(hfpmod.tx_dl_stream_handle);
        pal_stream_close(hfpmod.rx_stream_handle);
        hfpmod.tx_dl_stream_handle=NULL;
        hfpmod.rx_stream_handle=NULL;
        property_set("vendor.audio.ecnr.scd.dl", "");
        property_set("vendor.audio.ecnr.scd.ul", "");
        return ret;
    }

    if (hfpmod.bECNR_UL_Enable) {
    /* Mic -> BT SCO */

        ch_info.channels = ECNR_MIC_EC_CH;
        ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;
        ch_info.ch_map[1] = PAL_CHMAP_CHANNEL_FR;
        ch_info.ch_map[2] = PAL_CHMAP_CHANNEL_C;
        ch_info.ch_map[3] = PAL_CHMAP_CHANNEL_LFE;
        ch_info.ch_map[4] = PAL_CHMAP_CHANNEL_LB;
        ch_info.ch_map[5] = PAL_CHMAP_CHANNEL_RB;
        strlcpy(devices[0].custom_config.custom_key, "ecmx", PAL_MAX_CUSTOM_KEY_SIZE);
    }

    stream_tx_attr.type = PAL_STREAM_CAPTURE_BUS;
    stream_tx_attr.bus_addr = "BUSnn_HFP_UL_TX";
    stream_tx_attr.direction = PAL_AUDIO_INPUT;
    stream_tx_attr.in_media_config.sample_rate = hfpmod.sample_rate;
    stream_tx_attr.in_media_config.bit_width = HFP_16_BIT_FORMAT;
    stream_tx_attr.in_media_config.ch_info = ch_info;
    stream_tx_attr.in_media_config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;
    devices[0].id = PAL_DEVICE_IN_HANDSET_MIC;
    devices[0].config.sample_rate = 48000;
    devices[0].config.bit_width = 16;
    devices[0].config.ch_info = {0, {0}};
    devices[0].config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;
    ret = pal_stream_open(&stream_tx_attr,
            1, &devices[0],
            0,
            NULL,
            NULL,
            0,
            &hfpmod.tx_stream_handle);
    if (ret != 0) {
        LOG(ERROR) << __func__ << " HFP uplink stream (Mic TX) open failed, rc " << ret;
        pal_stream_stop(hfpmod.tx_dl_stream_handle);
        pal_stream_close(hfpmod.tx_dl_stream_handle);
        pal_stream_stop(hfpmod.rx_stream_handle);
        pal_stream_close(hfpmod.rx_stream_handle);
        hfpmod.tx_dl_stream_handle = NULL;
        hfpmod.rx_stream_handle = NULL;
        property_set("vendor.audio.ecnr.scd.dl", "");
        property_set("vendor.audio.ecnr.scd.ul", "");
        return ret;
    }
    if (hfpmod.bECNR_UL_Enable) {
        inBufCfg.buf_size = hfpmod.UL_ECMX_stream_buffer_size;
    } else {
        inBufCfg.buf_size = hfpmod.UL_RX_stream_buffer_size;
    }
    inBufCfg.buf_count = HFP_BUFFER_COUNT;
    LOG(DEBUG) << __func__ << " HFP UL tx stream (Mic->BT SCO) buffer size " << inBufCfg.buf_size << " count " << inBufCfg.buf_count;
    ret = pal_stream_set_buffer_size(hfpmod.tx_stream_handle, &inBufCfg, NULL);
    if (ret) {
        LOG(ERROR) << __func__ << " Pal Stream set buffer size Error " << ret;
    }
    ret = pal_stream_start(hfpmod.tx_stream_handle);
    if (ret != 0) {
        LOG(ERROR) << __func__ << " HFP tx stream (Mic TX) start failed, rc  " << ret;
        pal_stream_stop(hfpmod.tx_dl_stream_handle);
        pal_stream_close(hfpmod.tx_dl_stream_handle);
        pal_stream_stop(hfpmod.rx_stream_handle);
        pal_stream_close(hfpmod.rx_stream_handle);
        pal_stream_close(hfpmod.tx_stream_handle);
        hfpmod.rx_stream_handle = NULL;
        hfpmod.tx_dl_stream_handle = NULL;
        hfpmod.tx_stream_handle = NULL;
        property_set("vendor.audio.ecnr.scd.dl", "");
        property_set("vendor.audio.ecnr.scd.ul", "");
        return ret;
    }

    memset(&ch_info,0, sizeof(ch_info));
    ch_info.channels = 1;
    ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;

    stream_ul_rx_attr.type = PAL_STREAM_PLAYBACK_BUS;
    stream_ul_rx_attr.bus_addr = "BUSnn_HFP_UL_RX";
    stream_ul_rx_attr.direction = PAL_AUDIO_OUTPUT;
    stream_ul_rx_attr.out_media_config.sample_rate = hfpmod.sample_rate;
    stream_ul_rx_attr.out_media_config.bit_width = HFP_16_BIT_FORMAT;
    stream_ul_rx_attr.out_media_config.ch_info = ch_info;
    stream_ul_rx_attr.out_media_config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;
    devices[1].id = PAL_DEVICE_OUT_HFP_UPLINK;
    devices[1].config.sample_rate = hfpmod.sample_rate;
    devices[1].config.bit_width = 16;
    devices[1].config.ch_info = {0, {0}};
    devices[1].config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;


    ret = pal_stream_open(&stream_ul_rx_attr,
            1, &devices[1],
            0,
            NULL,
            NULL,
            0,
            &hfpmod.rx_ul_stream_handle);
    if (ret != 0) {
        LOG(ERROR) << __func__ << " HFP tx stream (BT SCO RX) open failed, rc " << ret;
        pal_stream_stop(hfpmod.tx_dl_stream_handle);
        pal_stream_close(hfpmod.tx_dl_stream_handle);
        pal_stream_stop(hfpmod.rx_stream_handle);
        pal_stream_close(hfpmod.rx_stream_handle);
        pal_stream_stop(hfpmod.tx_stream_handle);
        pal_stream_close(hfpmod.tx_stream_handle);
        hfpmod.rx_stream_handle = NULL;
        hfpmod.tx_dl_stream_handle = NULL;
        hfpmod.tx_stream_handle = NULL;
        property_set("vendor.audio.ecnr.scd.dl", "");
        property_set("vendor.audio.ecnr.scd.ul", "");
        return ret;
    }
    outBufCfg.buf_size = hfpmod.UL_RX_stream_buffer_size;
    outBufCfg.buf_count = HFP_BUFFER_COUNT;
    LOG(DEBUG) << __func__ << " HFP UL rx stream (Mic->BT SCO) buffer size " << outBufCfg.buf_size << " count " << outBufCfg.buf_count;
    ret = pal_stream_set_buffer_size(hfpmod.rx_ul_stream_handle, NULL, &outBufCfg);
    if (ret) {
        LOG(ERROR) << __func__ << " Pal Stream set buffer size Error " << ret;
    }
    ret = pal_stream_start(hfpmod.rx_ul_stream_handle);
    if (ret != 0) {
        LOG(ERROR) << __func__ << " HFP tx stream (BT SCO RX) start failed, rc " << ret;
        pal_stream_stop(hfpmod.tx_dl_stream_handle);
        pal_stream_close(hfpmod.tx_dl_stream_handle);
        pal_stream_stop(hfpmod.rx_stream_handle);
        pal_stream_close(hfpmod.rx_stream_handle);
        pal_stream_stop(hfpmod.tx_stream_handle);
        pal_stream_close(hfpmod.tx_stream_handle);
        pal_stream_close(hfpmod.rx_ul_stream_handle);
        hfpmod.tx_dl_stream_handle = NULL;
        hfpmod.rx_stream_handle = NULL;
        hfpmod.tx_stream_handle = NULL;
        hfpmod.rx_ul_stream_handle= NULL;
        property_set("vendor.audio.ecnr.scd.dl", "");
        property_set("vendor.audio.ecnr.scd.ul", "");
        return ret;
    }
    hfpmod.mic_mute = false;
    hfpmod.is_hfp_running = true;
    hfp_set_volume(hfpmod.hfp_volume);
#ifdef ECNR_HAL_DUMP_ENABLE
    if (hfpmod.hfp_pcm_dump) {
        open_hfp_pcm_dump();
    }
#endif
    start_hfp_threads();
    LOG(DEBUG) << __func__ << " End";
    return ret;
}

static int32_t stop_hfp() {
    int32_t ret = 0;

    LOG(DEBUG) << __func__ << " HFP stop enter";
    if (hfpmod.is_hfp_running != true) {
        LOG(DEBUG) << __func__ << " Already cleanup happened";
        return 0;
    }
    hfpmod.is_hfp_running = false;
    stop_hfp_thread();
    if (hfpmod.rx_stream_handle) {
        pal_stream_stop(hfpmod.rx_stream_handle);
        pal_stream_close(hfpmod.rx_stream_handle);
        hfpmod.rx_stream_handle = NULL;
    }
    if (hfpmod.tx_dl_stream_handle) {
        pal_stream_stop(hfpmod.tx_dl_stream_handle);
        pal_stream_close(hfpmod.tx_dl_stream_handle);
        hfpmod.tx_dl_stream_handle = NULL;
    }
    if (hfpmod.tx_stream_handle) {
        pal_stream_stop(hfpmod.tx_stream_handle);
        pal_stream_close(hfpmod.tx_stream_handle);
        hfpmod.tx_stream_handle = NULL;
    }

    if (hfpmod.rx_ul_stream_handle) {
        pal_stream_stop(hfpmod.rx_ul_stream_handle);
        pal_stream_close(hfpmod.rx_ul_stream_handle);
        hfpmod.rx_ul_stream_handle = NULL;
    }

    property_set("vendor.audio.ecnr.scd.dl", "");
    property_set("vendor.audio.ecnr.scd.ul", "");
    pal_param_btsco_t param_btsco;

    param_btsco.is_bt_hfp = true;
    param_btsco.bt_sco_on = true;
    ret = pal_set_param(PAL_PARAM_ID_BT_SCO, (void *)&param_btsco, sizeof(pal_param_btsco_t));
    if (ret != 0) {
        LOG(DEBUG) << __func__ << " Set PAL_PARAM_ID_BT_SCO failed";
    }

    pal_param_device_connection_t param_device_connection;

    param_device_connection.id = PAL_DEVICE_IN_HFP_DOWNLINK;
    param_device_connection.connection_state = false;
    ret = pal_set_param(PAL_PARAM_ID_DEVICE_CONNECTION, (void *)&param_device_connection,
                        sizeof(pal_param_device_connection_t));
    if (ret != 0) {
        LOG(ERROR) << __func__ << " Set PAL_PARAM_ID_DEVICE_DISCONNECTION for  "
                   << param_device_connection.id << " failed";
    }

    param_device_connection.id = PAL_DEVICE_OUT_HFP_UPLINK;
    param_device_connection.connection_state = false;
    ret = pal_set_param(PAL_PARAM_ID_DEVICE_CONNECTION, (void *)&param_device_connection,
                        sizeof(pal_param_device_connection_t));
    if (ret != 0) {
        LOG(ERROR) << __func__ << " Set PAL_PARAM_ID_DEVICE_DISCONNECTION for  "
                   << param_device_connection.id << " failed";
    }
#ifdef ECNR_HAL_TUNE
    hfpmod.mHalExtension->audio_extn_close_TuneIF(&(hfpmod.p_UL_ECNR_TuneIFData));
    hfpmod.mHalExtension->audio_extn_close_TuneIF(&(hfpmod.p_DL_ECNR_TuneIFData));
#endif
    if (hfpmod.UL_TX_stream_buffer)
        free(hfpmod.UL_TX_stream_buffer);
    hfpmod.UL_TX_stream_buffer = NULL;
    hfpmod.UL_TX_stream_buffer_size = 0;

    if (hfpmod.UL_RX_stream_buffer)
        free(hfpmod.UL_RX_stream_buffer);
    hfpmod.UL_RX_stream_buffer = NULL;
    hfpmod.UL_RX_stream_buffer_size = 0;

    if (hfpmod.UL_ECMX_stream_buffer)
        free(hfpmod.UL_ECMX_stream_buffer);
    hfpmod.UL_ECMX_stream_buffer = NULL;
    hfpmod.UL_ECMX_stream_buffer_size = 0;

    if (hfpmod.DL_TX_stream_buffer)
        free(hfpmod.DL_TX_stream_buffer);
    hfpmod.DL_TX_stream_buffer = NULL;
    hfpmod.DL_TX_stream_buffer_size = 0;

    if (hfpmod.DL_RX_stream_buffer)
        free(hfpmod.DL_RX_stream_buffer);
    hfpmod.DL_RX_stream_buffer = NULL;
    hfpmod.DL_RX_stream_buffer_size = 0;

    if (hfpmod.p_UL_ECNR_ProcessData.scd_buffer[0]) {
        free(hfpmod.p_UL_ECNR_ProcessData.scd_buffer[0]);
        hfpmod.p_UL_ECNR_ProcessData.scd_buffer[0] = NULL;
    }
    hfpmod.p_UL_ECNR_ProcessData.scd_buffer_size[0] = 0;

    if (hfpmod.p_UL_ECNR_ProcessData.scd_buffer[1]) {
        free(hfpmod.p_UL_ECNR_ProcessData.scd_buffer[1]);
        hfpmod.p_UL_ECNR_ProcessData.scd_buffer[1] = NULL;
    }
    hfpmod.p_UL_ECNR_ProcessData.scd_buffer_size[1] = 0;

    if (hfpmod.p_DL_ECNR_ProcessData.scd_buffer[0]) {
        free(hfpmod.p_DL_ECNR_ProcessData.scd_buffer[0]);
        hfpmod.p_DL_ECNR_ProcessData.scd_buffer[0] = NULL;
    }
    hfpmod.p_DL_ECNR_ProcessData.scd_buffer_size[0] = 0;

    if (hfpmod.p_DL_ECNR_ProcessData.scd_buffer[1]) {
        free(hfpmod.p_DL_ECNR_ProcessData.scd_buffer[1]);
        hfpmod.p_DL_ECNR_ProcessData.scd_buffer[1] = NULL;
    }
    hfpmod.p_DL_ECNR_ProcessData.scd_buffer_size[1] = 0;

    hfpmod.mHalExtension->audio_extn_resetIOBuffer(&(hfpmod.p_DL_ECNR_ProcessData));
    hfpmod.mHalExtension->audio_extn_resetIOBuffer(&(hfpmod.p_UL_ECNR_ProcessData));

    memset(&(hfpmod.p_DL_ECNR_ProcessData.audioIO), 0, sizeof(tECNR_AudioIO));
    memset(&(hfpmod.p_UL_ECNR_ProcessData.audioIO), 0, sizeof(tECNR_AudioIO));
    memset(&(hfpmod.p_UL_ECNR_ProcessData.sECNRTuneIO), 0, sizeof(tECNR_TuneIO));
    memset(&(hfpmod.p_DL_ECNR_ProcessData.sECNRTuneIO), 0, sizeof(tECNR_TuneIO));
    hfpmod.mHalExtension->audio_extn_ecnrDestroy(&(hfpmod.p_UL_ECNR_ProcessData.pMain));
    hfpmod.mHalExtension->audio_extn_ecnrDestroy(&(hfpmod.p_DL_ECNR_ProcessData.pMain));
    hfpmod.p_UL_ECNR_ProcessData.pMain = NULL;
    hfpmod.p_DL_ECNR_ProcessData.pMain = NULL;
#ifdef ECNR_HAL_DUMP_ENABLE
    close_hfp_pcm_dump();
#endif
    LOG(DEBUG) << __func__ << " End";
    return ret;
}
void hfp_init() {
    return;
}

bool hfp_is_active() {
    return hfpmod.is_hfp_running;
}
bool is_valid_out_device(pal_device_id_t id) {
    switch (id) {
        case PAL_DEVICE_OUT_HANDSET:
        case PAL_DEVICE_OUT_SPEAKER:
        case PAL_DEVICE_OUT_WIRED_HEADSET:
        case PAL_DEVICE_OUT_WIRED_HEADPHONE:
        case PAL_DEVICE_OUT_USB_DEVICE:
        case PAL_DEVICE_OUT_USB_HEADSET:
            return true;
        default:
            return false;
    }
}
bool is_valid_in_device(pal_device_id_t id) {
    switch (id) {
        case PAL_DEVICE_IN_HANDSET_MIC:
        case PAL_DEVICE_IN_SPEAKER_MIC:
        case PAL_DEVICE_IN_WIRED_HEADSET:
        case PAL_DEVICE_IN_USB_DEVICE:
        case PAL_DEVICE_IN_USB_HEADSET:
            return true;
        default:
            return false;
    }
}

bool has_valid_stream_handle() {
    return (hfpmod.rx_stream_handle && hfpmod.tx_stream_handle);
}

void hfp_set_device(struct pal_device *devices) {
    int rc = 0;

    if (hfpmod.is_hfp_running && has_valid_stream_handle() &&
        is_valid_out_device(devices[0].id) && is_valid_in_device(devices[1].id)) {
        rc = pal_stream_set_device(hfpmod.rx_stream_handle, 1, &devices[0]);
        if (!rc) {
            rc = pal_stream_set_device(hfpmod.tx_stream_handle, 1, &devices[1]);
        }
    }

    if (rc) {
        LOG(ERROR) << __func__ << ": failed to set devices for hfp";
    }
    return;
}

/*Set mic mute state.
 * *
 * * This interface is used for mic mute state control
 * */
int hfp_set_mic_mute(bool state) {
    int rc = 0;

    if (state == hfpmod.mic_mute) {
        LOG(DEBUG) << __func__ << " mic mute already " << state;
        return rc;
    }
    rc = hfp_set_mic_volume((state == true) ? 0.0 : hfpmod.mic_volume);
    if (rc == 0) hfpmod.mic_mute = state;
    LOG(DEBUG) << __func__ << " Setting mute state  " << state << " rc " << rc;
    return rc;
}

int hfp_set_mic_mute2(bool state __unused) {
    LOG(DEBUG) << __func__ << " Unsupported";
    return 0;
}

void hfp_set_parameters(bool adev_mute, struct str_parms *parms) {
    int status = 0;
    char value[32] = {0};
    float vol;
    int val;
    int rate;

    LOG(DEBUG) << __func__ << " enter";
    status = str_parms_get_str(parms, AUDIO_PARAMETER_HFP_ENABLE, value, sizeof(value));
    if (status >= 0) {
        if (!strncmp(value, "true", sizeof(value)) && !hfpmod.is_hfp_running) {
            status = start_hfp(parms);
            /*
             * Sync to adev mic mute state if hfpmod.mic_mute state is lost due
             * to HFP session tear down during device switch on companion device.
             */
            if (hfpmod.mic_mute != adev_mute) {
                LOG(DEBUG) << __func__ << " update mic mute with latest mute state " << adev_mute;
                hfp_set_mic_mute(adev_mute);
            }
        } else if (!strncmp(value, "false", sizeof(value)) && hfpmod.is_hfp_running) {
            stop_hfp();
        } else {
            LOG(ERROR) << __func__ << " hfp_enable " << value << " is unsupported";
        }
    }

    memset(value, 0, sizeof(value));
    status = str_parms_get_str(parms, AUDIO_PARAMETER_HFP_SET_SAMPLING_RATE, value, sizeof(value));
    if (status >= 0) {
        rate = atoi(value);
        if (rate == 8000) {
            hfpmod.sample_rate = (uint32_t)rate;
        } else if (rate == 16000) {
            hfpmod.sample_rate = (uint32_t)rate;
        } else
            LOG(ERROR) << __func__ << " Unsupported rate.. " << rate;
    }

    memset(value, 0, sizeof(value));
    status = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_HFP_VOLUME, value, sizeof(value));
    if (status >= 0) {
        if (sscanf(value, "%f", &vol) != 1) {
            LOG(ERROR) << __func__ << " error in retrieving hfp volume";
            status = -EIO;
            goto exit;
        }
        LOG(DEBUG) << __func__ << " set_hfp_volume usecase, Vol: " << vol;
        hfp_set_volume(vol);
    }

    memset(value, 0, sizeof(value));
    status = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_HFP_MIC_VOLUME, value, sizeof(value));
    if (status >= 0) {
        if (sscanf(value, "%f", &vol) != 1) {
            LOG(ERROR) << __func__ << " error in retrieving hfp mic volume";
            status = -EIO;
            goto exit;
        }
        LOG(DEBUG) << __func__ << " set_hfp_mic_volume usecase, Vol: " << vol;
        if (hfp_set_mic_volume(vol) == 0) hfpmod.mic_volume = vol;
    }

exit:
    LOG(DEBUG) << __func__ << " Exit";
}

#ifdef __cplusplus
}
#endif
