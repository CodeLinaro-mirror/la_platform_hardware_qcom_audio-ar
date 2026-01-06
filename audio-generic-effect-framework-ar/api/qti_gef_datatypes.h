/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.  
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef _QTI_GEF_AR_DATA_TYPES_H_
#define _QTI_GEF_AR_DATA_TYPES_H_

#include <system/audio.h>
#include "PalApi.h"

#include <aidl/android/media/audio/common/AudioDevice.h>
#include <aidl/android/media/audio/common/AudioDeviceType.h>
#include <aidl/android/media/audio/common/AudioOutputFlags.h>
#include <aidl/android/media/audio/common/AudioUuid.h>

using ::aidl::android::media::audio::common::AudioDevice;
using ::aidl::android::media::audio::common::AudioDeviceType;
using ::aidl::android::media::audio::common::AudioOutputFlags;
using ::aidl::android::media::audio::common::AudioUuid;

typedef void gef_handle_t;

/*
 * This declares various event types that
 * can be sent through the callback
 *
 * EFFECT_ENABLED     indicates that another effect got enabled
 * DEVICE_CONNECTED   device field of eventValue indicates which device
 *                    and the channel map is indicated in channel_map field
 *                    in eventValue
 */
typedef enum { EFFECT_ENABLED, DEVICE_CONNECTED } event_type;

/*
 * This declares the values corresponding to the event types
 * that can be sent through the callback
 *
 * value            Indicates whether it's effect on or off.
 *                  This field is relevant only if EFFECT_ENABLED is set
 * effect_uuid      The UUID of the effect that got enabled.
 *                  This field is relevant only if EFFECT_ENABLED is set
 * device           Indicates the device connected.
 *                  This field is relevant only when DEVICE_CONNECTED is set
 * channel_mask     Indicates channel mask. This field is relevant only
 *                  when DEVICE_CONNECTED is set.
 * stream_type      Indicates the app type. This field is relevant only
 *                  when DEVICE_CONNECTED is set.
 */
typedef struct {
    int value;                         // indicates error
    AudioUuid effect_uuid;             // will be relevant only for EFFECT_ENABLED
    std::vector<AudioDevice> devices;  // indicates the device connected
    audio_channel_mask_t channel_mask; // indicates the channel map
    int sample_rate;                   // indicates the sample rate
    int stream_type;                   // corresponding stream type
} event_value;

/*
 * This declares the prototype for the configuration parameters of the effect
 *
 * streamType  StreamType to which the param needs to be applied
 * deviceId    Device to which the param needs to be applied
 * samppleRate For sample rate relevant calibration data, please set
 *             non-zero sample rate. A CKV pair will be generated for
 *             sample rate.
 *             If the param is regardless of sample rate, please set it
 *             to 0.
 * persist     This is deprecated now. For value of 1, message of using
 *             gef_store_ar_cacheparam is printout. please use
 *             gef_store_ar_cacheparam or gef_retrieve_ar_cacheparam.
 */
typedef struct {
    AudioOutputFlags outputFlag;
    std::vector<AudioDevice> device_ids;
    int sample_rate;
    bool persist;
} effect_config_params;

/*
 * This declares the prototype for the structure for param set to ACDB.
 *
 * data      pointer to the buffer of size length that contains the parameters.
 *           data is in the structure of acdb_effect_param.
 * length    length of the pre-allocated buffer by GEF client. length equals
 *           to sizeof(struct acdb_effect_param) + blob_size. blob_size
 *           equals to num_kvs * 8 bytes + payload size.
 *           Example: isTKV = 1, tag = 0xc0000014, num_kvs = 1, blob_size = 16,
 *                    blob data
 *                    0xB2000000 <-- key of first TKV
 *                    1          <-- value of first TKV
 *                    0x8001026  <-- param id
 *                    1          <-- param value
 */
typedef struct {
    void* const data;
    const int length;
} effect_data_in;

/*
 * This declares the prototype for the structure for param get from ACDB.
 *
 * data      pointer to the buffer of size length that contains the parameters
 * length    length of the pre-allocated buffer by GEF client. length equals
 *           to sizeof(struct acdb_effect_param) + blob_size. blob_size
 *           equals to num_kvs * 8 bytes + payload size.
 *           Example: isTKV = 1, tag = 0xc0000014, num_kvs = 1, blob_size = 16,
 *                    blob data
 *                    0xB2000000 <-- key of first TKV
 *                    1          <-- value of first TKV
 *                    0x8001026  <-- param id
 *                    0          <-- return data starts here.
 */
typedef struct {
    void* data;
    int length;
} effect_data_out;

/*
 * This declares the prototype for the actual data structure for Audio Reach.
 *
 * isTKV       the type of data. true->TKV, false->CKV.
 * tag         tag of the module
 * num_kvs     num of TKVs or CKVs. for sample rate CKV, there is no need to
 *             specify in CKV. Just set non-zero value for sample rate.
 * blob_size   length of the param id + param value
 * blob[]      param id + param value
 */
struct acdb_effect_param {
    bool isTKV;
    uint32_t tag;
    uint32_t num_kvs;   /**< number of ckv or tkv*/
    uint32_t blob_size; /**< kv size + payload size*/
    uint8_t blob[];     /**< kv + payload */
};

/*
 * This declares the prototype for function pointer that effect
 * implementation registers with GEF
 */
typedef void (*gef_func_ptr)(void*, event_type, event_value);

#endif /* _QTI_GEF_AR_DATA_TYPES_H_ */
