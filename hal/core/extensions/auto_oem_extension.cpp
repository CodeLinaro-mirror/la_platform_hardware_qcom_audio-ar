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
 * Changes from Qualcomm Technologies, Inc. are provided under the following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL_OEMEXTENSTION_QTI"
#define LOG_NDDEBUG 0

#include <android-base/logging.h>
#include <cutils/properties.h>
#include <cutils/str_parms.h>
#include <string>
#include <errno.h>
#include <log/log.h>
#include <extensions/auto_oem_extension.h>
#include "PalApi.h"
#include <include/extensions/AudioVehicleListener.h>
#include "include/extensions/AudioConfig.h"
#include "include/extensions/AudioCalib.h"
#include <cmath> // For round function


#include <aidl/android/hardware/automotive/vehicle/SubscribeOptions.h>
#include <aidl/android/hardware/automotive/vehicle/VehicleProperty.h>
#include <android-base/strings.h>
#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <cutils/properties.h>
#include <utils/Errors.h>
#include <utils/Log.h>
#include <utils/StrongPointer.h>

#include <signal.h>
#include <stdio.h>

#define EQ_MODE_PARAM_ID 0x11112550
#define PERF_VEHICLE_SPEED_PARAM_ID 0x11112524
#define HVAC_FAN_SPEED_PARAM_ID 0x11112525
#define FADER_PARAM_ID 0x11112520
#define BALANCE_PARAM_ID 0x11112521
#define SPATIALISATION_PARAM_ID 0x11112505
#define PARAM_ID_SDVC 0x11112523

#define AWX_MODULE_CUSTOM_TAG 0XC0000057

#define EQ_MASK_SOURCE_TYPE  0x0002
#define EQ_MASK_SOURCE_VAL  1
#define MASK 0x0000FFFF
#define MIN__PERF_VEHICLE_SPEED 0
#define MAX_PERF_VEHICLE_SPEED 65534
#define MIN_HVAC_FAN_SPEED 0
#define MAX_HVAC_FAN_SPEED 8

#define DELAY_MS 200
#define MAX_REF_COUNT 5
#define INT_VALUE 4
#define MIN_AUDIO_SOURCE_VALUE 0
#define MAX_AUDIO_SOURCE_VALUE 3

#define FADER_BALANCE_MIN_INPUT -1.0f
#define FADER_BALANCE_MAX_INPUT 1.0f

#define FADER_BALANCE_SCALE 5.0f

#define PADDING_8BYTE_ALIGN(x)  ((((x) + 7) & 7) ^ 7)

#define INVALID_INIT_VALUE  -255

static int gs_VehicleSpeed = INVALID_INIT_VALUE ;
static int gs_audioSourceType = INVALID_INIT_VALUE;
static int gs_Balance = INVALID_INIT_VALUE;
static int gs_Fader = INVALID_INIT_VALUE;
static int gs_FanSpeed = INVALID_INIT_VALUE;
static int gs_SDVCPofile = INVALID_INIT_VALUE;
static pal_stream_handle_t* gsp_palHandle = NULL;
static int gs_powerpolicy = INVALID_INIT_VALUE;

namespace {

using aidl::android::hardware::automotive::vehicle::VehicleProperty;
using android::base::EqualsIgnoreCase;
using android::frameworks::automotive::vhal::ISubscriptionClient;
using android::frameworks::automotive::vhal::IVhalClient;
using ::android::hardware::automotive::vehicle::toInt;
using ::android::frameworks::automotive::vhal::VhalClientResult;
using ::android::frameworks::automotive::vhal::IHalPropValue;
using ::aidl::android::hardware::automotive::vehicle::RawPropValues;
using namespace ::qti::audio::oem::config;

#ifdef ENABLE_VHAL_TEST_WITH_KITCHENSINK
const VehicleProperty SpeedpropertyId = VehicleProperty::HVAC_TEMPERATURE_SET;
#else
const VehicleProperty SpeedpropertyId = VehicleProperty::PERF_VEHICLE_SPEED;
#endif //ENABLE_VHAL_TEST_WITH_KITCHENSINK


std::map<std::string, int> SpeakerMap = {
    {"FrontCenter",0},
    {"FrontLeft",1},
    {"FrontRight",2},
    {"Center",3},
    {"Rear",4},
    {"RearLeft",5},
    {"RearRight", 6},
    {"Right", 7},
    {"Left", 8},
};

// Open the Dummy Stream for Set TKV Handling for SDVC and RBVM sources.
void open_stream() {

    struct pal_stream_attributes stream_dl_attr = {};
    struct pal_device devices[1] = {};
    struct pal_buffer_config outBufCfg = {0, 0, 0};
    uint32_t DL_buffer_size;
    void * DL_stream_buffer = nullptr;
    struct pal_channel_info ch_info;
    uint32_t ret = 0;

    ch_info.channels = 1;
    ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;
    stream_dl_attr.type = PAL_STREAM_PLAYBACK_BUS;
    stream_dl_attr.bus_addr = "BUS02_NAV_GUIDANCE2";
    stream_dl_attr.direction = PAL_AUDIO_OUTPUT;
    stream_dl_attr.out_media_config.sample_rate = 48000;
    stream_dl_attr.out_media_config.bit_width = 16;
    stream_dl_attr.out_media_config.ch_info = ch_info;
    stream_dl_attr.out_media_config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    devices[0].id = PAL_DEVICE_OUT_SPEAKER;;
    devices[0].config.sample_rate = 48000;
    devices[0].config.bit_width = 16;
    devices[0].config.ch_info = ch_info;
    devices[0].config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    LOG(DEBUG) << __func__ << " Opening Dummy Stream handle";
    ret = pal_stream_open(&stream_dl_attr, 1, &devices[0], 0, NULL, NULL, 0, &gsp_palHandle);

    if(ret)
    {
        LOG(DEBUG) << " Stream open Failed " << __func__;
        gsp_palHandle = NULL ;
    }

    DL_buffer_size= 512*16*ch_info.channels;
    DL_stream_buffer = realloc(DL_stream_buffer, DL_buffer_size);
    outBufCfg.buf_size = DL_buffer_size;
    outBufCfg.buf_count = 4;
    ret = pal_stream_set_buffer_size(gsp_palHandle, NULL, &outBufCfg);

    ret = pal_stream_start(gsp_palHandle);

    if(ret)
    {
        LOG(DEBUG) << " Stream start Failed " << __func__;
        gsp_palHandle = NULL ;
    }
    return;

}

// Close the Dummy Stream
void close_stream()
{
    if (gsp_palHandle)
        pal_stream_close(gsp_palHandle);
}

// Helper to subscribe to VHal notifications
bool subscribeToVHal(ISubscriptionClient* client, VehicleProperty propertyId) {
    // Register for vehicle state change callbacks we care about
    std::vector<aidl::android::hardware::automotive::vehicle::SubscribeOptions> options = {
            {
                    .propId = static_cast<int32_t>(propertyId),
                    .areaIds = {},
            }
    };
    auto result = client->subscribe(options);
    if (!result.ok()) {
        LOG(ERROR) << "VHAL subscription for property " << static_cast<int32_t>(propertyId)
                     << "error" << result.error().message();
        return false;
    } else {
        LOG(DEBUG) << "VHAL subscription for propertyId = " << static_cast<int32_t>(propertyId) << " Success";
    }

    return true;
}

}

void AudioVehicleListener::onPropertyEvent(const std::vector<std::unique_ptr<IHalPropValue>>& values) {
    for(const auto& value : values) {
        int32_t propId = value->getPropId();
        LOG(VERBOSE) << __func__ << ": PropId : " << propId;
        int32_t area = value->getAreaId();
        LOG(VERBOSE) << __func__ << ": areaId : " << area;
        if (value->getPropId() == static_cast<int32_t>(SpeedpropertyId)) {
            if (value->getFloatValues().size() < 1) {
                LOG(ERROR) << "Invalid PERF_VEHICLE_SPEED getFloatValues size, empty value :" << value->getFloatValues().size();
                goto exit;
            } else {
                LOG(VERBOSE) << "Event Notify: New Vehicle Speed event received. Val:" << value->getFloatValues()[0];
                auto status = set_vehicle_speed(value->getFloatValues()[0]);
                if (status != 0) {
                    LOG(ERROR) << "Failed to set perf_vehicle_speed";
                    goto exit;
                }
            }
        }
        if (value->getPropId() == static_cast<int32_t>(VehicleProperty::HVAC_FAN_SPEED)) {
            if (value->getInt32Values().size() < 1) {
                LOG(ERROR) << "Invalid HVAC_FAN_SPEED getInt32Values size, empty value :" << value->getFloatValues().size();
                goto exit;
            } else {
                LOG(DEBUG) << "Event Notify: New HVAC Fan Speed event received. Val:" << value->getInt32Values()[0];
                auto status = set_fan_speed(value->getInt32Values()[0]);
                if (status != 0) {
                    LOG(ERROR) << "Failed to set hvac_fan_speed";
                    goto exit;
                }
            }
        }
    }

exit:
    LOG(DEBUG) << __func__ << ": Exit ";
}
void  set_spatilisation(struct str_parms *parms)
{
    char value[256];
    LOG(DEBUG) << __func__ << ": parameters : " << str_parms_to_str(parms);
    int ret = str_parms_get_str(parms, "spacialization_area", value, sizeof(value));
    if (ret >= 0)
    {
        LOG(DEBUG)<< __func__<<": value:"<< value;
        std::string key(value);
        auto it = SpeakerMap.find(key);

        if (it != SpeakerMap.end()) {
            LOG(DEBUG) << __func__ << "Found: " << it->first << " -> " << it->second << std::endl;
            ::aidl::qti::awx::VolumeParams spparams;
            ::aidl::qti::awx::pal_awx_param_t *pal_param = (aidl::qti::awx::pal_awx_param_t *)malloc(sizeof(aidl::qti::awx::pal_awx_param_t));
            if (pal_param != NULL)
            {
                pal_param->param_id = SPATIALISATION_PARAM_ID;
                spparams.eq_mask = 0x7E ; // Set for all bus
                spparams.value[0] = it->second;
                pal_param->param_size = sizeof(::aidl::qti::awx::VolumeParams);
                pal_param->data = &spparams;

                ::aidl::qti::awx::PalParamDelegator::AWX_set_param(pal_param, aidl::qti::awx::SYNC_WITH_AUDIO_BUS,true);
                free(pal_param);
            }

        } else {
            LOG(DEBUG) << "spacialization_area not found: " << key << std::endl;
        }

    }
}

void update_sdvc(param_type2_t *params) {
    LOG(VERBOSE) << "Enter " << __func__;

    aidl::qti::awx::pal_awx_param_t *pal_param = (aidl::qti::awx::pal_awx_param_t *)malloc(sizeof(aidl::qti::awx::pal_awx_param_t));

    if (params != NULL) {
        if (pal_param != NULL) {
            pal_param->param_id = PARAM_ID_SDVC;
            pal_param->param_size = sizeof(param_type2_t);
            pal_param->data = (void *)params;
            LOG(VERBOSE) << __func__ << ": Successfully created pal_param";
            if (gs_powerpolicy) {
                if (gsp_palHandle == NULL) {
                    open_stream();
                }

                // Check the pal Handle is not NULL before processing the request.
                if (gsp_palHandle != NULL) {
                    aidl::qti::awx::PalParamDelegator::AWX_set_param_handle(gsp_palHandle,pal_param, aidl::qti::awx::SYNC_WITHOUT_AUDIO_BUS);
                }
            }
        } else {
            LOG(ERROR) << "pal_param is null";
            goto exit;
        }
    } else {
        LOG(ERROR) << "Input Param is null";
    }

exit:
    if (pal_param) {
        free(pal_param);
    }

    LOG(DEBUG) << __func__ << ": Exit ";
}


void set_sdvc(struct str_parms *parms)
{
    char value[256];
    LOG(DEBUG) << __func__ << ": parameters : " << str_parms_to_str(parms);
    int ret = str_parms_get_str(parms, "sdvcprofile", value, sizeof(value));

    if (ret >= 0) {
        int result = atoi(value);
        gs_SDVCPofile = result;
        LOG(DEBUG)<< __func__<<": value:"<< value << " result:" << result;
        struct param_type2_t sdvc_data;
        sdvc_data.value = gs_SDVCPofile;
        update_sdvc(&sdvc_data);

    }
}

void set_powerpolicy(struct str_parms *parms)
{
    char value[256];
    LOG(DEBUG) << __func__ << ": parameters : " << str_parms_to_str(parms);
    int ret = str_parms_get_str(parms, "power_policy", value, sizeof(value));

    if (ret >= 0) {
        int result = atoi(value);
        LOG(DEBUG)<< __func__<<": value:"<< value << " result:" << result;

        // Power Policy is enabled
        if (result) {
            gs_powerpolicy = 1U;
            // Open Graph and gsp_palHandle
            open_stream();
        } else {
            // Close Graph and update gsp_palHandle to NULL
            gs_powerpolicy = 0U;
            close_stream();
        }
    }
}

int find_source_type(struct str_parms *parms) {
    LOG(DEBUG) << __func__ << ": Enter " ;
    int ret = 0;
    int source_type = -1;
    char value[256];
    LOG(DEBUG) << __func__ << ": parameters : " << str_parms_to_str(parms);

    ret = str_parms_get_str(parms, "AudioSource", value, sizeof(value));
    if (ret >= 0) {
        LOG(DEBUG)<< __func__<<": value:"<< value;
        if (strncmp(value, "FM", 2) == 0) {
            source_type = 0;
        } else if (strncmp(value, "AM", 2) == 0) {
            source_type = 1;
        } else if (strncmp(value, "DAB", 3) == 0) {
            source_type = 2;
        } else {
            source_type = 3;
        }
    }
    LOG(DEBUG) << __func__ << " audio source type =" << source_type;
    return source_type;
}

void update_audiosourcedata(struct pal_awx_source_data *source_data) {
    LOG(DEBUG) << "Enter " << __func__;
    aidl::qti::awx::pal_awx_param_t *pal_param = (aidl::qti::awx::pal_awx_param_t *)malloc(sizeof(aidl::qti::awx::pal_awx_param_t));
    if (pal_param != NULL && source_data != NULL) {
        pal_param->param_id = EQ_MODE_PARAM_ID;
        pal_param->param_size = sizeof(pal_awx_source_data);
        source_data->eq_mask = EQ_MASK_SOURCE_TYPE;
        pal_param->data = (void *)source_data;

        aidl::qti::awx::PalParamDelegator::AWX_set_param(pal_param, aidl::qti::awx::ASYNC);
    } else {
        LOG(ERROR) << "pal_param is null";
        goto exit;
    }

    if (pal_param) {
        free(pal_param);
    }

exit:
    LOG(DEBUG) << __func__ << ": Exit ";
}

int set_oem_audio_source_params(struct str_parms *parms) {
    int ret =0;
    struct pal_awx_source_data source_data;

    memset(&source_data, 0, sizeof(struct pal_awx_source_data));

    ret = find_source_type(parms);
    if (ret >= MIN_AUDIO_SOURCE_VALUE && ret <= MAX_AUDIO_SOURCE_VALUE) {
        LOG(DEBUG) << __func__ << ": valid audio source type" ;
        source_data.value[EQ_MASK_SOURCE_VAL] = ret;
        // cache the previous source data
        gs_audioSourceType = ret;
        update_audiosourcedata(&source_data);
    } else {
        LOG(WARNING) << __func__ << ": invalid audio source type" ;
    }
    return ret;
}
int set_vehicle_speed(int32_t val)
{
    LOG(DEBUG) << __func__ << ": Enter " ;
    struct param_type2_t speed_data;
    memset(&speed_data, 0, sizeof(struct param_type2_t));
    speed_data.value = val;
    if( gs_VehicleSpeed == val) {
        LOG(VERBOSE) << __func__ << "vechile speed is same as previous so we can  ignore" ;
        return EXIT_SUCCESS;
    }
    // cache the vehicle Speed.
    gs_VehicleSpeed = val ;

    if (speed_data.value >= MIN__PERF_VEHICLE_SPEED && speed_data.value <= MAX_PERF_VEHICLE_SPEED) {
        update_vehicle_speed_pal_param(&speed_data);
    } else {
        LOG(ERROR) << __func__ << ": invalid Vehicle speed: " << speed_data.value;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
int set_fan_speed(int32_t val) {
    LOG(DEBUG) << __func__ << ": Enter " ;
    struct param_type2_t speed_data;
    memset(&speed_data, 0, sizeof(struct param_type2_t));
    speed_data.value = val;
    gs_FanSpeed = val;

    if (speed_data.value >= MIN_HVAC_FAN_SPEED && speed_data.value <= MAX_HVAC_FAN_SPEED) {
        update_fan_speed_pal_param(&speed_data);
    } else {
        LOG(ERROR) << __func__ << ": invalid fan speed: " << speed_data.value;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

void update_fan_speed_pal_param(param_type2_t *params) {
    LOG(DEBUG) << ": Enter " << __func__;

    aidl::qti::awx::pal_awx_param_t *pal_param = (aidl::qti::awx::pal_awx_param_t *)malloc(sizeof(aidl::qti::awx::pal_awx_param_t));

    if (params != NULL) {
        if (pal_param != NULL) {
            pal_param->param_id = HVAC_FAN_SPEED_PARAM_ID;
            pal_param->param_size = sizeof(param_type2_t);
            pal_param->data = (void *)params;
            LOG(DEBUG) << __func__ << ": Successfully created pal_param";

            aidl::qti::awx::PalParamDelegator::AWX_set_param(pal_param, aidl::qti::awx::SYNC_WITHOUT_AUDIO_BUS);
        } else {
            LOG(ERROR) << ": pal_param is null";
            goto exit;
        }
    } else
    {
        LOG(ERROR) << ": input params is null";
    }
exit:
    if (pal_param) {
        free(pal_param);
    }

    LOG(DEBUG) << __func__ << ": Exit ";
}

void update_vehicle_speed_pal_param(param_type2_t *params) {
    LOG(VERBOSE) << "Enter " << __func__;

    aidl::qti::awx::pal_awx_param_t *pal_param = (aidl::qti::awx::pal_awx_param_t *)malloc(sizeof(aidl::qti::awx::pal_awx_param_t));

    if (params != NULL) {
        if (pal_param != NULL) {
            pal_param->param_id = PERF_VEHICLE_SPEED_PARAM_ID;
            pal_param->param_size = sizeof(param_type2_t);
            pal_param->data = (void *)params;
            LOG(VERBOSE) << __func__ << ": Successfully created pal_param";
            aidl::qti::awx::PalParamDelegator::AWX_set_param(pal_param, aidl::qti::awx::SYNC_WITHOUT_AUDIO_BUS);
        } else {
            LOG(ERROR) << "pal_param is null";
            goto exit;
        }
    } else {
        LOG(ERROR) << "Input Param is null";
    }

exit:
    if (pal_param) {
        free(pal_param);
    }

    LOG(DEBUG) << __func__ << ": Exit ";
}

int convertFloatToInt(float value) {
    // Ensure the value is within the expected range
    if (value < FADER_BALANCE_MIN_INPUT) {
        value = FADER_BALANCE_MIN_INPUT;
    }
    if (value > FADER_BALANCE_MAX_INPUT) {
        value = FADER_BALANCE_MAX_INPUT;
    }

    // Scale the value from [-1.0, 1.0] to [-5, 5]
    float scaledValue = value * FADER_BALANCE_SCALE;

    // Round to the nearest integer
    int intValue = static_cast<int>(round(scaledValue));

    return intValue;
}

void  set_type2_param(int paramID, int ParamValue)
{
    aidl::qti::awx::pal_awx_param_t *pal_param = (aidl::qti::awx::pal_awx_param_t *)malloc(sizeof(aidl::qti::awx::pal_awx_param_t));
    struct param_type2_t type2params;

    LOG(DEBUG) << "Enter " << __func__;

    if (pal_param != NULL) {
        pal_param->param_id = paramID;
        pal_param->param_size = sizeof(param_type2_t);
        pal_param->data = (void *)&type2params;
        type2params.value = ParamValue;
        LOG(DEBUG) << __func__ << ": Successfully created pal_param";

        aidl::qti::awx::PalParamDelegator::AWX_set_param(pal_param, aidl::qti::awx::SYNC_WITHOUT_AUDIO_BUS);
    } else {
        LOG(ERROR) << "pal_param is null";
        goto exit;
    }

exit:
    if (pal_param) {
        free(pal_param);
    }

    LOG(DEBUG) << __func__ << ": Exit ";
}


int setGeometryParam(struct str_parms *parms)
{
    int ret = 0;

    if (parms != NULL) {
        char value[256];

        ret = str_parms_get_str(parms, "Fader", value, sizeof(value));

        if (ret >= 0) {
            float valueinFloat;
            char *endptr = NULL;

            valueinFloat = strtof(value, &endptr);
            if (endptr == value) {
                LOG(ERROR) << "No digits were found\n";
            } else {
                LOG(ERROR) << "The float value is:" << valueinFloat;
            }

            int awxValue = convertFloatToInt(valueinFloat);
            int paramID = FADER_PARAM_ID;
            LOG(DEBUG) << "Fader Converted  Value " << awxValue << "ParamId " << paramID;
            gs_Fader = awxValue;
            set_type2_param(paramID,awxValue);
        }


        ret = str_parms_get_str(parms, "Balance", value, sizeof(value));

        if (ret >= 0) {
            float valueinFloat;
            char *endptr = NULL;
            valueinFloat = strtof(value, &endptr);
            if (endptr == value) {
                LOG(ERROR) << "No digits were found\n";
            } else {
                LOG(ERROR) << "The float value is:" << valueinFloat;
            }

            int awxValue = convertFloatToInt(valueinFloat);
            int paramID = BALANCE_PARAM_ID;
            // cache the previous value
            gs_Balance = awxValue;
            LOG(DEBUG) << "Balance awx Value" << awxValue << "ParamId " << paramID;
            set_type2_param(paramID,awxValue);
        }
    }
    else {
        LOG(ERROR) << "input Params are NULL\n";
    }

    return ret;

}

int oem_pal_param_update(const std::string& id) {
    int ret = -1;
    aidl::qti::awx::pal_awx_param_t pal_param;
    param_type2_t params;
    int cachedValue = 0;

    memset(&pal_param, 0, sizeof(aidl::qti::awx::pal_awx_param_t));
    memset(&params, 0, sizeof(param_type2_t));
    if (id == "Balance") {
        pal_param.param_id = BALANCE_PARAM_ID;
        if (gs_Balance != INVALID_INIT_VALUE)
            cachedValue = gs_Balance;
    } else if (id == "Fader") {
        pal_param.param_id = FADER_PARAM_ID;
        if (gs_Fader != INVALID_INIT_VALUE)
            cachedValue = gs_Fader;
    } else if (id == "isFaderAvailable")
    {
        ::qti::audio::oem::config::AudioConfigType req = AUDIO_CONFIG_FADER_AVAILABLITY ;
        ::qti::audio::oem::config::AudioConfigData configData;
        ::qti::audio::oem::config::AudioConfigManager::getInstance().getAudioConfigValue(req,&configData);
        std::string s = std::to_string(configData.defaultValue);
        LOG(DEBUG) << "String " << s << " Integer " << configData.defaultValue;
        return configData.defaultValue;
    } else {
        LOG(ERROR) << __func__ << ": invalid audio parameter id";
        return ret;
    }

    pal_param.param_size = sizeof(param_type2_t);
    pal_param.data = &params;
    // Defining CAPI param Type
    param_type capi_param_type = Type2;

    ret = aidl::qti::awx::PalParamDelegator::AWX_get_param(&pal_param, aidl::qti::awx::SYNC_WITHOUT_AUDIO_BUS); //get_vendor_params(&pal_param, capi_param_type);
    if (ret < 0) {
        ret = cachedValue;
        LOG(WARNING) << __func__ << "Error while fetching value return: returning Cached Value" << ret;
       return ret;
    } else {
        LOG(DEBUG) << __func__ << ": Parameter fetched successfully! get_param return val: " << params.value;
    }

    LOG(DEBUG) << "Exit " << __func__;
    return params.value;
}

extern "C" __attribute__((visibility("default")))int oem_init(void)
{
    int retValue = EXIT_SUCCESS ;
    // Construct our async helper object
    std::shared_ptr<AudioVehicleListener> pAudioListener = std::make_shared<AudioVehicleListener>();
    // Connect to the Vehicle HAL so we can monitor state
    std::shared_ptr<IVhalClient> pVnet;
    LOG(INFO) << "Connecting to Vehicle HAL";
    pVnet = IVhalClient::create();
    if (pVnet == nullptr) {
        LOG(ERROR) << "Vehicle HAL getService returned NULL.  Exiting.";
        retValue = EXIT_FAILURE;
    } else {
        auto subscriptionClient = pVnet->getSubscriptionClient(pAudioListener);
        // Register for vehicle state change callbacks we care about
        // Changes in these values are what will trigger a reconfiguration.
        LOG(DEBUG) << "Subscribing VHAL property Speed " << static_cast<int32_t>(SpeedpropertyId);
        if (!subscribeToVHal(subscriptionClient.get(), SpeedpropertyId)) {
            LOG(ERROR) << "Didn't register for PERF_VEHICLE_SPEED , Exiting.";
            retValue = EXIT_FAILURE;
        }
        else {
            LOG(DEBUG) << "regiter for PERF_VEHICLE_SPEED done.";
        }

        LOG(DEBUG) << "Subscribing VHAL property HVAC " << static_cast<int32_t>(VehicleProperty::HVAC_FAN_SPEED);
        if (!subscribeToVHal(subscriptionClient.get(), VehicleProperty::HVAC_FAN_SPEED)) {
            LOG(ERROR) << "Didn't register for  HVAC_FAN_SPEED notification, Exiting.";
            retValue = EXIT_FAILURE;
        }
        else {
            LOG(DEBUG) << "Register for HVAC_FAN_SPEED done.";
        }
    }

    ::qti::audio::oem::config::AudioConfigType req = AUDIO_CONFIG_MAX_VOL_STARTUP ;
    ::qti::audio::oem::config::AudioConfigData configData;
    ::qti::audio::oem::config::AudioConfigManager::getInstance().getAudioConfigValue(req,&configData);
    std::string s = std::to_string(configData.defaultValue);
    LOG(VERBOSE) << "String " << s << " Integer " << configData.defaultValue;
    property_set("persist.vendor.max_vol_startup",s.c_str());

    // Set property for default Ambiance
    ::qti::audio::oem::config::AudioConfigData ambConfigData;
    ::qti::audio::oem::config::AudioConfigManager::getInstance().getAudioConfigValue(AUDIO_CONFIG_DEFAULT_AMBIANCE,&ambConfigData);
    std::string ambianceString = std::to_string(ambConfigData.defaultValue);
    LOG(VERBOSE) << "String " << s << " Integer " << ambConfigData.defaultValue;
    property_set("persist.vendor.default_ambiance",ambianceString.c_str());

    // Set Property for default AGC state
    ::qti::audio::oem::config::AudioConfigData agcConfigData;
    ::qti::audio::oem::config::AudioConfigManager::getInstance().getAudioConfigValue(AUDIO_CONFIG_DEFAULT_AGC_STATE,&agcConfigData);
    std::string agcS = std::to_string(agcConfigData.defaultValue);
    LOG(VERBOSE) << "String " << s << " Integer " << agcConfigData.defaultValue;
    property_set("persist.vendor.default_agc",agcS.c_str());

    return retValue;

}

extern "C" __attribute__((visibility("default")))void oem_set_parameters(struct str_parms *parms) {

    int ret = 0;
    // AudioManager.setvendorparams()
       if (parms != NULL) {
            ret = set_oem_audio_source_params(parms);

            // if Source Params are found no need to check for Geometry
            if (ret != 0)
            {
              setGeometryParam(parms);
              set_spatilisation(parms);
              set_sdvc(parms);
              set_powerpolicy(parms);
            }
        }
        else {
            LOG(ERROR) << "audio source params are null, unable to proceed ";
            goto exit;
        }

exit:
    LOG(DEBUG) << __func__ << ": Exit";
}

extern "C" __attribute__((visibility("default")))int oem_get_parameters(const std::string& id){
    int status = 0;
    int ret =0;

    // AudioManager.getvendorparams()
    status = oem_pal_param_update(id);
    if (status < 0) {
        LOG(ERROR) << "oem_pal_param_update failed with ret:" << status;
    } else {
        LOG(DEBUG) << "oem_pal_param_update get val: " << status;
    }
    LOG(DEBUG) << __func__ << ": Exit";
    return status;
}

extern "C" __attribute__((visibility("default"))) void streamInfo(pal_stream_type_t streamType)
{
    oemStreamType = streamType;
    LOG(DEBUG) << __func__ << " oemStreamType: " << oemStreamType;

    if (gs_VehicleSpeed != INVALID_INIT_VALUE ) {
        set_vehicle_speed(gs_VehicleSpeed);
    }
    if (gs_audioSourceType != INVALID_INIT_VALUE ) {
        struct pal_awx_source_data source_data;
        source_data.value[EQ_MASK_SOURCE_VAL] = gs_audioSourceType;
        update_audiosourcedata(&source_data);
    }
    if (gs_Balance != INVALID_INIT_VALUE) {
        set_type2_param(BALANCE_PARAM_ID,gs_Balance);
    }
    if (gs_Fader != INVALID_INIT_VALUE) {
        set_type2_param(FADER_PARAM_ID,gs_Fader);
    }
    if (gs_FanSpeed != INVALID_INIT_VALUE) {
        set_fan_speed(gs_FanSpeed);
    }
}
