/*
* Copyright (c) 2019, 2021 The Linux Foundation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
*       copyright notice, this list of conditions and the following
*       disclaimer in the documentation and/or other materials provided
*       with the distribution.
*     * Neither the name of The Linux Foundation nor the names of its
*       contributors may be used to endorse or promote products derived
*       from this software without specific prior written permission.
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
* Changes from Qualcomm Innovation Center, Inc. are provided under the following license:
* Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/
#ifndef AUTO_OEM_EXTENSION_H
#define AUTO_OEM_EXTENSION_H

#include <PalApi.h>
#include <extensions/PalParamDelegator.h>

#ifdef __cplusplus
extern "C" {
#endif

//AWX Type3 data structure
struct pal_awx_source_data
{
    uint16_t eq_mask;
    uint16_t status;
    int32_t value[16];
};

/*
    For the control command, We classify them into the following categories according to their characteristics.
    • Type1: SYNC(*1) command with bus_mask(*2) field
    • Type2: SYNC command without bus_mask field
    • Type3: ASYNC(*1) command
    • Others: Commands which are nothing to do with audio feature control or not used by customers.

    (*1):
    For the command which will be finished in set_param call, we call them SYNC command.
    For the command which Harman module needs to mute->load EQ files->unmute, it may take hundreds of millisecond. To
    avoid blocking set_param API, Harman module will have a quickly check and return in set_param call, and processing it
    later, we call such CMDs ASYNC command.
    For ASYNC CMD, we define below two states:
    • IDLE: this CMD is IDLE, user can update the value of this CMD.
    • BUSY: this CMD is busy on executing, user can’t update the value of this CMD before it’s become IDLE.

    (*2):
    some control command related with audio bus, we use bus_mask field to specify these info
*/
typedef enum {
    Type1,
    Type2,
    Type3,
    Other
} param_type;

//CAPI module Type2 param structure
struct param_type2_t
{
    int32_t value;
};

int set_vehicle_speed(int32_t);
int set_fan_speed(int32_t);
void update_fan_speed_pal_param(param_type2_t *);
void update_vehicle_speed_pal_param(struct param_type2_t *);
int set_oem_audio_source_params(struct str_parms *);
int find_source_type(struct str_parms *);
int oem_pal_param_update(const std::string&);
void update_audiosourcedata(struct pal_awx_source_data*);

#ifdef __cplusplus
}
#endif

#endif // AUTO_OEM_EXTENSION_H