/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.  
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "generic_effect_ar"
#define LOG_NDDEBUG 0
/*#define LOG_NDEBUG 0*/

#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <qti_gef_api.h>
#include <stdlib.h>

#include <cutils/list.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <cutils/str_parms.h>
#include <cutils/trace.h>

#include <qti-audio/PlatformConverter.h>
#include "PalApi.h"
#include "PalDefs.h"
#include "PalARDefs.h"

static uint64_t TRACE_TAG = ATRACE_TAG_NEVER;

typedef struct effect_private_data {
    struct listnode list;
    AudioUuid uuid;
    bool enable;
    gef_func_ptr cb;
    void* data;             // data given by effect
    void* gef_private_data; // pointer to gef handle
} effect_private_data;

typedef struct gef_private_data {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int num_effects;

    // list of all effects
    struct listnode effect_list;
    int debug_enable;
    bool debug_func_entry;
    bool is_gef_initialized;
} gef_private_data;

static gef_private_data gef_global_handle;

static pthread_once_t gef_lib_once = PTHREAD_ONCE_INIT;

static void check_and_enable_debug_logs(struct gef_private_data* my_data) {
    if (!my_data) return;

    char val[PROPERTY_VALUE_MAX] = {0};
    if (property_get("vendor.audio.gef.debug.flags", val, "") && !strncmp(val, "1", 1)) {
        my_data->debug_enable = 1;
        my_data->debug_func_entry = true;
    }
}

static void check_and_enable_traces() {
    if (property_get_bool("vendor.audio.gef.enable.traces", false)) {
        TRACE_TAG = ATRACE_TAG_AUDIO;
    }
}

extern "C" {
static void gef_lib_init() {
    gef_private_data* my_data = &gef_global_handle;
    list_init(&my_data->effect_list);
    // initialize debug and tracing
    check_and_enable_debug_logs(my_data);
    check_and_enable_traces();

    pthread_mutex_init(&my_data->mutex, (const pthread_mutexattr_t*)NULL);
    pthread_cond_init(&my_data->cond, (const pthread_condattr_t*)NULL);

    my_data->num_effects = 0;
}

static void gef_lib_init_once() {
    pthread_once(&gef_lib_once, gef_lib_init);
}

int gefGetPalInfo(const std::vector<AudioDevice>& setDevices, pal_device_id_t* pal_device_ids,
                  AudioOutputFlags outPutFlags, pal_stream_type_t* pal_stream_type) {
    *pal_stream_type =
            qti::audio::PlatformConverter::getPalStreamTypeId(static_cast<int32_t>(outPutFlags));
    int idx = 0;
    for (auto dev : setDevices) {
        pal_device_ids[idx] = qti::audio::PlatformConverter::getPalDeviceId(dev.type);
        idx++;
    }
    return idx;
}

__attribute__((visibility("default"))) gef_handle_t* gef_register_session(AudioUuid effectId) {
    gef_private_data* my_data = &gef_global_handle;
    effect_private_data* effect_data = (effect_private_data*)NULL;

    ALOGD_IF(gef_global_handle.debug_func_entry, "%s: enter with (%d, %d, %d)", __func__,
             effectId.timeLow, effectId.timeMid, effectId.timeHiAndVersion);

    gef_lib_init_once();
    pthread_mutex_lock(&my_data->mutex);

    // store effect data
    // have to create dynamic memory and store in a list
    effect_data = (effect_private_data*)calloc(1, sizeof(effect_private_data));
    if (!effect_data) {
        goto ERROR_RETURN;
    }

    effect_data->uuid = effectId;
    effect_data->enable = false;
    effect_data->cb = (gef_func_ptr)NULL;
    effect_data->data = (void*)NULL;
    effect_data->gef_private_data = (void*)my_data;

    my_data->num_effects++;

    ALOGD_IF(gef_global_handle.debug_func_entry, "number of registered effects %d",
             my_data->num_effects);

    // add this to the list of nodes
    list_add_tail(&gef_global_handle.effect_list, &effect_data->list);

ERROR_RETURN:
    pthread_cond_signal(&my_data->cond);
    pthread_mutex_unlock(&my_data->mutex);

    ALOGD_IF(gef_global_handle.debug_func_entry, "%s: Exit with effect_handle %p", __func__,
             effect_data);
    return (gef_handle_t*)effect_data;
}

__attribute__((visibility("default"))) int gef_deregister_session(gef_handle_t* handle)

{
    int ret = 0;
    effect_private_data* effect_data = (effect_private_data*)handle;
    gef_private_data* gp_data = (gef_private_data*)NULL;

    ALOGD_IF(gef_global_handle.debug_func_entry, "%s: Enter", __func__);

    if (!effect_data || !effect_data->gef_private_data) {
        ret = -EINVAL;
        goto ERROR_RETURN;
    }

    gp_data = (gef_private_data*)effect_data->gef_private_data;
    if (!gp_data || gp_data != &gef_global_handle) {
        ret = -EINVAL;
        goto ERROR_RETURN;
    }

    pthread_mutex_lock(&gp_data->mutex);

    // remove the node from the list
    gp_data->num_effects--;

    ALOGD_IF(gef_global_handle.debug_func_entry, "number of registered effects %d",
             gp_data->num_effects);
    list_remove(&effect_data->list);
    free(effect_data);

    pthread_cond_signal(&gp_data->cond);
    pthread_mutex_unlock(&gp_data->mutex);

ERROR_RETURN:
    ALOGD_IF(gef_global_handle.debug_func_entry, "%s: Exit with error %d", __func__, ret);
    return ret;
}

__attribute__((visibility("default"))) int gef_enable_effect(gef_handle_t* handle) {
    effect_private_data* effect_data = (effect_private_data*)handle;
    effect_private_data* fx_ctxt;
    event_type type = EFFECT_ENABLED;
    event_value value;
    struct listnode* node;
    int ret = 0;

    ALOGD_IF(gef_global_handle.debug_func_entry, "%s: Enter", __func__);

    if (!effect_data || !effect_data->gef_private_data ||
        effect_data->gef_private_data != &gef_global_handle) {
        ret = -EINVAL;
        goto ERROR_RETURN;
    }

    effect_data->enable = true;

    // notify enablement event to effect clients
    value.value = effect_data->enable;
    value.effect_uuid = effect_data->uuid;

    list_for_each(node, &gef_global_handle.effect_list) {
        fx_ctxt = node_to_item(node, effect_private_data, list);
        if ((fx_ctxt != effect_data) && fx_ctxt->cb) {
            fx_ctxt->cb(fx_ctxt, type, value);
        }
    }

ERROR_RETURN:
    ALOGD_IF(gef_global_handle.debug_func_entry, "%s: Exit with error %d", __func__, ret);
    return ret;
}

__attribute__((visibility("default"))) int gef_disable_effect(gef_handle_t* handle) {
    effect_private_data* effect_data = (effect_private_data*)handle;
    effect_private_data* fx_ctxt;
    event_type type = EFFECT_ENABLED;
    event_value value;
    struct listnode* node;
    int ret = 0;

    ALOGD_IF(gef_global_handle.debug_func_entry, "%s: Enter", __func__);

    if (!effect_data || !effect_data->gef_private_data ||
        effect_data->gef_private_data != &gef_global_handle) {
        ret = -EINVAL;
        goto ERROR_RETURN;
    }

    effect_data->enable = false;

    // notify enablement event to effect clients
    value.value = effect_data->enable;
    value.effect_uuid = effect_data->uuid;

    list_for_each(node, &gef_global_handle.effect_list) {
        fx_ctxt = node_to_item(node, effect_private_data, list);
        if ((fx_ctxt != effect_data) && fx_ctxt->cb) {
            fx_ctxt->cb(fx_ctxt, type, value);
        }
    }

ERROR_RETURN:
    ALOGD_IF(gef_global_handle.debug_func_entry, "%s: Exit with error %d", __func__, ret);
    return ret;
}

__attribute__((visibility("default"))) int gef_query_version(int* majorVersion, int* minorVersion) {
    int ret = 0;

    ALOGD_IF(gef_global_handle.debug_func_entry, "%s: Enter", __func__);

    if (!majorVersion || !minorVersion) {
        ret = -EINVAL;
        goto ERROR_RETURN;
    }
    *majorVersion = 1;
    *minorVersion = 1;
ERROR_RETURN:

    ALOGD_IF(gef_global_handle.debug_func_entry, "%s: Exit with error %d", __func__, ret);
    return ret;
}

/*
 * This function is to be called by vendor abstraction layer to GEF to
 * register a callback. This callback will be invoked by GEF
 * under these circumstances
 *
 * 1. Error when sending a calibration
 * 2. Another effect which was also registered through GEF got enabled
 * 3. A device is connected to notify which device is connected
 *   and the channel map associated
 *
 * handle: - Handle to GEF
 * cb:- pointer to callback
 * data:- data that will be sent in the callback
 *
 * returns:- 0: Operation is successful
 *      EINVAL: When the session is invalid/handle is null etc
 */
__attribute__((visibility("default"))) int gef_register_callback(gef_handle_t* handle,
                                                                 gef_func_ptr cb, void* data) {
    int ret = 0;
    effect_private_data* effect_data = (effect_private_data*)handle;
    gef_private_data* gp_data;
    ALOGD_IF(gef_global_handle.debug_func_entry, "%s: Enter", __func__);

    if (!effect_data || !effect_data->gef_private_data) {
        ret = -EINVAL;
        goto ERROR_RETURN;
    }

    gp_data = (gef_private_data*)effect_data->gef_private_data;

    if (!gp_data || gp_data != &gef_global_handle) {
        ret = -EINVAL;
        goto ERROR_RETURN;
    }

    pthread_mutex_lock(&gp_data->mutex);

    effect_data->cb = cb;
    effect_data->data = data;

    pthread_cond_signal(&gp_data->cond);
    pthread_mutex_unlock(&gp_data->mutex);

ERROR_RETURN:
    ALOGD_IF(gef_global_handle.debug_func_entry, "%s: Exit with error %d", __func__, ret);
    return ret;
}

/*
 * This method sends the parameters that will be sent to the DSP
 * The parameters are automatically stored in ACDB cache.
 *
 * handle: - Handle to GEF
 * data:- data corresponding to the effect
 *
 * returns:- 0: Operation is successful
 *       EINVAL: When the session is invalid/handle is null etc
 *       ENODEV: When GEF has not been initialized
 *       ENOSYS: Sending parameters is not supported
 */
int gef_set_ar_param(gef_handle_t* handle, effect_config_params* params, effect_data_in* data) {
    int ret = 0;
    effect_private_data* effect_data = (effect_private_data*)handle;
    gef_private_data* gp_data = (gef_private_data*)NULL;
    int pal_device_count = 0;
    int device_count = 0;
    int i = 0;
    pal_device_id_t* pal_device_ids = NULL;
    pal_stream_type_t pal_stream_type;

    if (!params) return -EINVAL;

    if (params->device_ids.empty()) {
        ALOGI("%s: This is stream param, as device id is AUDIO_DEVICE_NONE", __func__);
        device_count = 1;
    } else
        device_count = params->device_ids.size();

    if (device_count) {
        pal_device_ids = (pal_device_id_t*)calloc(device_count, sizeof(pal_device_id_t));
        if (!pal_device_ids) return -ENOMEM;
    } else {
        return -EINVAL;
    }
    ALOGD_IF(gef_global_handle.debug_func_entry, "%s: Enter", __func__);

    if ((!effect_data) || (!data) || (!(data->data))) {
        ret = -EINVAL;
        goto ERROR_RETURN;
    }

    gp_data = (gef_private_data*)effect_data->gef_private_data;
    if (!gp_data || gp_data != &gef_global_handle) {
        ret = -EINVAL;
        goto ERROR_RETURN;
    }

    ret = -ENOSYS;
    pal_device_count =
            gefGetPalInfo(params->device_ids, pal_device_ids, params->outputFlag, &pal_stream_type);
    /*
     * handle stream-only or device-only param
     */
    if (pal_device_count == 0 && params->device_ids.empty()) pal_device_count = 1;

    if (pal_device_count <= 0) {
        ALOGE("%s: failed to get pal information from HAL.\n", __func__);
        goto ERROR_RETURN;
    }

    for (i = 0; i < pal_device_count; i++) {
        custom_payload_uc_info_t info;
        info.pal_stream_type = pal_stream_type;
        info.pal_device_id = pal_device_ids[i];
        ret = pal_set_custom_param(&info,
                            PAL_CUSTOM_PARAM_AR_UI_EFFECT,
                            data->data,
                            data->length);
    }

    if (params->persist) {
        ALOGE("%s: please use gef_store_ar_cacheparam.\n", __func__);
    }

ERROR_RETURN:
    free(pal_device_ids);

    ALOGD_IF(gef_global_handle.debug_func_entry, "%s: Exit with error %d", __func__, ret);
    return ret;
}

/*
 * This api will retrieve the entire data binary from the DSP
 * for the module and parameter ids mentioned.
 *
 * Vendor abstraction layer is expected to allocate memory for
 * the buffer into which the data corresponding to the parameters
 * will be stored
 * only only one device is allowed!
 * handle: - Handle to GEF
 * data:- data corresponding to the effect
 *
 * returns:- ENOSYS: Retrieving parameters is not supported
 */
int gef_get_ar_param(const gef_handle_t* handle, effect_config_params* params,
                     effect_data_out* data) {
    int ret = 0;
    effect_private_data* effect_data = (effect_private_data*)handle;
    gef_private_data* gp_data = (gef_private_data*)NULL;
    int pal_device_count = 0;
    int device_count = 0;
    pal_device_id_t pal_device_id;
    pal_stream_type_t pal_stream_type;
    custom_payload_uc_info_t info;
    size_t payload_size;


    if (!params) return -EINVAL;

    device_count = params->device_ids.size();
    if (device_count != 1) {
        ALOGE("%s: only one device is supported for param read.", __func__);
        ret = -EINVAL;
        goto ERROR_RETURN;
    }

    ALOGD("%s: device id=%s stream type = %s\n", __func__, params->device_ids[0].toString().c_str(),
          ::aidl::android::media::audio::common::toString(params->outputFlag).c_str());

    ALOGD_IF(gef_global_handle.debug_func_entry, "%s: Enter", __func__);

    if ((!effect_data) || (!data) || (!(data->data))) {
        ret = -EINVAL;
        goto ERROR_RETURN;
    }

    gp_data = (gef_private_data*)effect_data->gef_private_data;
    if (!gp_data || gp_data != &gef_global_handle) {
        ret = -EINVAL;
        goto ERROR_RETURN;
    }

    pal_device_count =
            gefGetPalInfo(params->device_ids, &pal_device_id, params->outputFlag, &pal_stream_type);

    if (pal_device_count != device_count) {
        ALOGE("%s: failed to get pal information from HAL.\n", __func__);
        goto ERROR_RETURN;
    }

    payload_size = (size_t)data->length;
    info.pal_stream_type = pal_stream_type;
    info.pal_device_id = pal_device_id;

    ret = pal_get_custom_param(&info,
                            PAL_CUSTOM_PARAM_AR_UI_EFFECT,
                            data->data,
                            &payload_size);

ERROR_RETURN:
    ALOGD_IF(gef_global_handle.debug_func_entry, "%s: Exit with error %d", __func__, ret);
    return ret;
}

/*
 * This method sends the parameters that are to be stored in ACDB.
 *
 * handle: - Handle to GEF
 * params:- Parameters of the effect
 * data:- data corresponding to the effect
 *
 * returns:- 0: Operation is successful
 *      EINVAL: When the session is invalid/handle is null etc
 *      ENODEV: When GEF has not been initialized
 *      ENOSYS: Sending parameters is not supported
 */
int gef_store_ar_cacheparam(gef_handle_t* handle, effect_config_params* params,
                            effect_data_in* data) {
    int ret = 0;
    effect_private_data* effect_data = (effect_private_data*)handle;
    gef_private_data* gp_data = (gef_private_data*)NULL;
    int pal_device_count = 0;
    int device_count = 0;
    int i = 0;
    pal_device_id_t* pal_device_ids = NULL;
    pal_stream_type_t pal_stream_type;
    custom_payload_uc_info_t info;

    if (!params) return -EINVAL;

    if (params->device_ids.empty()) {
        ALOGI("%s: This is stream param, as device id is AUDIO_DEVICE_NONE", __func__);
        device_count = 1;
    } else
        device_count = params->device_ids.size();

    if (device_count) {
        pal_device_ids = (pal_device_id_t*)calloc(device_count, sizeof(pal_device_id_t));
        if (!pal_device_ids) return -ENOMEM;
    } else {
        return -EINVAL;
    }

    ALOGD_IF(gef_global_handle.debug_func_entry, "%s: Enter", __func__);

    if ((!effect_data) || (!data) || (!(data->data))) {
        ret = -EINVAL;
        goto ERROR_RETURN;
    }

    gp_data = (gef_private_data*)effect_data->gef_private_data;
    if (!gp_data || gp_data != &gef_global_handle) {
        ret = -EINVAL;
        goto ERROR_RETURN;
    }

    ret = -ENOSYS;
    pal_device_count =
            gefGetPalInfo(params->device_ids, pal_device_ids, params->outputFlag, &pal_stream_type);

    /*
     * handle stream-only or device-only param
     */
    if (pal_device_count == 0 && params->device_ids.empty()) pal_device_count = 1;

    if (pal_device_count <= 0) {
        ALOGE("%s: failed to get pal information from HAL.\n", __func__);
        goto ERROR_RETURN;
    }

    for (i = 0; i < pal_device_count; i++) {
        info.pal_stream_type = pal_stream_type;
        info.pal_device_id = pal_device_ids[i];
        info.sample_rate = params->sample_rate;
        info.instance_id = 1;
        info.streamless = true;
        ret = pal_set_custom_param(&info,
                            PAL_CUSTOM_PARAM_AR_UI_EFFECT,
                            data->data,
                            data->length);

    }

ERROR_RETURN:
    free(pal_device_ids);
    ALOGD_IF(gef_global_handle.debug_func_entry, "%s: Exit with error %d", __func__, ret);
    return ret;
}

/*
 * This method retrieves the parameters that are stored in cache.
 * Vendor abstraction layer is expected to allocate memory for
 * the buffer into which the data corresponding to the parameters
 * will be stored
 *
 * THIS API IS NOT SUPPORTED FOR NOW
 * API will always return ENOSYS
 *
 * handle: - Handle to GEF
 * data:- data corresponding to the effect
 *
 * returns:- ENOSYS: Retrieving parameters is not supported
 */
int gef_retrieve_ar_cacheparam(const gef_handle_t* handle, effect_config_params* params,
                               effect_data_out* data) {
    int ret = 0;
    effect_private_data* effect_data = (effect_private_data*)handle;
    gef_private_data* gp_data = (gef_private_data*)NULL;
    int pal_device_count = 0;
    int device_count = 0;
    pal_device_id_t pal_device_id;
    pal_stream_type_t pal_stream_type;
    custom_payload_uc_info_t info;
    size_t payload_size = 0;


    if (!params) return -EINVAL;

    device_count = params->device_ids.size();
    if (device_count != 1) {
        ALOGE("%s: only one device is supported for param read.", __func__);
        ret = -EINVAL;
        goto ERROR_RETURN;
    }

    ALOGD("%s: device id=%s stream type = %s\n", __func__, params->device_ids[0].toString().c_str(),
          ::aidl::android::media::audio::common::toString(params->outputFlag).c_str());

    ALOGD_IF(gef_global_handle.debug_func_entry, "%s: Enter", __func__);

    if ((!effect_data) || (!data) || (!(data->data))) {
        ret = -EINVAL;
        goto ERROR_RETURN;
    }

    gp_data = (gef_private_data*)effect_data->gef_private_data;
    if (!gp_data || gp_data != &gef_global_handle) {
        ret = -EINVAL;
        goto ERROR_RETURN;
    }

    pal_device_count =
            gefGetPalInfo(params->device_ids, &pal_device_id, params->outputFlag, &pal_stream_type);

    if (pal_device_count != device_count) {
        ALOGE("%s: failed to get pal information from HAL.\n", __func__);
        goto ERROR_RETURN;
    }

    info.pal_stream_type = pal_stream_type;
    info.pal_device_id = pal_device_id;
    info.sample_rate = params->sample_rate;
    info.instance_id = 1;
    info.streamless = true;
    payload_size = (size_t)data->length;
    ret = pal_get_custom_param(&info,
                        PAL_CUSTOM_PARAM_AR_UI_EFFECT,
                        data->data,
                        &payload_size);

ERROR_RETURN:
    ALOGD_IF(gef_global_handle.debug_func_entry, "%s: Exit with error %d", __func__, ret);
    return ret;
}

void gef_init() {
    gef_lib_init_once();
}

void gef_deinit() {
    return;
}


void gef_interface_init() {
    gef_init();
}
void gef_interface_deinit() {
    gef_deinit();
}
}
