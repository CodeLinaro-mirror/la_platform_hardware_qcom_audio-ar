/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
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
/* Changes from Qualcomm Innovation Center are provided under the following license:
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include "AudioDevice.h"

#ifdef __cplusplus
 extern "C" {
#endif

auto_hal_init_config_t auto_config_ = {0};

#define MARKER_STRING_WIDTH 128

#define AUDIO_PARAMETER_DEVICES_TO_MUTE "DevicesToMute"
#define AUDIO_PARAMETER_DEVICES_TO_UNMUTE "DevicesToUnmute"
#define AUDIO_PARAMETER_DEVICES_TO_DUCK "DevicesToDuck"
#define AUDIO_PARAMETER_DEVICES_TO_UNDUCK "DevicesToUnduck"

#define DUCKED_VOLUME 0.0035

enum {
    STATE_ON,
    STATE_OFF
};

static void auto_hal_set_mute_state(char* mute_bus_addr, int mute_state) {
    int ret = 0;
    char *ptr = NULL;
    char *saveptr = NULL;
    char address[AUDIO_DEVICE_MAX_ADDRESS_LEN] = {0};

    for (ptr = strtok_r(mute_bus_addr, ",", &saveptr);
         ptr != NULL; ptr = strtok_r(NULL, ",", &saveptr)) {

        strlcpy(address, ptr, strlen(ptr) + 1);

        switch(mute_state) {
            case STATE_ON:
                ALOGD("%s: Muting BUS device %s", __func__, address);
                auto_config_.fp_set_mute_config_for_address(true, address);
                break;
            case STATE_OFF:
                ALOGD("%s: Unmuting BUS device %s", __func__, address);
                auto_config_.fp_set_mute_config_for_address(false, address);
                break;
        }
    }
}

static void auto_hal_set_duck_state(char* duck_bus_addr, int duck_state) {
    int ret = 0;
    char *ptr = NULL;
    char *saveptr = NULL;
    pal_stream_bus_duck_t duck_param;

    for (ptr = strtok_r(duck_bus_addr, ",", &saveptr);
         ptr != NULL; ptr = strtok_r(NULL, ",", &saveptr)) {

        switch(duck_state) {
            case STATE_ON:
                ALOGD("%s: ducking BUS device %s", __func__, ptr);
                duck_param.duck = true;
                duck_param.duck_volume = DUCKED_VOLUME;
                strlcpy(duck_param.bus_addr, ptr, strlen(ptr) + 1);
                ret = pal_set_param(PAL_PARAM_ID_STREAM_BUS_DUCK_CONFIG,
                                    (void*)&duck_param,
                                    sizeof(pal_stream_bus_duck_t));
                break;
            case STATE_OFF:
                ALOGD("%s: Unducking BUS device %s", __func__, ptr);
                duck_param.duck = false;
                strlcpy(duck_param.bus_addr, ptr, strlen(ptr) + 1);
                ret = pal_set_param(PAL_PARAM_ID_STREAM_BUS_DUCK_CONFIG,
                                    (void*)&duck_param,
                                    sizeof(pal_stream_bus_duck_t));
                break;
        }
    }
}

void autohal_init(auto_hal_init_config_t init_config)
{
    auto_config_.fp_set_mute_config_for_address =
                init_config.fp_set_mute_config_for_address;
    return;
}

pal_stream_type_t autohal_GetCarAudioPalStreamType(char * address) {
    int bus_num = -1;
    char *str = NULL;
    char *last_r = NULL;
    char local_address[AUDIO_DEVICE_MAX_ADDRESS_LEN] = {0};
    pal_stream_type_t palStreamType = PAL_STREAM_INVALID;

    if ((strcmp(address, "BUS00_MEDIA") == 0) ||
        (strcmp(address, "BUS01_SYS_NOTIFICATION") == 0) ||
        (strcmp(address, "BUS02_NAV_GUIDANCE") == 0) ||
        (strcmp(address, "BUS03_PHONE") == 0) ||
        (strcmp(address, "BUS05_ALERTS") == 0) ||
        (strcmp(address, "BUS08_FRONT_PASSENGER") == 0) ||
        (strcmp(address, "BUS16_REAR_SEAT") == 0)) {
        palStreamType = PAL_STREAM_PLAYBACK_BUS;
    } else if ((strcmp(address, "BUS04_INPUT") == 0) ||
        (strcmp(address, "BUS09_INPUT_FRONT_PASSENGER") == 0) ||
        (strcmp(address, "BUS17_INPUT_REAR_SEAT") == 0)) {
        palStreamType = PAL_STREAM_CAPTURE_BUS;
    } else {
        ALOGE("%s: invalid address %s", __func__, address);
    }

    return palStreamType;
}

/*Enable Place Marker*/
void place_marker(char const *name, bool isEnter)
{
    int fd=open("/sys/kernel/boot_kpi/kpi_values", O_WRONLY);
    if (fd > 0)
    {
        /* Only allow marker text shorter than MARKER_STRING_WIDTH */
        char earlyapp[MARKER_STRING_WIDTH] = {0};
        strlcpy(earlyapp, name, sizeof(earlyapp));
        write(fd, earlyapp, strlen(earlyapp));
        close(fd);
    }

    if (isEnter) {
        ATRACE_BEGIN(name);
    } else {
        ATRACE_END();
    }
}

int autohal_SetParameters(std::shared_ptr<AudioDevice> adev __unused, struct str_parms *parms) {
    int ret = 0;
    char duck_mute_value[128] = {0};

    ret = str_parms_get_str(parms,
                            AUDIO_PARAMETER_DEVICES_TO_MUTE,
                            duck_mute_value,
                            sizeof(duck_mute_value));
    if (ret >= 0)
        auto_hal_set_mute_state(duck_mute_value, STATE_ON);

    ret = str_parms_get_str(parms,
                            AUDIO_PARAMETER_DEVICES_TO_UNMUTE,
                            duck_mute_value,
                            sizeof(duck_mute_value));
    if (ret >= 0)
        auto_hal_set_mute_state(duck_mute_value, STATE_OFF);

    ret = str_parms_get_str(parms,
                            AUDIO_PARAMETER_DEVICES_TO_DUCK,
                            duck_mute_value,
                            sizeof(duck_mute_value));
    if (ret >= 0)
        auto_hal_set_duck_state(duck_mute_value, STATE_ON);

    ret = str_parms_get_str(parms,
                            AUDIO_PARAMETER_DEVICES_TO_UNDUCK,
                            duck_mute_value,
                            sizeof(duck_mute_value));
    if (ret >= 0)
        auto_hal_set_duck_state(duck_mute_value, STATE_OFF);
    return 0;
}

#ifdef __cplusplus
}
#endif
