/*
 * Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of The Linux Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Changes from Qualcomm Innovation Center are provided under the following license:
 *
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL_FM_QTI"
//#define LOG_NDDEBUG 0

#include <android-base/logging.h>
#include <cutils/properties.h>
#include <cutils/str_parms.h>
#include <errno.h>
#include <log/log.h>
#include <math.h>
#include <unistd.h>
#include "PalApi.h"

#ifdef DYNAMIC_LOG_ENABLED
#include <log_xml_parser.h>
#define LOG_MASK HAL_MOD_FILE_FM
#include <log_utils.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_PARAMETER_KEY_HANDLE_FM "handle_fm"
#define AUDIO_PARAMETER_KEY_FM_VOLUME "fm_volume"
#define AUDIO_PARAMETER_KEY_REC_PLAY_CONC "rec_play_conc_on"
#define AUDIO_PARAMETER_KEY_FM_MUTE "fm_mute"
#define AUDIO_PARAMETER_KEY_FM_RESTORE_VOLUME "fm_restore_volume"
#define AUDIO_PARAMETER_KEY_FM_ROUTING "fm_routing"
#define AUDIO_PARAMETER_KEY_FM_STATUS "fm_status"
#define FM_LOOPBACK_DRAIN_TIME_MS 2

#define CHANNELS 2
#define BIT_WIDTH 16
#define SAMPLE_RATE 48000

struct fm_module {
    bool running;
    bool muted;
    bool restart;
    float volume;
    // audio_devices_t device;
    pal_stream_handle_t *stream_handle;
};

static struct fm_module fm = {.running = 0,
                              .muted = 0,
                              .restart = 0,
                              .volume = 0,
                              //.device = (audio_devices_t)0,
                              .stream_handle = 0};

int32_t fm_set_volume(float value, bool persist = false) {
    int32_t ret = 0;
    struct pal_volume_data *pal_volume = NULL;

    LOG(DEBUG) << __func__ << " Enter: volume = " << value << " persist: " << persist;

    if (value < 0.0) {
        LOG(DEBUG) << __func__ << " Under 0.0, assuming 0.0" << value;
        value = 0.0;
    } else if (value > 1.0) {
        LOG(DEBUG) << __func__ << " Over 0.0, assuming 0.0" << value;
        value = 1.0;
    }

    if (persist) fm.volume = value;

    if (fm.muted && value > 0) {
        LOG(DEBUG) << __func__ << " fm is muted, applying '0' volume instead of " << value;
        value = 0;
    }

    if (!fm.running) {
        LOG(VERBOSE) << __func__ << " FM not active, ignoring set_volume call";
        return -EIO;
    }

    LOG(DEBUG) << __func__ << " Setting FM volume to " << value;

    pal_volume = (struct pal_volume_data *)malloc(sizeof(struct pal_volume_data) +
                                                  sizeof(struct pal_channel_vol_kv));

    if (!pal_volume) return -ENOMEM;

    pal_volume->no_of_volpair = 1;
    pal_volume->volume_pair[0].channel_mask = 0x03;
    pal_volume->volume_pair[0].vol = value;

    ret = pal_stream_set_volume(fm.stream_handle, pal_volume);
    if (ret) LOG(ERROR) << __func__ << " set volume failed: " << ret;

    free(pal_volume);
    LOG(DEBUG) << __func__ << " exit";
    return ret;
}

int32_t fm_start(int device_id) {
    int32_t ret = 0;
    const int num_pal_devs = 2;
    struct pal_stream_attributes stream_attr;
    struct pal_channel_info ch_info;
    struct pal_device pal_devs[num_pal_devs];
    pal_device_id_t pal_device_id = PAL_DEVICE_OUT_SPEAKER;

    LOG(DEBUG) << __func__ << " Enter";

    if (device_id == 2) // AUDIO_DEVICE_OUT_SPEAKER)
        pal_device_id = PAL_DEVICE_OUT_SPEAKER;
    else if (device_id == 4) // AUDIO_DEVICE_OUT_WIRED_HEADSET)
        pal_device_id = PAL_DEVICE_OUT_WIRED_HEADSET;
    else if (device_id == 8) // AUDIO_DEVICE_OUT_WIRED_HEADPHONE)
        pal_device_id = PAL_DEVICE_OUT_WIRED_HEADPHONE;
    else {
        LOG(DEBUG) << __func__ << " Unsupported device_id " << device_id;
        return -EINVAL;
    }

    ch_info.channels = CHANNELS;
    ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;
    ch_info.ch_map[1] = PAL_CHMAP_CHANNEL_FR;

    stream_attr.type = PAL_STREAM_LOOPBACK;
    stream_attr.info.opt_stream_info.loopback_type = PAL_STREAM_LOOPBACK_FM;
    stream_attr.direction = PAL_AUDIO_INPUT_OUTPUT;
    stream_attr.in_media_config.sample_rate = SAMPLE_RATE;
    stream_attr.in_media_config.bit_width = BIT_WIDTH;
    stream_attr.in_media_config.ch_info = ch_info;
    stream_attr.in_media_config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    stream_attr.out_media_config.sample_rate = SAMPLE_RATE;
    stream_attr.out_media_config.bit_width = BIT_WIDTH;
    stream_attr.out_media_config.ch_info = ch_info;
    stream_attr.out_media_config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    for (int i = 0; i < 2; ++i) {
        // TODO: remove hardcoded device id & pass adev to getPalDeviceIds instead
        pal_devs[i].id = i ? PAL_DEVICE_IN_FM_TUNER : pal_device_id;
        pal_devs[i].config.sample_rate = SAMPLE_RATE;
        pal_devs[i].config.bit_width = BIT_WIDTH;
        pal_devs[i].config.ch_info = ch_info;
        pal_devs[i].config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;
    }

    ret = pal_stream_open(&stream_attr, num_pal_devs, pal_devs, 0, NULL, NULL, 0,
                          &fm.stream_handle);

    if (ret) {
        LOG(ERROR) << __func__ << " stream open failed with: " << ret;
        return ret;
    }

    ret = pal_stream_start(fm.stream_handle);
    if (ret) {
        LOG(ERROR) << __func__ << " stream start failed with: " << ret;
        pal_stream_close(fm.stream_handle);
        return ret;
    }

    fm.running = true;
    fm_set_volume(fm.volume, true);
    LOG(DEBUG) << __func__ << " Exit";
    return ret;
}

int32_t fm_stop() {
    LOG(DEBUG) << __func__ << " enter";

    if (!fm.running) {
        LOG(ERROR) << __func__ << " FM not in running state...";
        return -EINVAL;
    }

    if (fm.stream_handle) {
        pal_stream_stop(fm.stream_handle);
        pal_stream_close(fm.stream_handle);
    }
    fm.stream_handle = NULL;
    fm.running = false;
    LOG(DEBUG) << __func__ << " Exit";
    return 0;
}

bool fm_get_running_status() {
    LOG(DEBUG) << __func__ << " enter";
    return fm.running;
    LOG(DEBUG) << __func__ << " Exit";
}

void fm_set_parameters(struct str_parms *parms) {
    int ret, val, num_pal_devs;
    pal_device_id_t *pal_devs;
    char value[32] = {0};
    float vol = 0.0;

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_HANDLE_FM, value, sizeof(value));
    if (ret >= 0) {
        val = atoi(value);
        LOG(DEBUG) << __func__ << " FM usecase";
        if (val) {
            if (val & 0x00100000 /*AUDIO_DEVICE_OUT_FM*/ && !fm.running)
                fm_start(val & ~(0x00100000) /*AUDIO_DEVICE_OUT_FM*/);
            else if (!(val & 0x00100000 /*AUDIO_DEVICE_OUT_FM*/) && fm.running) {
                fm_set_volume(0, false);
                usleep(FM_LOOPBACK_DRAIN_TIME_MS * 1000);
                fm_stop();
            }
        }
    }

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_FM_ROUTING, value, sizeof(value));
    if (ret >= 0 && fm.running) {
        val = atoi(value);
        LOG(DEBUG) << __func__ << " FM usecase";
        if (val && (val & 0x00100000 /*AUDIO_DEVICE_OUT_FM*/)) {
            fm_set_volume(0, false);
            fm_stop();
            fm_start(val & ~(0x00100000) /*AUDIO_DEVICE_OUT_FM*/);
        }
    }
    memset(value, 0, sizeof(value));

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_FM_VOLUME, value, sizeof(value));
    if (ret >= 0) {
        LOG(DEBUG) << __func__ << " Param: set volume";
        if (sscanf(value, "%f", &vol) != 1) {
            LOG(ERROR) << __func__ << " error in retrieving fm volume";
            return;
        }
        fm_set_volume(vol, true);
    }

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_FM_MUTE, value, sizeof(value));
    if (ret >= 0) {
        LOG(DEBUG) << __func__ << " Param: mute";
        fm.muted = (value[0] == '1');
        if (fm.muted)
            fm_set_volume(0);
        else
            fm_set_volume(fm.volume);
    }

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_FM_RESTORE_VOLUME, value, sizeof(value));
    if (ret >= 0) {
        LOG(DEBUG) << __func__ << " Param: restore volume";
        if (value[0] == '1') fm_set_volume(fm.volume);
    }

}

#ifdef __cplusplus
}
#endif
