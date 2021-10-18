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

#define LOG_TAG "auto_hal"
#define LOG_NDDEBUG 0

#include <errno.h>
#include <log/log.h>
#include <stdlib.h>
#include <cutils/properties.h>
#include "PalApi.h"
#include "AudioDevice.h"

#ifdef __cplusplus
 extern "C" {
#endif

void autohal_init()
{
    return;
}

pal_stream_type_t autohal_GetCarAudioPalStreamType(char * address) {
    int bus_num = -1;
    char *str = NULL;
    char *last_r = NULL;
    char local_address[AUDIO_DEVICE_MAX_ADDRESS_LEN] = {0};
    pal_stream_type_t palStreamType = PAL_STREAM_PLAYBACK_BUS;

    /* strtok will modify the original string. make a copy first */
    strlcpy(local_address, address, AUDIO_DEVICE_MAX_ADDRESS_LEN);

    /* extract bus number from address */
    str = strtok_r(local_address, "BUS_",&last_r);
    if (str != NULL)
      bus_num = (int)strtol(str, (char **)NULL, 10);

    /* validate bus number */
    if ((bus_num < 0) || (bus_num >= MAX_CAR_AUDIO_STREAMS)) {
      ALOGE("%s: invalid bus number %d", __func__, bus_num);
      return palStreamType;
    }

    return palStreamType;
}

#ifdef __cplusplus
}
#endif
