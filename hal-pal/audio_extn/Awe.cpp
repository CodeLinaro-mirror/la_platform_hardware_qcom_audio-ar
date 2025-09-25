/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL: AWE"
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
#include <map>
#include "PalApi.h"
#include "AudioDevice.h"


//////////////////////////////////////////////////////////////////////////////////////////
// Definitions
//////////////////////////////////////////////////////////////////////////////////////////

/** Definitions of the AWE command indexes for setparams.  Defined in the Audioweaver Design
 * file, but standarized.
 */
typedef enum
{
    AUDIO_EXTN_AWE_CONTROL_INDEX_MEDIA_VOLUME           =0,
    AUDIO_EXTN_AWE_CONTROL_INDEX_CALL_VOLUME            =1,
    AUDIO_EXTN_AWE_CONTROL_INDEX_CALL_RING_VOLUME       =2,
    AUDIO_EXTN_AWE_CONTROL_INDEX_NAV_ASSIST_VOLUME      =3,
    AUDIO_EXTN_AWE_CONTROL_INDEX_VR_VOLUME              =4,
    AUDIO_EXTN_AWE_CONTROL_INDEX_SYSTEM_VOLUME          =5,
    AUDIO_EXTN_AWE_CONTROL_INDEX_DRIVER_ZONE_VOLUME     =6,
    AUDIO_EXTN_AWE_CONTROL_INDEX_ICC_VOLUME             =7,
    AUDIO_EXTN_AWE_CONTROL_INDEX_ENT_MUTE               =8,
    AUDIO_EXTN_AWE_CONTROL_INDEX_MUTE_ALL               =9,
    AUDIO_EXTN_AWE_CONTROL_INDEX_DUCK_AWE               =10,
    AUDIO_EXTN_AWE_CONTROL_INDEX_HFP_MIC_MUTE           =11,
    AUDIO_EXTN_AWE_CONTROL_INDEX_SR_CARPLAY             =12,
    AUDIO_EXTN_AWE_CONTROL_INDEX_SR_VOIP                =13,
    AUDIO_EXTN_AWE_CONTROL_INDEX_MEDIA_ACTIVE           =14,
    AUDIO_EXTN_AWE_CONTROL_INDEX_NAV_ACTIVE             =15,
    AUDIO_EXTN_AWE_CONTROL_INDEX_VR_ACTIVE              =16,
    AUDIO_EXTN_AWE_CONTROL_INDEX_CALL_RING_ACTIVE       =17,
    AUDIO_EXTN_AWE_CONTROL_INDEX_CALL_MODE              =24,
    AUDIO_EXTN_AWE_CONTROL_INDEX_VOICE_CALL_ZONE        =25,
    AUDIO_EXTN_AWE_CONTROL_INDEX_VOIP_CALL_ZONE         =26,
    AUDIO_EXTN_AWE_CONTROL_INDEX_ASSIST_ZONE            =27,
    AUDIO_EXTN_AWE_CONTROL_INDEX_RECORDING_ZONE         =28,
    AUDIO_EXTN_AWE_CONTROL_INDEX_COUNT                  =33,
} audio_extn_awe_control_index_type_t;

#define AUDIO_PARAMETER_KEY_HFP_VOLUME          "hfp_volume"
#define AUDIO_PARAMETER_HFP_ZONE                "hfp_zone"
#define AUDIO_PARAMETER_VOICE_ASSISTANT_ZONE    "voice_assistant_zone"
#define AUDIO_PARAMETER_HFP_ENABLE              "hfp_enable"
#define AUDIO_PARAMETER_INPUT_MIC_ZONE          "input_mic_zone"
#define AUDIO_PARAMETER_OUTPUT_MUTE             "DevicesToMute"
#define AUDIO_PARAMETER_OUTPUT_UNMUTE           "DevicesToUnmute"


#define AUDIO_PARAMETER_ZONAL_ENABLE          "zonal_rendering_enabled"
#define AUDIO_PARAMETER_ZONAL_DEVICE          "zonal_rendering_device"


//////////////////////////////////////////////////////////////////////////////////////////
// Configuration
// This section could / should be part of a configuration file, but this module does
// not (yet?) support config files.
//////////////////////////////////////////////////////////////////////////////////////////


#define DEFAULT_ZONAL_DEVICE_BUS_NAME  "BUS16_REAR_SEAT"
#define DEFAULT_ZONAL_ENABLED_VALUE    1

// Multi-Map of bus names to AWE param indices.  Note that some busses go to
// more than one setparam.
// FUTURE: move this to a configuration file!
std::multimap<std::string,int> mapVolumeParamId = {
    {"BUS00_MEDIA",             AUDIO_EXTN_AWE_CONTROL_INDEX_MEDIA_VOLUME},
    {"BUS02_NAV_GUIDANCE",      AUDIO_EXTN_AWE_CONTROL_INDEX_NAV_ASSIST_VOLUME},
    {"BUS03_PHONE",             AUDIO_EXTN_AWE_CONTROL_INDEX_CALL_VOLUME},
    {"BUS08_FRONT_PASSENGER",   AUDIO_EXTN_AWE_CONTROL_INDEX_DRIVER_ZONE_VOLUME},
    {"BUS16_REAR_SEAT",         AUDIO_EXTN_AWE_CONTROL_INDEX_NAV_ASSIST_VOLUME},
    {"BUSxx_VR",                AUDIO_EXTN_AWE_CONTROL_INDEX_VR_VOLUME}, //Update Bus address of VR
    {"BUSxx_CALLRING",          AUDIO_EXTN_AWE_CONTROL_INDEX_CALL_RING_VOLUME}, //Update Bus address of CALL Ring
};

std::multimap<std::string,int> mapActiveStreamParamId = {
    {"BUS00_MEDIA",             AUDIO_EXTN_AWE_CONTROL_INDEX_MEDIA_ACTIVE},
    {"BUS02_NAV_GUIDANCE",      AUDIO_EXTN_AWE_CONTROL_INDEX_NAV_ACTIVE},
    {"BUSxx_VR",                AUDIO_EXTN_AWE_CONTROL_INDEX_VR_ACTIVE}, //Update Bus address of VR
    {"BUSxx_CALLRING",          AUDIO_EXTN_AWE_CONTROL_INDEX_CALL_RING_ACTIVE}, //Update Bus address of CALL Ring  
};


//////////////////////////////////////////////////////////////////////////////////////////
// Implementation
//////////////////////////////////////////////////////////////////////////////////////////


static int awe_set_custom_param(int index, void *value);
static int awe_get_custom_param(int index, void *value);



// Future for updated AWE release with additional indices
// #define AWE_CUSTOMER_NUM_COMMAND_INDEXES        ((int)AUDIO_EXTN_AWE_CONTROL_INDEX_COUNT)
#define AWE_CUSTOMER_NUM_COMMAND_INDEXES        ((int)64)
#define AWE_CUSTOMER_COMMAND_BUFFER_LENGTH      (AWE_CUSTOMER_NUM_COMMAND_INDEXES)
#define AWE_PAYLOAD_SIZE                        (AWE_CUSTOMER_NUM_COMMAND_INDEXES + 5)

#define AWE_MODULE_INSTANCE_ID                  (0x0)              // Set MIID with zero here, and the PAL finds the module by TAG
// #define AWE_MODULE_CONFIG_PARAM_ID              (0x08002177)    // Old value for ES4.1 timeframe
#define AWE_MODULE_CONFIG_PARAM_ID              (0x18052002)       // Updated for ES5.2

#define AWE_MAX_PARAM_BUFFER                        32

#define PAL_ALIGN_8BYTE(x) (((x) + 7) & (~7))

#ifndef UINT32
#define UINT32	unsigned int
#endif

#ifndef INT32
#define INT32	int
#endif


struct apm_module_param_data_t
{
    uint32_t module_instance_id;
    /**< Valid instance ID of module
       @values  */

  uint32_t param_id;
  /**< Valid ID of the parameter.

        @values See Chapter */

    uint32_t param_size;
    /**< Size of the parameter data based upon the
        module_instance_id/param_id combination.
       @values > 0 bytes, in multiples of
       4 bytes at least */

  uint32_t error_code;
    /**< Error code populated by the entity hosting the  module.
      Applicable only for out-of-band command mode  */
 } ;
typedef struct apm_module_param_data_t apm_module_param_data_t;


#ifdef __cplusplus
extern "C" {
#endif


void awe_lib_init() {
    AHAL_DBG("%s: enter", __func__);
    // FUTURE: could be used to load config file
}


int awe_set_parameters(std::shared_ptr<AudioDevice> adev , struct str_parms *params)
{

    int ret = 0;
    char value[AWE_MAX_PARAM_BUFFER]={0};
    int status = 0;
    float vol;
    float customVal;
    int val;
    int va_zone_id;
    int hfp_zone_id;
    int hfp_enable;
    int mic_zone_id;

    AHAL_VERBOSE("%s: enter", __func__);

    if (str_parms_get_str(params, AUDIO_PARAMETER_KEY_HFP_VOLUME,value, sizeof(value)) >= 0) {
        if (sscanf(value, "%f", &vol) != 1){
            AHAL_ERR("error in retrieving hfp volume. value=%s", value);
            status = -EIO;
            goto exit;
        }
        AHAL_DBG("set_hfp_volume usecase, Vol: [%f]", vol);
        ret = awe_set_custom_param(AUDIO_EXTN_AWE_CONTROL_INDEX_CALL_VOLUME, (void *)&vol);
    }

    if (str_parms_get_str(params, AUDIO_PARAMETER_HFP_ZONE,value, sizeof(value)) >= 0) {
        if (sscanf(value, "%d", &hfp_zone_id) != 1){
            AHAL_ERR("error in retrieving zonal_hfp: value=%s", value);
            status = -EIO;
            goto exit;
        }
        AHAL_DBG("zonal_hfp set hfp_zone_id=%d", hfp_zone_id);
        customVal = (float)hfp_zone_id;
        ret = awe_set_custom_param(AUDIO_EXTN_AWE_CONTROL_INDEX_VOICE_CALL_ZONE, (void *)&customVal);
    }

    if (str_parms_get_str(params, AUDIO_PARAMETER_VOICE_ASSISTANT_ZONE,value, sizeof(value)) >= 0) {
        if (sscanf(value, "%d", &va_zone_id) != 1){
            AHAL_ERR("error in retrieving va_zone_id: value=%s", value);
            status = -EIO;
            goto exit;
        }
        AHAL_DBG("va_zone_id=%d", va_zone_id);
        customVal = (float)va_zone_id;
        ret = awe_set_custom_param(AUDIO_EXTN_AWE_CONTROL_INDEX_ASSIST_ZONE, (void *)&customVal);
    }

    if (str_parms_get_str(params, AUDIO_PARAMETER_INPUT_MIC_ZONE,value, sizeof(value)) >= 0) {
        if (sscanf(value, "%d", &mic_zone_id) != 1){
            AHAL_ERR("error in retrieving mic_zone_id: value=%s", value);
            status = -EIO;
            goto exit;
        }
        AHAL_DBG("mic_zone_id=%d", mic_zone_id);
        customVal = (float)mic_zone_id;
        ret = awe_set_custom_param(AUDIO_EXTN_AWE_CONTROL_INDEX_RECORDING_ZONE, (void *)&customVal);
    }

    // TODO: the call mode parameter is actually a bitmask.  We may need to expand or change
    // the android setparam in the future.
    if (str_parms_get_str(params, AUDIO_PARAMETER_HFP_ENABLE, value,sizeof(value)) >= 0) {
        hfp_enable = 0;
        if (!strncmp(value, "true", sizeof(value)))
            hfp_enable = 1;
        customVal = (float)hfp_enable;
        ret = awe_set_custom_param(AUDIO_EXTN_AWE_CONTROL_INDEX_CALL_MODE, (void *)&customVal);
    }

    if (str_parms_get_str(params, AUDIO_PARAMETER_OUTPUT_MUTE, value,sizeof(value)) >= 0) {
        if (strstr(value, "BUS00_MEDIA")) {
            val = 1;
            customVal = (float)val;
            ret = awe_set_custom_param(AUDIO_EXTN_AWE_CONTROL_INDEX_ENT_MUTE, (void *)&customVal);
        }
    }

    if (str_parms_get_str(params, AUDIO_PARAMETER_OUTPUT_UNMUTE, value,sizeof(value)) >= 0) {
        if (strstr(value, "BUS00_MEDIA")) {
            val = 0;
            customVal = (float)val;
            ret = awe_set_custom_param(AUDIO_EXTN_AWE_CONTROL_INDEX_ENT_MUTE, (void *)&customVal);
        }
    }

exit:

    AHAL_VERBOSE("%s: exit", __func__);

    return ret;
}

void awe_get_params(std::shared_ptr<AudioDevice> adev __unused, struct str_parms *query, struct str_parms *reply)
{
    int ret;
    char value[AWE_MAX_PARAM_BUFFER] = {0};

    AHAL_VERBOSE("%s: enter", __func__);

    if(query && reply){
        ret = str_parms_get_str(query, AUDIO_PARAMETER_ZONAL_ENABLE, value, sizeof(value));
        if (ret >= 0)
            str_parms_add_int(reply, AUDIO_PARAMETER_ZONAL_ENABLE, DEFAULT_ZONAL_ENABLED_VALUE);
        ret = str_parms_get_str(query, AUDIO_PARAMETER_ZONAL_DEVICE, value, sizeof(value));
        if (ret >= 0)
            str_parms_add_str(reply, AUDIO_PARAMETER_ZONAL_DEVICE, DEFAULT_ZONAL_DEVICE_BUS_NAME);

        if (str_parms_get_str(query, AUDIO_PARAMETER_HFP_ZONE,value, sizeof(value)) >= 0) {
            float floatVal = 0.0;

            ret = awe_get_custom_param(AUDIO_EXTN_AWE_CONTROL_INDEX_VOICE_CALL_ZONE, (void *)&floatVal);
            if (ret == 0) {
                AHAL_DBG("zonal_hfp get hfp_zone_id=%d", (int)floatVal);
                str_parms_add_int(reply, AUDIO_PARAMETER_HFP_ZONE, (int)floatVal);
            }
        }
    }
}

int awe_set_mic_mute(bool state) {

    int ret = 0;
    float customVal;
    AHAL_INFO("Enter awe_set_mic_mute: %d", state);

    customVal = (float)state;
    ret = awe_set_custom_param(AUDIO_EXTN_AWE_CONTROL_INDEX_HFP_MIC_MUTE, (void *)&customVal);

    AHAL_DBG("Exit ret: %d", ret);
    return ret;
}


int awe_set_stream_active(int stream_type, const char *address,int value) {

    int ret = 0;
    AHAL_INFO("Enter awe_set_stream_active: Stream Type %d and Bus addr %s, value %d", stream_type, address, value);

    // Set volume level to AWE if necessary
    if (stream_type == PAL_STREAM_PLAYBACK_BUS && address != NULL) {

        for (std::multimap<std::string, int>::iterator itr = mapActiveStreamParamId.begin(); itr != mapActiveStreamParamId.end(); ++itr) {
            if (itr->first == address) {
                float valToSet = value;
                int aweSetIndex = itr->second;
                AHAL_DBG("found match for %s: %d", address, aweSetIndex);
                ret = awe_set_custom_param(aweSetIndex, (void *)&valToSet);
            }
        }

    }

    AHAL_DBG("Exit ret: %d", ret);
    return ret;
}


int awe_set_volume(int stream_type, const char *address, float left , float right) {

    int ret = 0;
    AHAL_INFO("Enter awe_set_volume: Stream Type %d and Bus addr %s, left %f, right %f", stream_type, address, left, right);

    // Set volume level to AWE if necessary
    if ((stream_type == 0) ||
        (stream_type == PAL_STREAM_PLAYBACK_BUS)) {

        // Look up in our map of bus names to param indices.
        // Because this is a multimap, do a brute force iteration and comparison.
        for (std::multimap<std::string, int>::iterator itr = mapVolumeParamId.begin(); itr != mapVolumeParamId.end(); ++itr) {
            if (itr->first == address) {
                float valToSet = (left == right) ? left : (left + right) / 2.0;
                int aweSetIndex = itr->second;
                AHAL_DBG("found match for %s: %d", address, aweSetIndex);
                ret = awe_set_custom_param(aweSetIndex, (void *)&valToSet);
            }
        }
    }

    AHAL_DBG("Exit ret: %d", ret);
    return ret;
}



// START: Set Custom Param with offest and value
// index is zero based, see audio_defs_ar.h for values
int awe_set_custom_param(int index, void *value)
{
    UINT32 s_sendBuffer[AWE_CUSTOMER_COMMAND_BUFFER_LENGTH];
    int32_t ret = 0;
    uint8_t  *payloadInfo = NULL;
    size_t  size = 0;
    struct apm_module_param_data_t* header;

    if (index > AWE_CUSTOMER_NUM_COMMAND_INDEXES) {
        AHAL_ERR("%s: Cannot set custom param index=%d.", __func__, index);
        return -EINVAL;
    }

    //create payload and Get
    payloadInfo = new uint8_t[PAL_ALIGN_8BYTE((AWE_PAYLOAD_SIZE -1) *(sizeof(uint32_t)))]();

    header = (struct apm_module_param_data_t*)payloadInfo;
    header->module_instance_id = AWE_MODULE_INSTANCE_ID;
    header->param_id = AWE_MODULE_CONFIG_PARAM_ID;
    header->error_code = 0x0;
    header->param_size = PAL_ALIGN_8BYTE((AWE_CUSTOMER_COMMAND_BUFFER_LENGTH) *(sizeof(uint32_t)));

    ret = pal_get_param(PAL_PARAM_ID_CUSTOM_MODULE_CONFIG, (void **)&payloadInfo, &size, nullptr);
    AHAL_INFO("%s: pal_get_param returned ret=%d", __func__, ret);
    if (ret != 0) {
        AHAL_ERR("%s: error from PAL, error=%d.  Returning.", __func__, ret);
        delete[] payloadInfo;
        payloadInfo = NULL;
        return ret;
    }
    memcpy(s_sendBuffer, payloadInfo, (AWE_CUSTOMER_COMMAND_BUFFER_LENGTH* sizeof(uint32_t)));

    delete[] payloadInfo;
    payloadInfo = NULL;

    //create payload and Set
    static uint32_t SendPayLoadPkt[AWE_PAYLOAD_SIZE];

    SendPayLoadPkt[0] = PAL_ALIGN_8BYTE((AWE_PAYLOAD_SIZE -1) *(sizeof(uint32_t)));
    SendPayLoadPkt[1] = AWE_MODULE_INSTANCE_ID;
    SendPayLoadPkt[2] = AWE_MODULE_CONFIG_PARAM_ID;
    SendPayLoadPkt[3] = PAL_ALIGN_8BYTE((AWE_CUSTOMER_COMMAND_BUFFER_LENGTH) *(sizeof(uint32_t)));
    SendPayLoadPkt[4] = 0;

    //set custom param value
    memcpy(&s_sendBuffer[index],value, sizeof(UINT32));
    AHAL_INFO("awe_set_custom_param index %d and value 0x%x", index, s_sendBuffer[index]);
    memcpy(&SendPayLoadPkt[5], s_sendBuffer, (AWE_CUSTOMER_COMMAND_BUFFER_LENGTH* sizeof(uint32_t)));

    ret = pal_set_param(PAL_PARAM_ID_CUSTOM_MODULE_CONFIG, (void *)SendPayLoadPkt, AWE_PAYLOAD_SIZE);

    return 0;
}

// START: Set Custom Param with offest and value
// index is zero based, see audio_defs_ar.h for values
int awe_get_custom_param(int index, void *value)
{
    UINT32 s_sendBuffer[AWE_CUSTOMER_COMMAND_BUFFER_LENGTH];
    int32_t ret = 0;
    uint8_t  *payloadInfo = NULL;
    size_t  size = 0;
    struct apm_module_param_data_t* header;

    if (index > AWE_CUSTOMER_NUM_COMMAND_INDEXES) {
        AHAL_ERR("%s: Cannot get custom param index=%d.", __func__, index);
        return -EINVAL;
    }

    //create payload and Get
    payloadInfo = new uint8_t[PAL_ALIGN_8BYTE((AWE_PAYLOAD_SIZE -1) *(sizeof(uint32_t)))]();

    header = (struct apm_module_param_data_t*)payloadInfo;
    header->module_instance_id = AWE_MODULE_INSTANCE_ID;
    header->param_id = AWE_MODULE_CONFIG_PARAM_ID;
    header->error_code = 0x0;
    header->param_size = PAL_ALIGN_8BYTE((AWE_CUSTOMER_COMMAND_BUFFER_LENGTH) *(sizeof(uint32_t)));

    ret = pal_get_param(PAL_PARAM_ID_CUSTOM_MODULE_CONFIG, (void **)&payloadInfo, &size, nullptr);
    AHAL_INFO("%s: pal_get_param returned ret=%d", __func__, ret);
    if (ret != 0) {
        AHAL_ERR("%s: error from PAL, error=%d.  Returning.", __func__, ret);
        delete[] payloadInfo;
        payloadInfo = NULL;
        return ret;
    }

    //get custom param value
    memcpy(s_sendBuffer, payloadInfo, (AWE_CUSTOMER_COMMAND_BUFFER_LENGTH* sizeof(uint32_t)));
    memcpy(value, &s_sendBuffer[index], sizeof(UINT32));
    AHAL_INFO("awe_get_custom_param index %d and value1 0x%x", index, s_sendBuffer[index]);
    AHAL_INFO("awe_get_custom_param index %d and value2 0x%x", index, (UINT32) * ((UINT32*)value));

    delete[] payloadInfo;
    payloadInfo = NULL;

    return 0;
}

void awe_reset_recording() {
    AHAL_INFO("Enter awe_reset_recording");
    float valueToSet = 0.0;
    awe_set_custom_param(AUDIO_EXTN_AWE_CONTROL_INDEX_RECORDING_ZONE, (void *)&valueToSet);
    AHAL_DBG("Exit");
}

#ifdef __cplusplus
}
#endif
