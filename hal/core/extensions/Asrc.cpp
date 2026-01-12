/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

 #define LOG_TAG "ASRC_Extension"
#include <errno.h>
#include <log/log.h>
#include <stdlib.h>

#include <utils/Trace.h>
#include "PalApi.h"
#include "PalDefs.h"
#include <aidl/qti/audio/core/VString.h>
#include <extensions/AudioExtension.h>

using ::aidl::qti::audio::core::VString;

/* ascrc_ratio parameteres */
asrc_ratio_t asrc_ratio = {0,0,0};

extern "C" {
    void asrc_set_parameters(struct str_parms* params) {
        int32_t ret = -EINVAL;
        char value[256] = {0};
        ret = str_parms_get_str(params, "asrc_start", value, sizeof(value));
        if (ret >= 0) {
            LOG(DEBUG) << __func__ << ": Getting ASRC parameters !";
            ret = str_parms_get_str(params, "asrc_effective", value, sizeof(value));
            if (ret >= 0) {
                sscanf(value, "%x", &(asrc_ratio.effective));
                LOG(DEBUG) << __func__ << ": asrc effective = " << asrc_ratio.effective;
            }

            ret = str_parms_get_str(params, "asrc_ratio", value, sizeof(value));
            if (ret >= 0) {
                sscanf(value, "%x", &(asrc_ratio.ratio));
                LOG(DEBUG) << __func__ << ": asrc ratio = " << asrc_ratio.ratio;
            }

            ret = str_parms_get_str(params, "asrc_ramp", value, sizeof(value));
            if (ret >= 0) {
                sscanf(value, "%x", &(asrc_ratio.ramp));
                LOG(DEBUG) << __func__ << ": asrc ramp = " << asrc_ratio.ramp;
            }

            ret = pal_set_param(PAL_PARAM_ID_ASRC, &asrc_ratio, sizeof(asrc_ratio));
            if (ret < 0) {
                LOG(ERROR) << __func__ << ": pal_set_param failed, err = " << ret;
                return;
            }
            return;
        }
    }
}

