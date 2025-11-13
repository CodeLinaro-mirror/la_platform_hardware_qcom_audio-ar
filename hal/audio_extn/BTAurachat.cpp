/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL: BTAurachat"
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

#define AUDIO_PARAMETER_AC_RX_ENABLE      "achat_rx_enable"
#define AUDIO_PARAMETER_AC_TX_ENABLE      "achat_tx_enable"
#define AUDIO_PARAMETER_KEY_AC_VOLUME     "achat_rx_volume"
#define AUDIO_PARAMETER_AC_TX_ACQUIRE     "achat_tx_acquire"


struct AurachatParams {
    bool is_achat_running;
    bool is_tx_running;
    bool is_rx_running;
    bool is_tx_acquire_enabled;
    float achat_rx_volume;
    uint32_t sample_rate;
    pal_stream_handle_t *rx_stream_handle;
    pal_stream_handle_t *tx_stream_handle;
};

static struct AurachatParams achatParams = {
    .is_achat_running = 0,
    .is_tx_running = 0,
    .is_rx_running = 0,
    .is_tx_acquire_enabled = 0,
    .achat_rx_volume = 14,
    .sample_rate = 16000
};

const float volume_lookup[16] = {
    0.0L,
    17.0L  / 8192.0L,
    38.0L  / 8192.0L,
    81.0L  / 8192.0L,
    121.0L / 8192.0L,
    193.0L / 8192.0L,
    307.0L / 8192.0L,
    458.0L / 8192.0L,
    728.0L / 8192.0L,
    1157.0L / 8192.0L,
    1551.0L / 8192.0L,
    2185.0L / 8192.0L,
    3078.0L / 8192.0L,
    4129.0L / 8192.0L,
    5816.0L / 8192.0L,
    8192.0L / 8192.0L
};

static int32_t achatSetVolume(float value)
{
    int32_t vol, ret = 0;
    struct pal_volume_data *pal_volume = NULL;

    achatParams.achat_rx_volume = value;

    if (value < 0.0) {
        AHAL_DBG("(%f) Under 0.0, assuming 0.0\n", value);
        value = 0.0;
    } else {
        value = value> 15.0 ? 1.0 : volume_lookup[static_cast<int>(value)] ;
        AHAL_DBG("Volume brought within range (%f)\n", value);
    }

    vol = lrint((value * 0x2000) + 0.5);

    if (!achatParams.is_rx_running || !achatParams.rx_stream_handle) {
        AHAL_DBG("Aurachat RX not active, ignoring set_volume call");
        return -EIO;
    }

    AHAL_DBG("Setting Aurachat RX volume to %f", value);

    pal_volume = (struct pal_volume_data *) calloc(1,sizeof(struct pal_volume_data) + sizeof(struct pal_channel_vol_kv));

    if (!pal_volume)
        return -ENOMEM;

    pal_volume->no_of_volpair = 1;
    pal_volume->isDucking = false;
    pal_volume->volume_pair[0].channel_mask = 0x03;
    pal_volume->volume_pair[0].vol = value;

    ret = pal_stream_set_volume(achatParams.rx_stream_handle, pal_volume);
    if (ret)
        AHAL_ERR("set volume failed: %d", ret);

    free(pal_volume);
    AHAL_DBG("achatSetVolume exit");
    return ret;
}

static int32_t achatStartTx(std::shared_ptr<AudioDevice> adev __unused,
        struct str_parms *parms __unused)
{
    int32_t ret = 0;
    uint32_t no_of_devices = 2;
    struct pal_stream_attributes stream_attr = {};
    struct pal_stream_attributes stream_tx_attr = {};
    struct pal_device devices[2] = {};
    struct pal_channel_info ch_info;

    AHAL_DBG("Aurachat tx start enter");
    if (achatParams.tx_stream_handle)
        return 0; //Aurachat TX already running;

    ch_info.channels = 1;
    ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;

    /* Mic -> BT Broadcast */
    stream_tx_attr.type = PAL_STREAM_LOOPBACK;
    stream_tx_attr.info.opt_stream_info.loopback_type = PAL_STREAM_LOOPBACK_BT_AC_TX;
    stream_tx_attr.direction = PAL_AUDIO_INPUT_OUTPUT;
    stream_tx_attr.in_media_config.sample_rate = achatParams.sample_rate;
    stream_tx_attr.in_media_config.bit_width = 16;
    stream_tx_attr.in_media_config.ch_info = ch_info;
    stream_tx_attr.in_media_config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    stream_tx_attr.out_media_config.sample_rate = 16000;
    stream_tx_attr.out_media_config.bit_width = 16;
    stream_tx_attr.out_media_config.ch_info = ch_info;
    stream_tx_attr.out_media_config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    devices[0].id = PAL_DEVICE_OUT_BLUETOOTH_A2DP;
    devices[0].config.sample_rate = achatParams.sample_rate;
    devices[0].config.bit_width = 16;
    devices[0].config.ch_info = ch_info;
    devices[0].config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    devices[1].id = PAL_DEVICE_IN_SPEAKER_MIC;

    ret = pal_stream_open(&stream_tx_attr,
            no_of_devices, devices,
            0,
            NULL,
            NULL,
            0,
            &achatParams.tx_stream_handle);
    if (ret != 0) {
        AHAL_ERR("AC tx stream (Mic->BT A2dp) open failed, rc %d", ret);
        achatParams.tx_stream_handle = NULL;
        return ret;
    }
    ret = pal_stream_start(achatParams.tx_stream_handle);
    if (ret != 0) {
        AHAL_ERR("AC tx stream (Mic->BT A2dp) start failed, rc %d", ret);
        pal_stream_close(achatParams.tx_stream_handle);
        achatParams.tx_stream_handle = NULL;
        return ret;
    }

    achatParams.is_tx_running = true;
    AHAL_DBG("Aurachat tx start end");
    return ret;
}

static int32_t achatStartRx(std::shared_ptr<AudioDevice> adev __unused,
        struct str_parms *parms __unused)
{
    int32_t ret = 0;
    uint32_t no_of_devices = 2;
    struct pal_stream_attributes stream_attr = {};
    struct pal_stream_attributes stream_tx_attr = {};
    struct pal_device devices[2] = {};
    struct pal_channel_info ch_info;

    AHAL_DBG("Aurachat rx start enter");
    if (achatParams.rx_stream_handle)
        return 0; //Aurachat RX already running;

    pal_param_device_connection_t param_device_connection;

    param_device_connection.id = PAL_DEVICE_IN_BLUETOOTH_BROADCAST;
    param_device_connection.connection_state = true;
    ret =  pal_set_param(PAL_PARAM_ID_DEVICE_CONNECTION,
                        (void*)&param_device_connection,
                        sizeof(pal_param_device_connection_t));
    if (ret != 0) {
        AHAL_ERR("Set PAL_PARAM_ID_DEVICE_CONNECTION for %d failed", param_device_connection.id);
        return ret;
    }

    ch_info.channels = 1;
    ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;

    /* Broadcast Tx  -> Spkr */
    stream_attr.type = PAL_STREAM_LOOPBACK;
    stream_attr.info.opt_stream_info.loopback_type = PAL_STREAM_LOOPBACK_BT_AC_RX;
    stream_attr.direction = PAL_AUDIO_INPUT_OUTPUT;
    stream_attr.in_media_config.sample_rate = achatParams.sample_rate;
    stream_attr.in_media_config.bit_width = 16;
    stream_attr.in_media_config.ch_info = ch_info;
    stream_attr.in_media_config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    stream_attr.out_media_config.sample_rate = 16000;
    stream_attr.out_media_config.bit_width = 16;
    stream_attr.out_media_config.ch_info = ch_info;
    stream_attr.out_media_config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    devices[0].id = PAL_DEVICE_IN_BLUETOOTH_BROADCAST;
    devices[0].config.sample_rate = achatParams.sample_rate;
    devices[0].config.bit_width = 16;
    devices[0].config.ch_info = ch_info;
    devices[0].config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    devices[1].id = PAL_DEVICE_OUT_SPEAKER;
    strlcpy(devices[1].custom_config.custom_key, "bt-ac-rx-usecase",
        sizeof(devices[1].custom_config.custom_key));
    ret = pal_stream_open(&stream_attr,
            no_of_devices, devices,
            0,
            NULL,
            NULL,
            0,
            &achatParams.rx_stream_handle);
    if (ret != 0) {
        AHAL_ERR("BT AC rx stream (BT AC ->Spkr) open failed, rc %d", ret);
        achatParams.rx_stream_handle = NULL;
        return ret;
    }
    ret = pal_stream_start(achatParams.rx_stream_handle);
    if (ret != 0) {
        AHAL_ERR("AC rx stream (BT AC ->Spkr) start failed, rc %d", ret);
        pal_stream_close(achatParams.rx_stream_handle);
        achatParams.rx_stream_handle = NULL;
        return ret;
    }

    achatParams.is_rx_running = true;
    achatParams.is_achat_running = true;
    achatSetVolume(achatParams.achat_rx_volume);

    AHAL_DBG("BT Aurachat rx start end");
    return ret;
}

static int32_t achatStopTx()
{
    int32_t ret = 0;

    if (achatParams.tx_stream_handle) {
        pal_stream_stop(achatParams.tx_stream_handle);
        pal_stream_close(achatParams.tx_stream_handle);
        achatParams.tx_stream_handle = NULL;
    }

    achatParams.is_tx_running = false;
    AHAL_DBG("Aurachat Tx stop end");
    return ret;

}

static int32_t achatStopRx()
{
    int32_t ret = 0;

    AHAL_DBG("Aurachat rx stop enter");
    achatParams.is_achat_running = false;
    if (achatParams.rx_stream_handle) {
        pal_stream_stop(achatParams.rx_stream_handle);
        pal_stream_close(achatParams.rx_stream_handle);
        achatParams.rx_stream_handle = NULL;
    }

    pal_param_device_connection_t param_device_connection;

    param_device_connection.id = PAL_DEVICE_IN_BLUETOOTH_BROADCAST;
    param_device_connection.connection_state = false;
    ret =  pal_set_param(PAL_PARAM_ID_DEVICE_CONNECTION,
                        (void*)&param_device_connection,
                        sizeof(pal_param_device_connection_t));
    if (ret != 0) {
        AHAL_ERR("Set PAL_PARAM_ID_DEVICE_DISCONNECTION for %d failed", param_device_connection.id);
    }

    achatParams.is_rx_running = false;
    AHAL_DBG("Aurachat Rx stop end");
    return ret;
}

void achat_init()
{
    return;
}

bool isAuraChatActive(std::shared_ptr<AudioDevice> adev __unused)
{
    return achatParams.is_achat_running;
}

int achat_tx_acquire_enable(std::shared_ptr<AudioDevice> adev, bool state)
{
    int rc = 0;

    AHAL_DBG("achat_tx_acquire_enable called with state=%d", state);

    // Check if state is already applied
    if (state == achatParams.is_tx_acquire_enabled) {
        AHAL_DBG("TX Acquire state already %d, no change needed", state);
        return rc;
    }

    // Prepare structured payload
    pal_voice_active_detection_payload vadPayload;
    memset(&vadPayload, 0, sizeof(vadPayload));
    vadPayload.isVadEnabled = state;

    AHAL_DBG("Prepared TX Acquire payload -> isVadEnabled=%d", vadPayload.isVadEnabled);

    // Call PAL API to set parameter
    rc = pal_set_param(PAL_PARAM_ID_VOICE_ACTIVE_DETECTION,
                       (void *)&vadPayload,
                       sizeof(pal_voice_active_detection_payload));

    if (rc) {
        AHAL_ERR("Failed to set VAD state %d via pal_set_param, rc=%d", state, rc);
    } else {
        AHAL_DBG("Successfully set VAD state to %d", state);
        achatParams.is_tx_acquire_enabled = state;
    }

    AHAL_DBG("achat_vad_enable exit with rc=%d", rc);
    return rc;
}

void achat_set_parameters(std::shared_ptr<AudioDevice> adev, struct str_parms *parms)
{
    int status = 0;
    char value[32]={0};
    float vol;
    int val;
    int rate;

    AHAL_DBG("enter");

    //Rx enable parameter
    status = str_parms_get_str(parms, AUDIO_PARAMETER_AC_RX_ENABLE, value, sizeof(value));
    if (status >= 0) {
        //start Rx if not running
        if (!strncmp(value, "true", sizeof(value)) && !achatParams.is_rx_running) {
            status = achatStartRx(adev, parms);
        }
        //stop Rx if it is running
        else if (!strncmp(value, "false", sizeof(value)) && achatParams.is_rx_running) {
            achatStopRx();
        }
        else {
            AHAL_ERR("achat_rx_enable=%s is unsupported", value);
        }
    }

    //Tx enable parameter
    memset(value, 0, sizeof(value));
    status = str_parms_get_str(parms, AUDIO_PARAMETER_AC_TX_ENABLE, value, sizeof(value));
    if (status >= 0) {
        //start TX if not running
        if (!strncmp(value, "true", sizeof(value)) && !achatParams.is_tx_running) {
            status = achatStartTx(adev, parms);
        }
        //stop TX if it is running
        else if (!strncmp(value, "false", sizeof(value)) && achatParams.is_tx_running) {
            if (achatParams.is_tx_acquire_enabled) {
                AHAL_DBG("TX acquire is enabled, disabling it now for Tx Stop");
                achat_tx_acquire_enable(adev, false);
            }
            achatStopTx();
        }
        else {
            AHAL_ERR("achat_tx_enable=%s is unsupported", value);
        }
    }

    //TX Acquire parameter
    memset(value, 0, sizeof(value));
    status = str_parms_get_str(parms, AUDIO_PARAMETER_AC_TX_ACQUIRE, value, sizeof(value));
    if (status >= 0) {
        if (!strncmp(value, "true", sizeof(value))) {
            AHAL_DBG("Enabling TX Acquire");
            status = achat_tx_acquire_enable(adev, true);
        } else if (!strncmp(value, "false", sizeof(value))) {
            AHAL_DBG("Disabling TX Acquire");
            status = achat_tx_acquire_enable(adev, false);
        } else {
            AHAL_ERR("Invalid TX Acquire parameter value: %s", value);
        }
    }

    //volume adjustment
    memset(value, 0, sizeof(value));
    status = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_AC_VOLUME, value, sizeof(value));
    if (status >= 0) {
        if (sscanf(value, "%f", &vol) != 1){
            AHAL_ERR("error in retrieving aurachat rx volume");
            status = -EIO;
            goto exit;
        }
        AHAL_DBG("achatSetVolume usecase, Vol: [%f]", vol);
        achatSetVolume(vol);
    }

exit:
    AHAL_VERBOSE("Exit");
}

#ifdef __cplusplus
}
#endif
