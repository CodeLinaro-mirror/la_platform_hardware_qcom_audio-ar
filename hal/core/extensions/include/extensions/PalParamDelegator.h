/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

 #pragma once

#include <android-base/logging.h>
#include <android-base/thread_annotations.h>
#include <array>
#include <cstddef>
#include <errno.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "PalApi.h"
#include "PalApi.h"
#include <stdint.h>

#define PADDING_8BYTE_ALIGN(x)  ((((x) + 7) & 7) ^ 7)

#define ASYNC_STATUS_BUSY 2
#define ASYNC_STATUS_OK 0

#define MAX_VOLUME_VALUE 0
#define MIN_VOLUME_VALUE -9000
#define PARAM_ID_VOLUME  0x11112501
#define SET 0x7F

extern pal_stream_type_t oemStreamType;

namespace aidl::qti::awx   {


struct VolumeParams {
    uint16_t eq_mask;
    uint16_t status;
    int32_t value[16];
};

typedef struct {
    uint32_t param_id;
    uint32_t param_size;
    void* data;
} pal_awx_param_t;

typedef enum {
    SYNC_WITH_AUDIO_BUS,
    SYNC_WITHOUT_AUDIO_BUS,
    ASYNC,
    OTHER
} effect_type;

struct PalParamDelegator {
    public:
        /**
        * @brief AWX_set_param sends the pal parameters to PAL for AWX module
        * @param params Parameter data
        * @param effect effect type
        */
        static void AWX_set_param(pal_awx_param_t *parms, effect_type effect);
        /**
        * @brief AWX_set_param_handle sends the pal parameters to PAL for AWX module
        * @param handle Pal handle
        * @param params Parameter data
        * @param effect effect type
        */
        static void AWX_set_param_handle(pal_stream_handle_t *handle,pal_awx_param_t *parms, effect_type effect);
        /**
        * @brief AWX_get_param sends the pal parameters to PAL for AWX module
        * @param params Parameter data
        * @param effect effect type
        */
        static int AWX_get_param(pal_awx_param_t *parms, effect_type effect);
        /**
        * @brief AWX_get_param_handle gets  the pal parameters to PAL for AWX module
        * @param handle Pal handle
        * @param params Parameter data
        * @param effect effect type
        */
        static int AWX_get_param_handle(pal_stream_handle_t *handle,pal_awx_param_t *parms, effect_type effect);
        /**
        * @brief handleEffectASYNC handles the Async effects
        * @param status status
        * @param pal_payload pay load buffer
        * @param pal_size pay load size
        * @param aud_source_effect_device device id
        *  @param customPayload custome payload
        */
        static int handleEffectASYNC(int status, pal_param_payload* pal_payload, uint32_t payload_size,
                    pal_device_id_t aud_source_effect_device, pal_effect_custom_payload_t* customPayload);
        /**
        * @brief createPayload creats the payload
        * @param status status
        * @param pal_payload pay load buffer
        * @param effect_payload effect payload
        * @param customPayload  custom payload
        * @param param_id param id
        * @param pal_param_size pal param size
        */
        static void createPayload(uint8_t* payloadInfo, pal_param_payload** pal_payload,
                        effect_pal_payload_t** effect_payload, pal_effect_custom_payload_t** customPayload,
                        uint32_t param_id, uint32_t pal_param_size);
};

}// aidl::ampere::effects