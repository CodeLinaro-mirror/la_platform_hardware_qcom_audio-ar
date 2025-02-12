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
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <include/extensions/auto_oem_extension.h>
#include "PalApi.h"
#include <include/extensions/AudioVehicleListener.h>
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

#define AWX_MODULE_CUSTOM_TAG 0XC0000057

#define EQ_MASK_SOURCE_TYPE  0x0004
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


namespace {

using aidl::android::hardware::automotive::vehicle::VehicleProperty;
using android::base::EqualsIgnoreCase;
using android::frameworks::automotive::vhal::ISubscriptionClient;
using android::frameworks::automotive::vhal::IVhalClient;
using ::android::hardware::automotive::vehicle::toInt;
using ::android::frameworks::automotive::vhal::VhalClientResult;
using ::android::frameworks::automotive::vhal::IHalPropValue;
using ::aidl::android::hardware::automotive::vehicle::RawPropValues;

#ifndef ENABLE_VHAL_TEST_WITH_KITCHENSINK
#define ENABLE_VHAL_TEST_WITH_KITCHENSINK
const VehicleProperty SpeedpropertyId = VehicleProperty::HVAC_TEMPERATURE_SET;
const VehicleProperty HVACpropertyId = VehicleProperty::HVAC_RECIRC_ON;
#else
const VehicleProperty SpeedpropertyId = VehicleProperty::PERF_VEHICLE_SPEED;
const VehicleProperty HVACpropertyId = VehicleProperty::HVAC_FAN_SPEED;
#endif //ENABLE_VHAL_TEST_WITH_KITCHENSINK

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
    }
    else
    {
        LOG(DEBUG) << "VHAL subscription for propertyId = " << static_cast<int32_t>(propertyId) << " Success";
    }

    return true;
}

}

void AudioVehicleListener::onPropertyEvent(const std::vector<std::unique_ptr<IHalPropValue>>& values) {
    for(const auto& value : values) {
        int32_t propId = value->getPropId();
        LOG(DEBUG) << __func__ << ": PropId : " << propId;
        int32_t area = value->getAreaId();
        LOG(DEBUG) << __func__ << ": areaId : " << area;
        if (value->getPropId() == static_cast<int32_t>(SpeedpropertyId)) {
            if (value->getFloatValues().size() < 1) {
                LOG(ERROR) << "Invalid PERF_VEHICLE_SPEED getFloatValues size, empty value :" << value->getFloatValues().size();
                goto exit;
            } else {
                LOG(DEBUG) << "Event Notify: New Vehicle Speed event received. Val:" << value->getFloatValues()[0];
                auto status = set_vehicle_speed(value->getFloatValues()[0]);
                if (status != 0) {
                    LOG(ERROR) << "Failed to set perf_vehicle_speed";
                    goto exit;
                }
            }
        }
        if (value->getPropId() == static_cast<int32_t>(HVACpropertyId)) {
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
        }
        else if (strncmp(value, "AM", 2) == 0) {
            source_type = 1;
        }
        else if (strncmp(value, "DAB", 3) == 0) {
            source_type = 2;
        }
        else {
            source_type = 3;
        }
    }
    LOG(DEBUG) << __func__ << " audio source type =" << source_type;
    return source_type;
}

void update_audiosourcedata(struct pal_awx_source_data *source_data) {
    LOG(DEBUG) << "Enter " << __func__;
    int status = 0;
    struct pal_awx_param_t *pal_param = (struct pal_awx_param_t *)malloc(sizeof(struct pal_awx_param_t));
    if (pal_param != NULL && source_data != NULL) {
        pal_param->param_id = EQ_MODE_PARAM_ID;
        pal_param->param_size = sizeof(pal_awx_source_data);
        source_data->eq_mask = EQ_MASK_SOURCE_TYPE;
        pal_param->data = (void *)source_data;

        audiosource_set_param(pal_param);
    }
    else {
        LOG(ERROR) << "pal_param is null";
        goto exit;
    }


    if (pal_param) {
        free(pal_param);
    }

exit:
    LOG(DEBUG) << __func__ << ": Exit ";
}
void audiosource_set_param(struct pal_awx_param_t* pal_param) {
    LOG(DEBUG) << __func__ <<": Enter ";
    int status = 0;
    size_t padBytes = 0;
    uint8_t* payloadInfo = NULL;
    pal_param_payload* pal_payload = nullptr;
    effect_pal_payload_t* effect_payload = nullptr;
    pal_effect_custom_payload_t* customPayload = nullptr;
    pal_awx_param_t* data = pal_param;
    uint32_t pal_param_size = data->param_size;
    pal_device_id_t aud_source_effect_device = PAL_DEVICE_OUT_SPEAKER;
    pal_awx_source_data *dummy_ptr = nullptr;
    //CAPI param Type
    param_type capi_param_type = Type3;

    uint32_t payload_size = sizeof(pal_param_payload) + sizeof(effect_pal_payload_t) + sizeof(pal_effect_custom_payload_t) + pal_param_size;

    padBytes = PADDING_8BYTE_ALIGN(payload_size);

    payloadInfo = (uint8_t *) calloc(1, payload_size + padBytes);
    if (!payloadInfo) {
        status = -ENOMEM;
        goto exit;
    }

    pal_payload = (pal_param_payload*) payloadInfo;
    effect_payload = (effect_pal_payload_t*) (payloadInfo + sizeof(pal_param_payload));
    customPayload = (pal_effect_custom_payload_t*) (payloadInfo + sizeof(pal_param_payload) + sizeof(effect_pal_payload_t));

    pal_payload->payload_size = sizeof(effect_pal_payload_t) + sizeof(pal_effect_custom_payload_t) + pal_param_size;

    effect_payload->isTKV = PARAM_NONTKV;
    effect_payload->tag = AWX_MODULE_CUSTOM_TAG;
    effect_payload->payloadSize = sizeof(pal_effect_custom_payload_t) + pal_param_size;

    customPayload->paramId = data->param_id;

    if (data->data != NULL) {
        memcpy(customPayload->data, data->data, pal_param_size);
    }
    else {
        LOG(DEBUG) << __func__ << ": data is invalid";
        goto exit;
    }

    dummy_ptr = (pal_awx_source_data *)&(customPayload->data[0]);
    LOG(DEBUG) << __func__ << " PARAM ID: " << std::hex << customPayload->paramId;
    LOG(DEBUG) << __func__ << " EQ_mask: " << std::hex << dummy_ptr->eq_mask;
    LOG(DEBUG) << __func__ << " value[2]: " << dummy_ptr->value[2];

    status = pal_gef_rw_param(PAL_PARAM_ID_UIEFFECT, (void *) pal_payload, payload_size, aud_source_effect_device, PAL_STREAM_PLAYBACK_BUS, GEF_PARAM_WRITE, NULL);

    if (capi_param_type == Type3) {
        status = capi_param_type3_handling(status, pal_payload, payload_size, aud_source_effect_device, dummy_ptr);
    }

    if (status != 0) {
        LOG(ERROR) << "Error setting param with error " << status;
        goto exit;
    } else {
        LOG(INFO) << __func__ << ": Set parameter successfully";
        goto exit;
    }

exit:
    if (payloadInfo) {
        free(payloadInfo);
    }

    pal_payload = nullptr;
    effect_payload = nullptr;
    customPayload = nullptr;
    data = nullptr;
    dummy_ptr = nullptr;

    LOG(DEBUG) << __func__ << ": Exit";
}

int capi_param_type3_handling(int status, pal_param_payload* pal_payload, uint32_t payload_size,
                            pal_device_id_t aud_source_effect_device, pal_awx_source_data *dummy_ptr) {
    LOG(DEBUG) << __func__ << ": Enter ";
    if (status == -1) {
        bool idle = false;
        int read_status = 0;
        int ref_count = 0;
        while ((!idle) && (ref_count < MAX_REF_COUNT)) {
            read_status = pal_gef_rw_param(PAL_PARAM_ID_UIEFFECT, (void*) pal_payload, payload_size,
                                        aud_source_effect_device, PAL_STREAM_PLAYBACK_BUS, GEF_PARAM_READ, NULL);
            int num = EQ_MASK_SOURCE_TYPE;
            LOG(DEBUG) << __func__ << " read eq_mask: " << std::hex << num << " done.";
            num &= MASK;
            LOG(DEBUG) << __func__ << " read status: " << num << " done.";

            //Check eq_mask status of Harman AWX modules: 0 for idle, 1 for busy
            if (num == INT_VALUE) {
                idle = true;
                status = 0;
            } else {
                ref_count++;
                //Type3 is asynchronous, sleep is required if status is busy
                sleep(DELAY_MS);
            }
        }
    }
    LOG(DEBUG) << __func__ << ": Exit ";
    return status;
}

int set_oem_audio_source_params(struct str_parms *parms) {
    int ret =0;
    struct pal_awx_source_data source_data;

    memset(&source_data, 0, sizeof(struct pal_awx_source_data));

    ret = find_source_type(parms);
    if (ret >= MIN_AUDIO_SOURCE_VALUE && ret <= MAX_AUDIO_SOURCE_VALUE)
    {
        LOG(DEBUG) << __func__ << ": valid audio source type" ;
        source_data.value[2] = ret;
        update_audiosourcedata(&source_data);
    }
    else {
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
    if (speed_data.value >= MIN_HVAC_FAN_SPEED && speed_data.value <= MAX_HVAC_FAN_SPEED) {
        update_fan_speed_pal_param(&speed_data);
    } else {
        LOG(ERROR) << __func__ << ": invalid fan speed: " << speed_data.value;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
int AWX_set_param_sync(struct pal_awx_param_t* pal_param) {
    LOG(DEBUG) << __func__ <<": Enter ";
    int status = 0;
    size_t padBytes = 0;
    uint8_t* payloadInfo = NULL;
    pal_param_payload* pal_payload = nullptr;
    effect_pal_payload_t* effect_payload = nullptr;
    pal_effect_custom_payload_t* customPayload = nullptr;
    pal_awx_param_t* data = pal_param;
    uint32_t pal_param_size = data->param_size;
    pal_device_id_t aud_source_effect_device = PAL_DEVICE_OUT_SPEAKER;

    uint32_t payload_size = sizeof(pal_param_payload) + sizeof(effect_pal_payload_t) + sizeof(pal_effect_custom_payload_t) + pal_param_size;

    padBytes = PADDING_8BYTE_ALIGN(payload_size);

    payloadInfo = (uint8_t *) calloc(1, payload_size + padBytes);
    if (!payloadInfo) {
        status = -ENOMEM;
        goto exit;
    }

    pal_payload = (pal_param_payload*) payloadInfo;
    effect_payload = (effect_pal_payload_t*) (payloadInfo + sizeof(pal_param_payload));
    customPayload = (pal_effect_custom_payload_t*) (payloadInfo + sizeof(pal_param_payload) + sizeof(effect_pal_payload_t));

    pal_payload->payload_size = sizeof(effect_pal_payload_t) + sizeof(pal_effect_custom_payload_t) + pal_param_size;

    effect_payload->isTKV = PARAM_NONTKV;
    effect_payload->tag = AWX_MODULE_CUSTOM_TAG;
    effect_payload->payloadSize = sizeof(pal_effect_custom_payload_t) + pal_param_size;

    customPayload->paramId = data->param_id;

    if (data->data != NULL) {
        memcpy(customPayload->data, data->data, pal_param_size);
    }
    else {
        LOG(ERROR) << __func__ << ": data is invalid";
        goto exit;
    }

    LOG(DEBUG) << __func__ << " PARAM ID: " << std::hex << customPayload->paramId <<"PARAM VALUE: " << pal_param->data;

    status = pal_gef_rw_param(PAL_PARAM_ID_UIEFFECT, (void *) pal_payload, payload_size, aud_source_effect_device, PAL_STREAM_PLAYBACK_BUS, GEF_PARAM_WRITE, NULL);

    if (status != 0) {
        LOG(ERROR) << "Error setting param with error " << status;
        goto exit;
    } else {
        LOG(INFO) << __func__ << ": Set parameter successfully";
        goto exit;
    }

exit:
    if (payloadInfo) {
        free(payloadInfo);
    }

    pal_payload = nullptr;
    effect_payload = nullptr;
    customPayload = nullptr;
    data = nullptr;

    LOG(DEBUG) << __func__ << ": Exit";
    return status;
}

void update_fan_speed_pal_param(struct param_type2_t *params) {
    LOG(DEBUG) << ": Enter " << __func__;

    struct pal_awx_param_t *pal_param = (struct pal_awx_param_t *)malloc(sizeof(struct pal_awx_param_t));
    int ret;

    if (params != NULL) {
        if (pal_param != NULL) {
            pal_param->param_id = HVAC_FAN_SPEED_PARAM_ID;
            pal_param->param_size = sizeof(param_type2_t);
            pal_param->data = (void *)params;
            LOG(DEBUG) << __func__ << ": Successfully created pal_param";
            ret = AWX_set_param_sync(pal_param);
            if (ret != 0)
            {
                LOG(WARNING) << __func__ << "AWX_set_param_sync failed ret:"<<ret;
            }
        }
        else {
            LOG(ERROR) << ": pal_param is null";
            goto exit;
        }
    }
    else
    {
        LOG(ERROR) << ": input params is null";
    }
exit:
    if (pal_param) {
        free(pal_param);
    }

    LOG(DEBUG) << __func__ << ": Exit ";
}

void update_vehicle_speed_pal_param(struct param_type2_t *params) {
    LOG(DEBUG) << "Enter " << __func__;

    struct pal_awx_param_t *pal_param = (struct pal_awx_param_t *)malloc(sizeof(struct pal_awx_param_t));
    int ret;

    if (params != NULL) {
        if (pal_param != NULL) {
            pal_param->param_id = PERF_VEHICLE_SPEED_PARAM_ID;
            pal_param->param_size = sizeof(param_type2_t);
            pal_param->data = (void *)params;
            LOG(DEBUG) << __func__ << ": Successfully created pal_param";
            ret = AWX_set_param_sync(pal_param);

            if (ret != 0)
            {
                LOG(WARNING) << __func__ << "AWX_set_param_sync failed ret:"<<ret;
            }
        }
        else {
            LOG(ERROR) << "pal_param is null";
            goto exit;
        }
    }
    else {
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
    if (value < FADER_BALANCE_MIN_INPUT)
    {
        value = FADER_BALANCE_MIN_INPUT;
    }
    if (value > FADER_BALANCE_MAX_INPUT)
    {
        value = FADER_BALANCE_MAX_INPUT;
    }

    // Scale the value from [-1.0, 1.0] to [-5, 5]
    float scaledValue = value * FADER_BALANCE_SCALE;

    // Round to the nearest integer
    int intValue = static_cast<int>(round(scaledValue));

    return intValue;
}

int  set_type2_param(int paramID, int ParamValue)
{
    struct pal_awx_param_t *pal_param = (struct pal_awx_param_t *)malloc(sizeof(struct pal_awx_param_t));
    struct param_type2_t type2params;
    int ret = -EINVAL ;

    LOG(DEBUG) << "Enter " << __func__;

    if (pal_param != NULL) {
        pal_param->param_id = paramID;
        pal_param->param_size = sizeof(param_type2_t);
        pal_param->data = (void *)&type2params;
        type2params.value = ParamValue;
        LOG(DEBUG) << __func__ << ": Successfully created pal_param";

        // Set Param of type 2
        ret = AWX_set_param_sync(pal_param);

        if (ret != 0)
        {
            LOG(WARNING) << __func__ << "AWX_set_param_sync failed ret:"<<ret;
        }
    }
    else {
        LOG(ERROR) << "pal_param is null";
        goto exit;
    }

exit:
    if (pal_param) {
        free(pal_param);
    }

    LOG(DEBUG) << __func__ << ": Exit ";
    return ret;
}


int setGeometryParam(struct str_parms *parms)
{
    int ret = 0;

    if (parms != NULL)
    {
        char value[256];

        ret = str_parms_get_str(parms, "Fader", value, sizeof(value));

        if (ret >= 0)
        {

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
            LOG(DEBUG) << "Converted  Value " << awxValue << "ParamId " << paramID;
            ret = set_type2_param(paramID,awxValue);
            if (ret != 0)
            {
                LOG(ERROR) << "set_type2_param Failed ret " <<ret;
            }
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
            LOG(DEBUG) << "awx Value" << awxValue << "{aramId " << paramID;
            ret = set_type2_param(paramID,awxValue);
            if (ret != 0)
            {
                LOG(ERROR) << "set_type2_param Failed ret " <<ret;
            }
        }
    }
    else
    {
        LOG(ERROR) << "input Params are NULL\n";
    }

    return ret;

}

int oem_pal_param_update(const std::string& id) {
    int ret = -1;
    struct pal_awx_param_t pal_param;
    param_type2_t params;

    memset(&pal_param, 0, sizeof(pal_awx_param_t));
    memset(&params, 0, sizeof(param_type2_t));
    if (id == "Balance") {
        pal_param.param_id = BALANCE_PARAM_ID;
    } else if ((id == "Fader") || (id == "isFaderAvailable")) {
        pal_param.param_id = FADER_PARAM_ID;
    } else {
        LOG(ERROR) << __func__ << ": invalid audio parameter id";
        return ret;
    }

    pal_param.param_size = sizeof(param_type2_t);
    pal_param.data = &params;
    // Defining CAPI param Type
    param_type capi_param_type = Type2;

    ret = get_vendor_params(&pal_param, capi_param_type);
    if (ret < 0) {
        LOG(ERROR) << __func__ << "Error while fetching value return: " << ret;
       return ret;
    } else {
        LOG(DEBUG) << __func__ << ": Parameter fetched successfully! get_param return val: " << params.value;
    }

    LOG(DEBUG) << "Exit " << __func__;
    return params.value;
}
int get_vendor_params(struct pal_awx_param_t* pal_param, param_type capi_param_type) {
    LOG(DEBUG) << __func__ <<": Enter ";
    int status = 0;
    size_t padBytes = 0;
    uint8_t* payloadInfo = NULL;
    pal_param_payload* pal_payload = nullptr;
    effect_pal_payload_t* effect_payload = nullptr;
    pal_effect_custom_payload_t* customPayload = nullptr;
    pal_awx_param_t* data = pal_param;
    uint32_t pal_param_size = data->param_size;
    pal_device_id_t aud_source_effect_device = PAL_DEVICE_OUT_SPEAKER;

    uint32_t payload_size = sizeof(pal_param_payload) + sizeof(effect_pal_payload_t) + sizeof(pal_effect_custom_payload_t) + pal_param_size;

    padBytes = PADDING_8BYTE_ALIGN(payload_size);

    payloadInfo = (uint8_t *) calloc(1, payload_size + padBytes);
    if (!payloadInfo) {
        status = -ENOMEM;
        goto cleanup;
    }

    pal_payload = (pal_param_payload*) payloadInfo;
    effect_payload = (effect_pal_payload_t*) (payloadInfo + sizeof(pal_param_payload));
    customPayload = (pal_effect_custom_payload_t*) (payloadInfo + sizeof(pal_param_payload) + sizeof(effect_pal_payload_t));

    pal_payload->payload_size = sizeof(effect_pal_payload_t) + sizeof(pal_effect_custom_payload_t) + pal_param_size;

    effect_payload->isTKV = PARAM_NONTKV;
    effect_payload->tag = AWX_MODULE_CUSTOM_TAG;
    effect_payload->payloadSize = sizeof(pal_effect_custom_payload_t) + pal_param_size;

    customPayload->paramId = data->param_id;

    LOG(DEBUG) << __func__ << " PARAM ID: " << std::hex << customPayload->paramId;

    status = pal_gef_rw_param(PAL_PARAM_ID_UIEFFECT, (void *) pal_payload, payload_size, aud_source_effect_device, PAL_STREAM_PLAYBACK_BUS, GEF_PARAM_READ, NULL);

    LOG(DEBUG) << __func__ << " value " << customPayload->data[0];

    memcpy(data->data, customPayload->data, pal_param_size);

cleanup:
    if (payloadInfo) {
        free(payloadInfo);
    }
    pal_payload = nullptr;
    effect_payload = nullptr;
    customPayload = nullptr;
    data = nullptr;

    LOG(DEBUG) << __func__ << ": Exit";
    return status;
}

extern "C" __attribute__((visibility("default")))int oem_init(void)
{
    // Construct our async helper object
    std::shared_ptr<AudioVehicleListener> pAudioListener = std::make_shared<AudioVehicleListener>();
    // Connect to the Vehicle HAL so we can monitor state
    std::shared_ptr<IVhalClient> pVnet;
    LOG(INFO) << "Connecting to Vehicle HAL";
    pVnet = IVhalClient::create();
    if (pVnet == nullptr) {
        LOG(ERROR) << "Vehicle HAL getService returned NULL.  Exiting.";
        return EXIT_FAILURE;
    } else {
        auto subscriptionClient = pVnet->getSubscriptionClient(pAudioListener);
        // Register for vehicle state change callbacks we care about
        // Changes in these values are what will trigger a reconfiguration.
        if (!subscribeToVHal(subscriptionClient.get(), SpeedpropertyId)) {
            LOG(ERROR) << "Didn't register for PERF_VEHICLE_SPEED , Exiting.";
            return EXIT_FAILURE;
        }
        else
        {
            LOG(ERROR) << "regiter for PERF_VEHICLE_SPEED done.";
        }

        if (!subscribeToVHal(subscriptionClient.get(), HVACpropertyId)) {
            LOG(ERROR) << "Didn't register for  HVAC_FAN_SPEED notification, Exiting.";
            return EXIT_FAILURE;
        }
        else
        {
            LOG(ERROR) << "Register for HVAC_FAN_SPEED done.";
        }
    }
    return EXIT_SUCCESS;

}

extern "C" __attribute__((visibility("default")))void oem_set_parameters(struct str_parms *parms) {

    int ret = 0;
    // AudioManager.setvendorparams()
    if (parms != NULL)
    {
        ret = set_oem_audio_source_params(parms);

        // if Source Params are found no need to check for Geometry
        if (ret != 0)
        {
            ret = setGeometryParam(parms);
        }
    }
    else
    {
        LOG(ERROR) << "audio source params are null, unable to proceed ";
        goto exit;
    }

    if (ret != 0)
    {
        LOG(WARNING) << "unable set the OEM params with error" << ret;
        goto exit;
    }
    else
    {
        LOG(DEBUG) << "set the audio source params done." << ret;
    }

exit:
    LOG(DEBUG) << __func__ << ": Exit";
}

extern "C" __attribute__((visibility("default")))int oem_get_parameters(const std::string& id){
    int status = 0;
    int ret =0;

    // AudioManager.getvendorparams()
    status = oem_pal_param_update(id);
    if (status < 0)
    {
        LOG(ERROR) << "oem_pal_param_update failed with ret:" << status;
    }
    else
    {
        LOG(DEBUG) << "oem_pal_param_update get val: " << status;
    }
    LOG(DEBUG) << __func__ << ": Exit";
    return status;
}
