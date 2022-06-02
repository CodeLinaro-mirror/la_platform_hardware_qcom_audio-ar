/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */


#define LOG_TAG "AHAL: BTSink"
#define LOG_NDDEBUG 0

#include <errno.h>
#include <math.h>
#include <log/log.h>
#include "PalApi.h"
#include "AudioDevice.h"
#include "AudioCommon.h"

#ifdef DYNAMIC_LOG_ENABLED
#include <log_xml_parser.h>
#define LOG_MASK HAL_MOD_FILE_BTSINK
#include <log_utils.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


#define CHANNELS 2
#define BIT_WIDTH 16
#define SAMPLE_RATE 48000

#define AUDIO_PARAMETER_BTSINK_ENABLE "btsink_enable"
#define AUDIO_PARAMETER_BTSINK_VOLUME "btsink_volume"

struct btsink_module {
    bool running;
    float volume;
    audio_devices_t device;
    pal_stream_handle_t* stream_handle;
};

static struct btsink_module btsink = {
    .running = 0,
    .volume = 0,
    .device = (audio_devices_t)0,
    .stream_handle = 0
};

int32_t btsink_set_volume(float value)
{
    int32_t vol, ret = 0;
    struct pal_volume_data *pal_volume = NULL;

    if (value < 0.0) {
        AHAL_DBG("(%f) Under 0.0, assuming 0.0\n", value);
        value = 0.0;
    } else {
        value = ((value > 15.000000) ? 1.0 : (value / 15));
        AHAL_DBG("Volume brought with in range (%f)\n", value);
    }
    vol  = lrint((value * 0x2000) + 0.5);


    if (!btsink.running) {
        AHAL_DBG(" BT Sink not active, ignoring set_volume call");
        return -EIO;
    }

    AHAL_DBG("Setting BT Sink volume to %f", value);

    pal_volume = (struct pal_volume_data *) calloc(1,sizeof(struct pal_volume_data) + sizeof(struct pal_channel_vol_kv));

    if (!pal_volume)
       return -ENOMEM;

    pal_volume->no_of_volpair = 1;
    pal_volume->volume_pair[0].channel_mask = 0x03;
    pal_volume->volume_pair[0].vol = value;

    ret = pal_stream_set_volume(btsink.stream_handle, pal_volume);
    if (ret)
        AHAL_ERR("set volume failed: %d", ret);

    free(pal_volume);
    AHAL_DBG("exit");
    return ret;
}


int32_t start_btsink(std::shared_ptr<AudioDevice> adev __unused, struct str_parms *parms __unused)
{
    struct pal_stream_attributes stream_attr;
    struct pal_channel_info ch_info;
    const int num_pal_devs = 2;
    struct pal_device pal_devs[num_pal_devs];
    int32_t ret = 0;

    ch_info.channels = CHANNELS;
    ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;
    ch_info.ch_map[1] = PAL_CHMAP_CHANNEL_FR;

    stream_attr.type = PAL_STREAM_LOOPBACK;
    stream_attr.info.opt_stream_info.loopback_type = PAL_STREAM_LOOPBACK_PCM;

    stream_attr.direction = PAL_AUDIO_INPUT_OUTPUT;
    stream_attr.in_media_config.sample_rate = SAMPLE_RATE;
    stream_attr.in_media_config.bit_width = BIT_WIDTH;
    stream_attr.in_media_config.ch_info = ch_info;
    stream_attr.in_media_config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    stream_attr.out_media_config.sample_rate = SAMPLE_RATE;
    stream_attr.out_media_config.bit_width = BIT_WIDTH;
    stream_attr.out_media_config.ch_info = ch_info;
    stream_attr.out_media_config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    pal_devs[0].id = PAL_DEVICE_IN_BLUETOOTH_A2DP;
    pal_devs[0].config.sample_rate = SAMPLE_RATE;
    pal_devs[0].config.bit_width = BIT_WIDTH;
    pal_devs[0].config.ch_info = ch_info;
    pal_devs[0].config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    pal_devs[1].id = PAL_DEVICE_OUT_SPEAKER;
    pal_devs[1].config.sample_rate = SAMPLE_RATE;
    pal_devs[1].config.bit_width = BIT_WIDTH;
    pal_devs[1].config.ch_info = ch_info;
    pal_devs[1].config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    ret = pal_stream_open(&stream_attr, num_pal_devs, pal_devs,
            0, NULL, NULL, 0, &btsink.stream_handle);
    if (ret != 0) {
        AHAL_ERR("BT A2DP sink rx stream open failed, rc %d", ret);
        return ret;
    }
    ret = pal_stream_start(btsink.stream_handle);
    if (ret != 0) {
        AHAL_ERR("BT A2DP sink rx stream start failed, rc %d", ret);
        pal_stream_close(btsink.stream_handle);
        return ret;
    }
    btsink.running = true;
    return ret;
}

int stop_btsink()
{
    int ret = 0;

    AHAL_DBG("enter");

    if (!btsink.running){
        AHAL_ERR("BT Sink not in running state...");
        return -EINVAL;
    }

    if (btsink.stream_handle) {
        pal_stream_stop(btsink.stream_handle);
        pal_stream_close(btsink.stream_handle);
    }

    btsink.stream_handle = NULL;
    btsink.running = false;
    AHAL_DBG("exit");
    return ret;
}
void btsink_set_parameters(std::shared_ptr<AudioDevice> adev, struct str_parms *parms)
{
    int ret;
    pal_device_id_t *pal_devs;
    char value[32] = {0};
    float vol = 0.0;

    AHAL_DBG("Enter");

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_BTSINK_ENABLE,
                            value, sizeof(value));
    if (ret >= 0) {
        ret = 0;
        if (!strncmp(value, "true", sizeof(value)) && !btsink.running)
            ret = start_btsink(adev, parms);
        else if (!strncmp(value, "false", sizeof(value)) && btsink.running)
            stop_btsink();
        else
            AHAL_ERR("btsink_enable=%s is unsupported", value);

        if (ret != 0) {
           AHAL_DBG("start_btsink : failed");
           btsink.running = false;
           return;
        }
    }

    memset(value, 0, sizeof(value));
    ret = str_parms_get_str(parms, AUDIO_PARAMETER_BTSINK_VOLUME, value, sizeof(value));
    if (ret >= 0) {
       AHAL_DBG("Param: set volume");
        if (sscanf(value, "%f", &vol) != 1){
            AHAL_ERR("error in retrieving btsink volume");
            return;
        }
        btsink_set_volume(vol);
    }

    AHAL_DBG("exit");
}
#ifdef __cplusplus
}
#endif
