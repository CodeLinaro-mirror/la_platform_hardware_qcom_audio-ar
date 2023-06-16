/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
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
 */

#define LOG_TAG "AHAL: icc"
#define LOG_NDDEBUG 0

#include <errno.h>
#include <math.h>
#include "AudioCommon.h"
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <math.h>
#include <cutils/properties.h>
#include "PalApi.h"
#include "AudioDevice.h"

#define AUDIO_PARAMETER_ICC_ENABLE               "conversation_mode_state"
#define AUDIO_PARAMETER_ICC_SET_SAMPLING_RATE    "icc_set_sampling_rate"
#define AUDIO_PARAMETER_KEY_ICC_VOLUME           "icc_volume"

#define CHANNELS                                 4
#define BIT_WIDTH                                16
#define SAMPLE_RATE                              48000
#define MAX_ICC_UI_BUFFER                        32

typedef enum {
    ICC_VOLUME_STEP_1 = 1,
    ICC_VOLUME_STEP_2,
    ICC_VOLUME_STEP_3,
    ICC_VOLUME_STEP_4,
    ICC_VOLUME_STEP_5,
    ICC_VOLUME_STEP_6,
    ICC_VOLUME_STEP_7,
    ICC_VOLUME_STEP_8,
    ICC_VOLUME_STEP_9,
    ICC_VOLUME_STEP_10,
    ICC_VOLUME_STEP_11,
    ICC_VOLUME_STEP_12,
    ICC_VOLUME_STEP_13,
    ICC_VOLUME_STEP_14,
    ICC_VOLUME_STEP_15,
} icc_vol_step_t;

#ifdef __cplusplus
extern "C" {
#endif

struct icc_module {
    bool is_icc_running;
    bool muted;
    uint32_t volume;
    pal_stream_handle_t* stream_handle;
};

static struct icc_module iccmod = {
    .is_icc_running = 0,
    .muted = 0,
    .volume = 0,
    .stream_handle = 0
};

static int32_t icc_set_volume(uint32_t value)
{
    int32_t ret = 0;
    float vol;
    struct pal_volume_data *pal_volume = NULL;

    AHAL_DBG("%s: Enter: volume = %u", __func__, value);

    if(!iccmod.is_icc_running) {
        AHAL_DBG("%s: ICC not active, ignoring icc_set_volume call", __func__);
        goto exit;
    }

    switch(value)
    {
        case ICC_VOLUME_STEP_1:
            vol = 0.001;
            break;

        case ICC_VOLUME_STEP_2:
            vol = 0.003;
            break;

        case ICC_VOLUME_STEP_3:
            vol = 0.005;
            break;

        case ICC_VOLUME_STEP_4:
            vol = 0.012;
            break;

        case ICC_VOLUME_STEP_5:
            vol = 0.02;
            break;

        case ICC_VOLUME_STEP_6:
            vol = 0.03;
            break;

        case ICC_VOLUME_STEP_7:
            vol = 0.05;
            break;

        case ICC_VOLUME_STEP_8:
            vol = 0.08;
            break;

        case ICC_VOLUME_STEP_9:
            vol = 0.12;
            break;

        case ICC_VOLUME_STEP_10:
            vol = 0.17;
            break;

        case ICC_VOLUME_STEP_11:
            vol = 0.24;
            break;

        case ICC_VOLUME_STEP_12:
            vol = 0.35;
            break;

        case ICC_VOLUME_STEP_13:
            vol = 0.45;
            break;

        case ICC_VOLUME_STEP_14:
            vol = 0.65;
            break;

        case ICC_VOLUME_STEP_15:
            vol = 0.85;
            break;

        default:
            vol = 0.0;

    }

    iccmod.volume = value;

    pal_volume = (struct pal_volume_data *)malloc(sizeof(struct pal_volume_data)
            +sizeof(struct pal_channel_vol_kv));

    if (!pal_volume)
       return -ENOMEM;

    pal_volume->no_of_volpair = 1;
    pal_volume->volume_pair[0].channel_mask = 0x03;
    pal_volume->volume_pair[0].vol = vol;

    ret = pal_stream_set_volume(iccmod.stream_handle, pal_volume);
    if (ret)
        AHAL_ERR("set volume failed: %d \n", ret);

    free(pal_volume);

exit:
    return ret;
}

bool icc_is_active(std::shared_ptr<AudioDevice> adev __unused)
{
    return iccmod.is_icc_running;
}

static int32_t icc_start(std::shared_ptr<AudioDevice> adev __unused,
        struct str_parms *parms __unused)
{
    int32_t ret = 0;
    const int num_pal_devs = 2;
    struct pal_stream_attributes stream_attr;
    struct pal_channel_info ch_info;
    struct pal_device pal_devs[num_pal_devs] = {};

    AHAL_VERBOSE("Enter icc_start");

    ch_info.channels = CHANNELS;
    ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;

    stream_attr.type = PAL_STREAM_LOOPBACK;
    stream_attr.info.opt_stream_info.loopback_type = PAL_STREAM_LOOPBACK_ICC;
    stream_attr.direction = PAL_AUDIO_INPUT_OUTPUT;
    stream_attr.in_media_config.sample_rate = SAMPLE_RATE;
    stream_attr.in_media_config.bit_width = BIT_WIDTH;
    stream_attr.in_media_config.ch_info = ch_info;
    stream_attr.in_media_config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    stream_attr.out_media_config.sample_rate = SAMPLE_RATE;
    stream_attr.out_media_config.bit_width = BIT_WIDTH;
    stream_attr.out_media_config.ch_info = ch_info;
    stream_attr.out_media_config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    pal_devs[0].id = PAL_DEVICE_OUT_SPEAKER;
    pal_devs[0].config.sample_rate = SAMPLE_RATE;
    pal_devs[0].config.bit_width = BIT_WIDTH;
    pal_devs[0].config.ch_info = ch_info;
    pal_devs[0].config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    pal_devs[1].id = PAL_DEVICE_IN_SPEAKER_MIC;
    pal_devs[1].config.sample_rate = SAMPLE_RATE;
    pal_devs[1].config.bit_width = BIT_WIDTH;
    pal_devs[1].config.ch_info = ch_info;
    pal_devs[1].config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    ret = pal_stream_open(&stream_attr,
            num_pal_devs, pal_devs,
            0,
            NULL,
            NULL,
            0,
            &iccmod.stream_handle);
    if (ret) {
        AHAL_ERR("stream open failed with: %d", ret);
        iccmod.stream_handle = NULL;
        return ret;
    }

    ret = pal_stream_start(iccmod.stream_handle);
    if (ret) {
        AHAL_ERR("stream start failed with %d", ret);
        pal_stream_close(iccmod.stream_handle);
        iccmod.stream_handle = NULL;
        return ret;
    }

    iccmod.is_icc_running = true;

    ret = icc_set_volume(iccmod.volume);
    if (ret) {
        AHAL_ERR("set volume failed: %d \n", ret);
        return ret;
    }

    return ret;
}

void icc_stop()
{
    AHAL_VERBOSE("enter %s", __func__);

    if(!iccmod.is_icc_running){
        AHAL_DBG("ICC not in running state...");
        return;
    }

    if (iccmod.stream_handle) {
        pal_stream_stop(iccmod.stream_handle);
        pal_stream_close(iccmod.stream_handle);
        iccmod.stream_handle = NULL;
    }

    iccmod.is_icc_running = false;
}

int icc_set_parameters(std::shared_ptr<AudioDevice> adev , struct str_parms *parms)
{
    int ret = 0;
    pal_device_id_t *pal_devs;
    char value[MAX_ICC_UI_BUFFER] = {0};
    uint32_t vol = 0;

    if (str_parms_get_str(parms, AUDIO_PARAMETER_ICC_ENABLE, value, sizeof(value)) >= 0) {
        if (!strncmp(value, "true", sizeof(value)) && !iccmod.is_icc_running) {
            ret = icc_start(adev, parms);
        }
        else if (!strncmp(value, "false", sizeof(value)) && iccmod.is_icc_running) {
            icc_stop();
        }
        else if (!strncmp(value, "true", sizeof(value)) && iccmod.is_icc_running) {
            AHAL_DBG("%s: icc is already enabled", __func__);
            return ret;
        }
        else {
            AHAL_ERR("icc_enable=%s is unsupported", value);
            return -1;
        }
    }

    memset(value, 0, sizeof(value));

    if (str_parms_get_str(parms, AUDIO_PARAMETER_KEY_ICC_VOLUME, value, sizeof(value)) >= 0) {

        AHAL_DBG("%s: Param: set ICC volume", __func__);

        if (sscanf(value, "%u", &vol) != 1){
            ALOGE("%s: error in retrieving icc volume", __func__);
            ret = -EIO;
            return ret;
        }

        AHAL_DBG("icc_set_volume usecase, Vol: [%u]", vol);

        ret = icc_set_volume(vol);
        if (ret) {
            AHAL_ERR("set volume failed: %d \n", ret);
            return ret;
        }
    }

    AHAL_DBG("%s: exit", __func__);

    return ret;
}

void icc_get_params(std::shared_ptr<AudioDevice> adev __unused, struct str_parms *query, struct str_parms *reply)
{
    int ret;
    char value[MAX_ICC_UI_BUFFER] = {0};

    AHAL_VERBOSE("%s: enter", __func__);

    if(query && reply){
        ret = str_parms_get_str(query, AUDIO_PARAMETER_ICC_ENABLE, value, sizeof(value));
        if (ret >= 0)
            str_parms_add_int(reply, AUDIO_PARAMETER_ICC_ENABLE, iccmod.is_icc_running);

        ret = str_parms_get_str(query, AUDIO_PARAMETER_KEY_ICC_VOLUME, value, sizeof(value));
        if (ret >= 0)
            str_parms_add_int(reply, AUDIO_PARAMETER_KEY_ICC_VOLUME, iccmod.volume);
    }

    AHAL_VERBOSE("%s: icc status: %s, volume: %u", __func__, iccmod.is_icc_running ? "enabled" : "disabled", iccmod.volume);
}
#ifdef __cplusplus
}
#endif
