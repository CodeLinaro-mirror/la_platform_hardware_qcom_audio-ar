/*
 * Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */



#define LOG_TAG "AHAL: hfp_ag"
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

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_PARAMETER_HFP_AG_ENABLE      "BT_SCO"
#define AUDIO_PARAMETER_HFP_AG_SET_SAMPLING_RATE "bt_wbs"
#define AUDIO_PARAMETER_KEY_HFP_VOLUME "hfp_volume"
#define AUDIO_PARAMETER_HFP_PCM_DEV_ID "hfp_pcm_dev_id"

#define AUDIO_PARAMETER_KEY_HFP_MIC_VOLUME "hfp_mic_volume"

struct hfp_ag_module {
    bool is_hfp_running;
    float hfp_volume;
    int ucid;
    float mic_volume;
    bool mic_mute;
    uint32_t sample_rate;
    pal_stream_handle_t *rx_stream_handle;
    pal_stream_handle_t *tx_stream_handle;
};

#define PLAYBACK_VOLUME_MAX 0x2000
#define CAPTURE_VOLUME_DEFAULT                (15.0)
static struct hfp_ag_module agmod = {
    .is_hfp_running = 0,
    .hfp_volume = 0,
    .ucid = USECASE_AUDIO_HFP_SCO_WB,
    .mic_volume = CAPTURE_VOLUME_DEFAULT,
    .mic_mute = 0,
    .sample_rate = 16000,
};

static int32_t hfp_ag_set_volume(float value)
{
    int32_t ret = 0;
    struct pal_volume_data *pal_volume = NULL;

    // TODO: not tested yet
    AHAL_DBG("(%f)\n", value);

    agmod.hfp_volume = value;

    if (value < 0.0) {
        ALOGW("(%f) Under 0.0, assuming 0.0\n", value);
        value = 0.0;
    }

    if (!agmod.is_hfp_running) {
        AHAL_ERR("AG not active, ignoring set_ag_volume call");
        goto exit;
    }

    AHAL_DBG("Setting AG volume to %f \n", value);

    pal_volume = (struct pal_volume_data *)malloc(sizeof(struct pal_volume_data)
            +sizeof(struct pal_channel_vol_kv));

    if (!pal_volume)
       return -ENOMEM;

    pal_volume->no_of_volpair = 1;
    pal_volume->volume_pair[0].channel_mask = 0x03;
    pal_volume->volume_pair[0].vol = value;
    ret = pal_stream_set_volume(agmod.rx_stream_handle, pal_volume);
    if (ret)
        AHAL_ERR("set volume failed: %d \n", ret);

    free(pal_volume);
exit:
    AHAL_VERBOSE("exit");
    return ret;
}

/*Set mic volume to value.
 * *
 * * This interface is used for mic volume control, set mic volume as value(range 0 ~ 15).
 * */
static int hfp_ag_set_mic_volume(float value)
{
    int volume, ret = 0;
    struct pal_volume_data *pal_volume = NULL;

    // TODO: not tested yet
    if (!agmod.is_hfp_running) {
        AHAL_ERR("HFP AG not active, ignoring hfp_ag_set_mic_volume call");
        return -EIO;
    }

    if (value < 0.0) {
        ALOGW("(%f) Under 0.0, assuming 0.0\n", value);
        value = 0.0;
    } else if (value > CAPTURE_VOLUME_DEFAULT) {
        value = CAPTURE_VOLUME_DEFAULT;
        ALOGW("Volume brought within range (%f)\n", value);
    }

    value = value / CAPTURE_VOLUME_DEFAULT;

    volume = (int)(value * PLAYBACK_VOLUME_MAX);

    pal_volume = (struct pal_volume_data *)malloc(sizeof(struct pal_volume_data)
            +sizeof(struct pal_channel_vol_kv));

    if (!pal_volume)
       return -ENOMEM;

    pal_volume->no_of_volpair = 1;
    pal_volume->volume_pair[0].channel_mask = 0x03;
    pal_volume->volume_pair[0].vol = value;
    if (pal_stream_set_volume(agmod.tx_stream_handle, pal_volume) < 0) {
        AHAL_ERR("Couldn't set HFP AG Volume: [%d]", volume);
        return -EINVAL;
    }

    return ret;
}

static float hfp_ag_get_mic_volume(void)
{
    return agmod.mic_volume;
}

static int32_t start_ag(std::shared_ptr<AudioDevice> adev __unused,
        struct str_parms *parms __unused)
{
    int32_t ret = 0;
    uint32_t no_of_devices = 1;
    struct pal_stream_attributes stream_attr = {};
    struct pal_stream_attributes stream_tx_attr = {};
    struct pal_device devices[2] = {};
    struct pal_channel_info ch_info;

    AHAL_DBG("AG start enter");
    agmod.is_hfp_running = true;

    pal_param_device_connection_t param_device_connection;

    param_device_connection.id = PAL_DEVICE_IN_BLUETOOTH_SCO2_HEADSET;
    param_device_connection.connection_state = true;
    ret =  pal_set_param(PAL_PARAM_ID_DEVICE_CONNECTION,
                        (void*)&param_device_connection,
                        sizeof(pal_param_device_connection_t));
    if (ret != 0) {
        AHAL_ERR("Set PAL_PARAM_ID_DEVICE_CONNECTION for %d failed", param_device_connection.id);
        return ret;
    }

    param_device_connection.id = PAL_DEVICE_OUT_BLUETOOTH_SCO2;
    param_device_connection.connection_state = true;
    ret =  pal_set_param(PAL_PARAM_ID_DEVICE_CONNECTION,
                        (void*)&param_device_connection,
                        sizeof(pal_param_device_connection_t));
    if (ret != 0) {
        AHAL_ERR("Set PAL_PARAM_ID_DEVICE_CONNECTION for %d failed", param_device_connection.id);
        return ret;
    }

    pal_param_btsco_t param_btsco;

    param_btsco.bt_sco_on = true;
    if (agmod.sample_rate == 16000) {
        param_btsco.bt_wb_speech_enabled = true;
    } else {
        param_btsco.bt_wb_speech_enabled = false;
    }
    ret =  pal_set_param(PAL_PARAM_ID_BT_AG_SCO,
                        (void*)&param_btsco,
                        sizeof(pal_param_btsco_t));
    if (ret != 0) {
        AHAL_ERR("Set PAL_PARAM_ID_BT_AG_SCO failed");
        return ret;
    }

    AHAL_DBG("AG start end");
    return ret;
}

static int32_t stop_ag()
{
    int32_t ret = 0;

    AHAL_DBG("AG stop enter");

    pal_param_btsco_t param_btsco;

    param_btsco.bt_sco_on = false;
    ret =  pal_set_param(PAL_PARAM_ID_BT_AG_SCO,
                        (void*)&param_btsco,
                        sizeof(pal_param_btsco_t));
    if (ret != 0) {
        AHAL_ERR("Set PAL_PARAM_ID_BT_AG_SCO failed");
    }

    pal_param_device_connection_t param_device_connection;

    param_device_connection.id = PAL_DEVICE_IN_BLUETOOTH_SCO2_HEADSET;
    param_device_connection.connection_state = false;
    ret =  pal_set_param(PAL_PARAM_ID_DEVICE_CONNECTION,
                        (void*)&param_device_connection,
                        sizeof(pal_param_device_connection_t));
    if (ret != 0) {
        AHAL_ERR("Set PAL_PARAM_ID_DEVICE_DISCONNECTION for %d failed", param_device_connection.id);
    }

    param_device_connection.id = PAL_DEVICE_OUT_BLUETOOTH_SCO2;
    param_device_connection.connection_state = false;
    ret =  pal_set_param(PAL_PARAM_ID_DEVICE_CONNECTION,
                        (void*)&param_device_connection,
                        sizeof(pal_param_device_connection_t));
    if (ret != 0) {
        AHAL_ERR("Set PAL_PARAM_ID_DEVICE_DISCONNECTION for %d failed", param_device_connection.id);
    }
    agmod.is_hfp_running = false;

    AHAL_DBG("AG stop end");
    return ret;
}

void hfp_ag_init()
{
    return;
}

bool hfp_ag_is_active(std::shared_ptr<AudioDevice> adev __unused)
{
    return agmod.is_hfp_running;
}

audio_usecase_t hfp_ag_get_usecase()
{
    return agmod.ucid;
}

/*Set mic mute state.
 * *
 * * This interface is used for mic mute state control
 * */
int hfp_ag_set_mic_mute(bool state)
{
    int rc = 0;

    // TODO: not tested yet
    if (state == agmod.mic_mute) {
        AHAL_DBG("mute state already %d", state);
        return rc;
    }

    rc = hfp_ag_set_mic_volume((state == true) ? 0.0 : agmod.mic_volume);
    if (rc == 0)
        agmod.mic_mute = state;
    AHAL_DBG("Setting mute state %d, rc %d\n", state, rc);
    return rc;
}

int hfp_ag_set_parameters(std::shared_ptr<AudioDevice> adev, struct str_parms *parms)
{
    int status = 0;
    char value[32]={0};
    float vol;
    int val;

    if (str_parms_get_str(parms, AUDIO_PARAMETER_HFP_AG_ENABLE, value,sizeof(value)) >= 0) {
        if (!strncmp(value, "on", sizeof(value))) {
            if (!agmod.is_hfp_running) {
                status = start_ag(adev, parms);
            } else {
                AHAL_DBG("BT_SCO=on, but hfp ag is already running, skip");
            }
        } else if (!strncmp(value, "off", sizeof(value))) {
            if (agmod.is_hfp_running) {
                status = stop_ag();
            } else {
                AHAL_DBG("BT_SCO=off, buf hfp ag is not running, skip");
            }
        } else {
            AHAL_ERR("BT_SCO=%s is unsupported", value);
        }
    }

    memset(value, 0, sizeof(value));
    if (str_parms_get_str(parms,AUDIO_PARAMETER_HFP_AG_SET_SAMPLING_RATE, value,sizeof(value)) >= 0) {
        if (!strncmp(value, "off", sizeof(value))){
            agmod.ucid = USECASE_AUDIO_HFP_SCO;
            agmod.sample_rate = 8000;
        } else if (!strncmp(value, "on", sizeof(value))){
            agmod.ucid = USECASE_AUDIO_HFP_SCO_WB;
            agmod.sample_rate = 16000;
        } else
            AHAL_ERR("bt_wbs=%s is unsupported", value);
        AHAL_DBG("ag sample rate: %d", agmod.sample_rate);
    }

    memset(value, 0, sizeof(value));
    if (str_parms_get_str(parms, AUDIO_PARAMETER_KEY_HFP_VOLUME,value, sizeof(value)) >= 0) {
        if (sscanf(value, "%f", &vol) != 1){
            AHAL_ERR("error in retrieving hfp volume");
            status = -EIO;
            goto exit;
        }
        AHAL_DBG("set_hfp_volume usecase, Vol: [%f]", vol);
        status= hfp_ag_set_volume(vol);
        if (status) {
            AHAL_ERR("set volume failed: %d \n", status);
            goto exit;
        }
    }

    memset(value, 0, sizeof(value));
    if (str_parms_get_str(parms, AUDIO_PARAMETER_KEY_HFP_MIC_VOLUME,value, sizeof(value)) >= 0) {
        if (sscanf(value, "%f", &vol) != 1){
            AHAL_ERR("error in retrieving hfp mic volume");
            status = -EIO;
            goto exit;
        }
        AHAL_DBG("set_hfp_mic_volume usecase, Vol: [%f]", vol);
        status = hfp_ag_set_mic_volume(vol);
        if (status == 0)
            agmod.mic_volume = vol;
    }

exit:
    AHAL_VERBOSE("Exit");
    return status;
}

#ifdef __cplusplus
}
#endif
