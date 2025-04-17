/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL_Effect_PALPARAM_DELEGATOR"
#include "PalParamDelegator.h"
#include <cstddef>


pal_stream_type_t oemStreamType = PAL_STREAM_INVALID;

namespace aidl::qti::awx  {
// AWX set param for vendorExtension audio effect
void PalParamDelegator::AWX_set_param(pal_awx_param_t* param, effect_type effect) {
    LOG(DEBUG) << "Enter " << __func__;

    if (param == NULL){
        LOG(ERROR) << __func__ <<" Param is null ";
        return;
    }

    int status = 0;
    size_t padBytes = 0;
    pal_param_payload* pal_payload = nullptr;
    effect_pal_payload_t* effect_payload = nullptr;
    pal_effect_custom_payload_t* customPayload = nullptr;
    pal_awx_param_t* data = param;
    uint8_t* payloadInfo = NULL;
    uint32_t pal_param_size = data->param_size;
    pal_device_id_t aud_source_effect_device = PAL_DEVICE_OUT_SPEAKER;

    uint32_t payload_size = sizeof(pal_param_payload) + sizeof(effect_pal_payload_t)
                            + sizeof(pal_effect_custom_payload_t) + pal_param_size;
    padBytes = PADDING_8BYTE_ALIGN(payload_size);

    payloadInfo = (uint8_t*) calloc(1, (payload_size + padBytes));

    if (payloadInfo == NULL) {
        LOG(DEBUG) << __func__ << " payloadInfo null";
        status = -ENOMEM;
        goto cleanup;
    }

    createPayload(payloadInfo, &pal_payload, &effect_payload,
                                &customPayload, data->param_id, pal_param_size);

    memcpy(customPayload->data, data->data, pal_param_size);

    LOG(DEBUG) << __func__ << std::hex << " param Id: " << customPayload->paramId << " value: "
                              << customPayload->data[0] << " param_size: " << pal_param_size;

    if(oemStreamType == PAL_STREAM_COMPRESSED || oemStreamType==PAL_STREAM_PCM_OFFLOAD
            || oemStreamType==PAL_STREAM_PLAYBACK_BUS){
        LOG(DEBUG) << __func__ << " oemStreamType: " << oemStreamType;
        status = pal_gef_rw_param(PAL_PARAM_ID_UIEFFECT, (void *) pal_payload, payload_size,
                       aud_source_effect_device, oemStreamType, GEF_PARAM_WRITE, NULL);
    } else {
        LOG(ERROR) << __func__ << " Invalid stream";
        status = -ENOMEM;
        goto cleanup;
    }
    if (effect == ASYNC) {
        status = handleEffectASYNC(status, pal_payload, payload_size, aud_source_effect_device, customPayload);
    }

    if(status != 0){
        LOG(DEBUG) << __func__ << "pal_gef_rw_param failed";
        goto cleanup;
    }

cleanup:
    if (payloadInfo) {
        free(payloadInfo);
    }
    pal_payload = nullptr;
    effect_payload = nullptr;
    customPayload = nullptr;

    if (status != 0) {
        LOG(ERROR) << "Error setting param with error: " << status;
    } else {
        LOG(INFO) << __func__ << " Set parameter successfully";
    }

    LOG(DEBUG) << "Exit " << __func__;
}

// AWX get param for vendorExtension audio effect
int PalParamDelegator::AWX_get_param(pal_awx_param_t* param, effect_type effect) {
    LOG(DEBUG) << "Enter " << __func__;

    if (param == NULL){
        LOG(ERROR) << __func__ <<" Param is null ";
        return -ENOMEM;
    }

    int status = 0;
    size_t padBytes = 0;
    uint8_t* payloadInfo = NULL;
    pal_param_payload* pal_payload = nullptr;
    effect_pal_payload_t* effect_payload = nullptr;
    pal_effect_custom_payload_t* customPayload = nullptr;
    pal_awx_param_t* data = param;
    uint32_t pal_param_size = data->param_size;
    pal_device_id_t aud_source_effect_device = PAL_DEVICE_OUT_SPEAKER;

    uint32_t payload_size = sizeof(pal_param_payload) + sizeof(effect_pal_payload_t)
                            + sizeof(pal_effect_custom_payload_t) + pal_param_size;
    padBytes = PADDING_8BYTE_ALIGN(payload_size);

    payloadInfo = (uint8_t*) calloc(1, (payload_size + padBytes));

    if (payloadInfo == NULL) {
        LOG(DEBUG) << __func__ <<" payloadInfo null";
        status = -ENOMEM;
        goto cleanup;
    }

    createPayload(payloadInfo, &pal_payload, &effect_payload,
                                &customPayload, data->param_id, pal_param_size);

    LOG(DEBUG) << __func__ << " oemStreamType: " << oemStreamType;
    if(oemStreamType == PAL_STREAM_COMPRESSED || oemStreamType==PAL_STREAM_PCM_OFFLOAD
            || oemStreamType==PAL_STREAM_PLAYBACK_BUS){
        status = pal_gef_rw_param(PAL_PARAM_ID_UIEFFECT, (void*) pal_payload, payload_size,
                          aud_source_effect_device, oemStreamType, GEF_PARAM_READ, NULL);
    } else {
        LOG(ERROR) << __func__ << " Invalid stream";
        status = -ENOMEM;
        goto cleanup;
    }
    if(status != 0){
        LOG(DEBUG) << __func__ << "pal_gef_rw_param failed";
        goto cleanup;
    }

    LOG(DEBUG) << __func__ << " after param Id: " << std::hex << customPayload->paramId
                           << " value: " << customPayload->data[0] << " param_size: " << pal_param_size;

    memcpy(data->data, customPayload->data, pal_param_size);

cleanup:
    if (payloadInfo) {
        free(payloadInfo);
    }
    pal_payload = nullptr;
    effect_payload = nullptr;
    customPayload = nullptr;

    LOG(DEBUG) << "Exit " << __func__;
    return status;
}
void PalParamDelegator::AWX_set_param_handle(pal_stream_handle_t *handle, pal_awx_param_t* param, effect_type effect) {
    LOG(DEBUG) << "Enter " << __func__;

    if (param == NULL){
        LOG(ERROR) << __func__ <<" Param is null ";
        return;
    }

    int status = 0;
    size_t padBytes = 0;
    pal_param_payload* pal_payload = nullptr;
    effect_pal_payload_t* effect_payload = nullptr;
    pal_effect_custom_payload_t* customPayload = nullptr;
    pal_awx_param_t* data = param;
    uint8_t* payloadInfo = NULL;
    uint32_t pal_param_size = data->param_size;
    pal_device_id_t aud_source_effect_device = PAL_DEVICE_OUT_SPEAKER;

    uint32_t payload_size = sizeof(pal_param_payload) + sizeof(effect_pal_payload_t)
                            + sizeof(pal_effect_custom_payload_t) + pal_param_size;
    padBytes = PADDING_8BYTE_ALIGN(payload_size);

    payloadInfo = (uint8_t*) calloc(1, (payload_size + padBytes));

    if (payloadInfo == NULL) {
        LOG(DEBUG) << __func__ << " payloadInfo null";
        status = -ENOMEM;
        goto cleanup;
    }

    createPayload(payloadInfo, &pal_payload, &effect_payload,
                                &customPayload, data->param_id, pal_param_size);

    memcpy(customPayload->data, data->data, pal_param_size);

    LOG(DEBUG) << __func__ << std::hex << " param Id: " << customPayload->paramId << " value: "
                              << customPayload->data[0] << " param_size: " << pal_param_size;

    status = pal_stream_set_param(handle, PAL_PARAM_ID_UIEFFECT, pal_payload);

    if (effect == ASYNC) {
        status = handleEffectASYNC(status, pal_payload, payload_size, aud_source_effect_device, customPayload);
    }

    if(status != 0){
        LOG(DEBUG) << __func__ << "pal_stream_set_param failed";
        goto cleanup;
    }

cleanup:
    if (payloadInfo) {
        free(payloadInfo);
    }
    pal_payload = nullptr;
    effect_payload = nullptr;
    customPayload = nullptr;

    if (status != 0) {
        LOG(ERROR) << "Error setting param with error: " << status;
    } else {
        LOG(INFO) << __func__ << " Set parameter successfully";
    }

    LOG(DEBUG) << "Exit " << __func__;
}
// AWX get param for vendorExtension audio effect
int PalParamDelegator::AWX_get_param_handle(pal_stream_handle_t *handle,pal_awx_param_t* param, effect_type effect) {
    LOG(DEBUG) << "Enter " << __func__;

    if (param == NULL){
        LOG(ERROR) << __func__ <<" Param is null ";
        return -ENOMEM;
    }

    int status = 0;
    size_t padBytes = 0;
    uint8_t* payloadInfo = NULL;
    pal_param_payload* pal_payload = nullptr;
    effect_pal_payload_t* effect_payload = nullptr;
    pal_effect_custom_payload_t* customPayload = nullptr;
    pal_awx_param_t* data = param;
    uint32_t pal_param_size = data->param_size;
    pal_device_id_t aud_source_effect_device = PAL_DEVICE_OUT_SPEAKER;

    uint32_t payload_size = sizeof(pal_param_payload) + sizeof(effect_pal_payload_t)
                            + sizeof(pal_effect_custom_payload_t) + pal_param_size;
    padBytes = PADDING_8BYTE_ALIGN(payload_size);

    payloadInfo = (uint8_t*) calloc(1, (payload_size + padBytes));

    if (payloadInfo == NULL) {
        LOG(DEBUG) << __func__ <<" payloadInfo null";
        status = -ENOMEM;
        goto cleanup;
    }

    if (handle == NULL)
    {
        LOG(DEBUG) << __func__ <<" PAL HANDLE is NULL";
        status = -ENOMEM;
        goto cleanup;
    }

    createPayload(payloadInfo, &pal_payload, &effect_payload,
                                &customPayload, data->param_id, pal_param_size);

    status = pal_stream_get_param(handle, PAL_PARAM_ID_UIEFFECT, &pal_payload);

    if(status != 0){
        LOG(DEBUG) << __func__ << "pal_gef_rw_param failed";
        goto cleanup;
    }

    LOG(DEBUG) << __func__ << " after param Id: " << std::hex << customPayload->paramId
                           << " value: " << customPayload->data[0] << " param_size: " << pal_param_size;

    memcpy(data->data, customPayload->data, pal_param_size);

cleanup:
    if (payloadInfo) {
        free(payloadInfo);
    }
    pal_payload = nullptr;
    effect_payload = nullptr;
    customPayload = nullptr;

    LOG(DEBUG) << "Exit " << __func__;
    return status;
}

void PalParamDelegator::createPayload(uint8_t* payloadInfo,  pal_param_payload** pal_payload,
                       effect_pal_payload_t** effect_payload, pal_effect_custom_payload_t** customPayload,
                       uint32_t param_id, uint32_t pal_param_size) {
    LOG(DEBUG) << "Enter " << __func__;

    if (!payloadInfo) {
        LOG(ERROR) << "Bad Parameter";
        return;
    }

    *pal_payload = (pal_param_payload*) payloadInfo;
    *effect_payload = (effect_pal_payload_t*) (payloadInfo + sizeof(pal_param_payload));
    *customPayload = (pal_effect_custom_payload_t*) (payloadInfo + sizeof(pal_param_payload)
                    + sizeof(effect_pal_payload_t));

    (*pal_payload)->payload_size = sizeof(effect_pal_payload_t) +
                                sizeof(pal_effect_custom_payload_t) + pal_param_size;

    (*effect_payload)->isTKV = PARAM_NONTKV;
    (*effect_payload)->tag = 0xC0000057;
    (*effect_payload)->payloadSize = sizeof(pal_effect_custom_payload_t) + pal_param_size;

    (*customPayload)->paramId = param_id;

    LOG(DEBUG) << "Exit " << __func__;
    return;
}

int PalParamDelegator::handleEffectASYNC(int status, pal_param_payload* pal_payload, uint32_t payload_size,
                    pal_device_id_t aud_source_effect_device, pal_effect_custom_payload_t* customPayload) {
    LOG(DEBUG) << "Enter " << __func__;
    LOG(DEBUG) << __func__ << " oemStreamType: " << oemStreamType;

    if (status == -1) {
        int read_status = 0;
        if (oemStreamType == PAL_STREAM_COMPRESSED || oemStreamType==PAL_STREAM_PCM_OFFLOAD
            || oemStreamType==PAL_STREAM_PLAYBACK_BUS)
            {
                read_status = pal_gef_rw_param(PAL_PARAM_ID_UIEFFECT, (void*) pal_payload, payload_size,
                aud_source_effect_device, oemStreamType, GEF_PARAM_READ, NULL);
                int num = customPayload->data[0];
                num &= 0x0000FFFF;
                //Checking AsyncTransactionStatus(Byte 2 and 3) of Harman modules in DSP : 0 for OK, 2 for BUSY
                if (num == ASYNC_STATUS_OK || num == ASYNC_STATUS_BUSY) {
                    status = 0;
                }
            } else {
                LOG(ERROR) << __func__ << " Invalid stream";
                status = -ENOMEM;
            }
    }
    LOG(DEBUG) << "Exit " << __func__;
    return status;
}
}
