/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "auto_hal"
#define LOG_NDDEBUG 0

#include <errno.h>
#include <log/log.h>
#include <stdlib.h>
#include <cutils/properties.h>
#include <utils/Trace.h>
#include "PalApi.h"

#include <aidl/qti/audio/core/VString.h>
#include <extensions/AudioExtension.h>

using ::aidl::qti::audio::core::VString;

#ifdef __cplusplus
 extern "C" {
#endif

#define MARKER_STRING_WIDTH 128

#define AUDIO_PARAMETER_DEVICES_TO_MUTE "DevicesToMute"
#define AUDIO_PARAMETER_DEVICES_TO_UNMUTE "DevicesToUnmute"
#define AUDIO_PARAMETER_DEVICES_TO_DUCK "DevicesToDuck"
#define AUDIO_PARAMETER_DEVICES_TO_UNDUCK "DevicesToUnduck"

#define MUTE_VOLUME -9000
#define DUCK_VOLUME -6300

#define AUDIO_DEVICE_MAX_ADDRESS_LEN 32

auto_hal_init_config_t init_config;

enum {
    MUTE,
    UNMUTE,
    DUCK,
    UNDUCK
};

static void auto_hal_set_mute_state(char* mute_bus_addr, int mute_state) {
    int ret = 0;
    char *ptr = NULL;
    char *saveptr = NULL;
    char address[AUDIO_DEVICE_MAX_ADDRESS_LEN] = {0};

    ALOGE("%s:fp mute_config %s", __func__);
    if (!init_config.fp_set_mute_config_for_address) {
        ALOGE("%s: function pointer to set_mute_config is null", __func__);
        return ;
    }
    if (mute_bus_addr == NULL) {
        ALOGE("null bus address");
        return;
    } else {
        for (ptr = strtok_r(mute_bus_addr, ",", &saveptr);
             ptr != NULL; ptr = strtok_r(NULL, ",", &saveptr)) {
             strlcpy(address, ptr, strlen(ptr) + 1);

            switch(mute_state) {
                case MUTE:
                    ALOGD("%s: Muting BUS device %s", __func__, address);
                    init_config.fp_set_mute_config_for_address(address, true, MUTE_VOLUME);
                    break;
                case UNMUTE:
                    ALOGD("%s: Unmuting BUS device %s", __func__, address);
                    init_config.fp_set_mute_config_for_address(address, false, MUTE_VOLUME);
                    break;
                case DUCK:
                    ALOGD("%s: Ducking BUS device %s", __func__, address);
                    init_config.fp_set_mute_config_for_address(address, true, DUCK_VOLUME);
                    break;
                case UNDUCK:
                    ALOGD("%s: Unducking BUS device %s", __func__, address);
                    init_config.fp_set_mute_config_for_address(address, false, DUCK_VOLUME);
                    break;
            }
        }
    }
}

void autohal_init(auto_hal_init_config_t init_config)
{
    if (init_config.fp_set_mute_config_for_address != NULL) {
        ALOGD("Function pointer is set.");
    }
    return;
}

int autohal_setParameters(struct str_parms *parms) {
    int ret = 0;
    char duck_mute_value[128] = {0};

    if (parms != NULL) {
        ALOGE("parms is not null");

        ret = str_parms_get_str(parms, AUDIO_PARAMETER_DEVICES_TO_MUTE, duck_mute_value, sizeof(duck_mute_value));
        if (ret >= 0) {
            ALOGE("mute info: %s", duck_mute_value);
            auto_hal_set_mute_state(duck_mute_value, MUTE);
        }

        ret = str_parms_get_str(parms, AUDIO_PARAMETER_DEVICES_TO_UNMUTE, duck_mute_value, sizeof(duck_mute_value));
        if (ret >= 0) {
            ALOGE("unmute info: %s", duck_mute_value);
            auto_hal_set_mute_state(duck_mute_value, UNMUTE);
        }

        ret = str_parms_get_str(parms, AUDIO_PARAMETER_DEVICES_TO_DUCK, duck_mute_value, sizeof(duck_mute_value));
        if (ret >= 0) {
            ALOGE("duck info: %s", duck_mute_value);
            auto_hal_set_mute_state(duck_mute_value, DUCK);
        }

        ret = str_parms_get_str(parms, AUDIO_PARAMETER_DEVICES_TO_UNDUCK, duck_mute_value, sizeof(duck_mute_value));
        if (ret >= 0) {
            ALOGE("unduck info: %s", duck_mute_value);
            auto_hal_set_mute_state(duck_mute_value, UNDUCK);
        }
    } else {
        ALOGE("parms is null");
        return -EINVAL;
    }

    return 0;
}

#ifdef __cplusplus
}
#endif
