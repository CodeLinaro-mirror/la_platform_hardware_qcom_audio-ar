/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
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
 */

/*
 * ​​​​​Changes from Qualcomm Innovation Center are provided under the following license:
 *
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 */

#ifndef AUDIOEXTN_H
#define AUDIOEXTN_H
#include <cutils/str_parms.h>
#include <set>
#include "PalDefs.h"
#include "audio_defs_ar.h"
#include <log/log.h>
#include "battery_listener.h"

typedef void (*batt_listener_init_t)(battery_status_change_fn_t);
typedef void (*batt_listener_deinit_t)();
typedef bool (*batt_prop_is_charging_t)();
typedef bool (*audio_device_cmp_fn_t)(audio_devices_t);

class AudioDevice;
//HFP
typedef int audio_usecase_t;
typedef void(*hfp_init_t)();
typedef bool(*hfp_is_active_t)(std::shared_ptr<AudioDevice> adev);
typedef audio_usecase_t(*hfp_get_usecase_t)();
typedef int(*hfp_set_mic_mute_t)(bool state);
typedef int(*hfp_set_mic_mute2_t)(std::shared_ptr<AudioDevice> adev, bool state);
//HFP AG
typedef void(*hfp_ag_init_t)();
typedef bool(*hfp_ag_is_active_t)(std::shared_ptr<AudioDevice> adev);
typedef audio_usecase_t(*hfp_ag_get_usecase_t)();
typedef int(*hfp_ag_set_mic_mute_t)(bool state);
//AUTO HAL
typedef void(*autohal_init_t)();
typedef pal_stream_type_t (*autohal_GetCarAudioPalStreamType_t)(char * address);
typedef void (*place_marker_t)(char const *name, bool isEnter);

typedef int (*set_parameters_t) (std::shared_ptr<AudioDevice>, struct str_parms*);
typedef void (*get_parameters_t) (std::shared_ptr<AudioDevice>, struct str_parms*, struct str_parms*);

// POWER_POLICY FEATURE
typedef void (*fp_in_set_power_policy_t) (uint8_t);
typedef void (*fp_out_set_power_policy_t) (uint8_t);

typedef struct power_policy_init_config {
    fp_in_set_power_policy_t     fp_in_set_power_policy;
    fp_out_set_power_policy_t    fp_out_set_power_policy;
} power_policy_init_config_t;

class AudioExtn
{
private:
    static int GetProxyParameters(std::shared_ptr<AudioDevice> adev,
            struct str_parms *query, struct str_parms *reply);

public:
    static int audio_extn_parse_compress_metadata(struct audio_config *config_, pal_snd_dec_t *pal_snd_dec, str_parms *parms, uint32_t *sr, uint16_t *ch);
    static void audio_extn_get_parameters(std::shared_ptr<AudioDevice> adev, struct str_parms *query, struct str_parms *reply);
    static int audio_extn_set_parameters(std::shared_ptr<AudioDevice> adev, struct str_parms *params);
    static int get_controller_stream_from_params(struct str_parms *parms, int *controller, int *stream);

    static void battery_listener_feature_init(bool is_feature_enabled);
    static void battery_properties_listener_init(battery_status_change_fn_t fn);
    static void battery_properties_listener_deinit();
    static bool battery_properties_is_charging();

    static int audio_extn_hidl_init();

    //HFP
    static int hfp_feature_init(bool is_feature_enabled);
    static bool audio_extn_hfp_is_active(std::shared_ptr<AudioDevice> adev);
    audio_usecase_t audio_extn_hfp_get_usecase();
    static int audio_extn_hfp_set_mic_mute(bool state);
    static int audio_extn_hfp_set_parameters(std::shared_ptr<AudioDevice> adev, struct str_parms *parms);
    static int audio_extn_hfp_set_mic_mute2(std::shared_ptr<AudioDevice> adev, bool state);

    //HFP AG
    static int hfp_ag_feature_init(bool is_feature_enabled);
    static bool audio_extn_hfp_ag_is_active(std::shared_ptr<AudioDevice> adev);
    audio_usecase_t audio_extn_hfp_ag_get_usecase();
    static int audio_extn_hfp_ag_set_mic_mute(bool state);
    static int audio_extn_hfp_ag_set_parameters(std::shared_ptr<AudioDevice> adev, struct str_parms *parms);

    //A2DP
    static int a2dp_source_feature_init(bool is_feature_enabled);

    /* start device utils */
    static bool audio_devices_cmp(const std::set<audio_devices_t>&, audio_device_cmp_fn_t);
    static bool audio_devices_cmp(const std::set<audio_devices_t>&, audio_devices_t);
    static audio_devices_t get_device_types(const std::set<audio_devices_t>&);
    static bool audio_devices_empty(const std::set<audio_devices_t>&);
    /* end device utils */

    // FM
    static void audio_extn_fm_init(bool enabled=true);
    static void audio_extn_fm_set_parameters(std::shared_ptr<AudioDevice> adev, struct str_parms *params);
    static void audio_extn_fm_get_parameters(std::shared_ptr<AudioDevice> adev, struct str_parms *query, struct str_parms *reply);

    // ICC
    static int audio_extn_icc_set_parameters(std::shared_ptr<AudioDevice> adev, struct str_parms *parms);
    static void audio_extn_icc_get_parameters(std::shared_ptr<AudioDevice> adev, struct str_parms *query, struct str_parms *reply);
    static int icc_feature_init(bool is_feature_enabled);
    static void icc_feature_deinit();

    /* start kpi optimize perf apis */
    static void audio_extn_kpi_optimize_feature_init(bool is_feature_enabled);
    static int audio_extn_perf_lock_init(void);
    static void audio_extn_perf_lock_acquire(int *handle, int duration,
            int *perf_lock_opts, int size);
    static void audio_extn_perf_lock_release(int *handle);
    /* end kpi optimize perf apis */
    //AUTO HAL
    static int autohal_feature_init(bool is_feature_enabled);
    static pal_stream_type_t audio_extn_autohal_GetCarAudioPalStreamType(char* address);
    static void audio_extn_place_marker(char const *name, bool isEnter);
    static int audio_extn_autohal_set_parameters(std::shared_ptr<AudioDevice> adev, struct str_parms *params);
    //Power Policy
    static int power_policy_feature_init(bool is_feature_enabled);
};

#endif /* AUDIOEXTN_H */
