 /*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL_Effect_Ambiance_Rsl"

#include <Utils.h>
#include <cstddef>

#include "RslContext.h"
#include "RslTypes.h"
#include "extensions/AudioExtension.h"

#define MAX_PROFILE_VALUE 3
#define MIN_PROFILE_VALUE 0
#define BIT0 0x1

namespace aidl::ampere::effects {

using aidl::android::media::audio::common::AudioDeviceDescription;
using aidl::android::media::audio::common::AudioDeviceType;

AmbianceContext::AmbianceContext(const Parameter::Common& common,
                                   const RslEffectType& type, bool processData)
    : RslContext(common, type, processData) {
    LOG(DEBUG) << "Enter " <<__func__ << type << " ioHandle " << common.ioHandle;

    init(); // init default state

    mState = EffectState::INITIALIZED;
}

AmbianceContext::~AmbianceContext() {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();
    deInit();
}

void AmbianceContext::init() {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();
    memset(&mAmbianceParams, 0, sizeof(struct AmbianceParams));
}

void AmbianceContext::deInit() {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();
    stop();
}

RetCode AmbianceContext::start() {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();

    std::lock_guard lg(mMutex);

    return RetCode::SUCCESS;
}

RetCode AmbianceContext::stop() {
    std::lock_guard lg(mMutex);
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();

    struct AmbianceParams ambianceParam = {0}; // by default enable bit is 0

    return RetCode::SUCCESS;
}

RetCode AmbianceContext::setAmbianceProfile(int profile) {
    std::lock_guard lg(mMutex);
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();

    if ( profile < MIN_PROFILE_VALUE || profile > MAX_PROFILE_VALUE ) {
        LOG(DEBUG) << __func__ << " Error in setting value, not in range 0 to 3 ";
        return RetCode::ERROR_ILLEGAL_PARAMETER;
    }

    mAmbianceParams.value[0] = profile;

    if (updatePalParameters(&mAmbianceParams) == 0) {
        return RetCode::SUCCESS;
    }

    return RetCode::ERROR_NULL_POINTER;
}

int AmbianceContext::getAmbianceProfile() {
    LOG(DEBUG) << "Enter " << __func__;

    int ret = -1;
    pal_awx_param_t pal_param;
    AmbianceParams params;

    memset(&params, 0, sizeof(AmbianceParams));
    memset(&pal_param, 0, sizeof(pal_awx_param_t));

    pal_param.param_id = PARAM_ID_AMBIANCE;
    pal_param.param_size = sizeof(AmbianceParams);
    pal_param.data = &params;

    // Defining CAPI param Type
    effect_type type = ASYNC;
    ret = ::qti::audio::core::AWX_get_param(&pal_param, type);

    if (ret < 0) {
        LOG(ERROR) << __func__ << "Error while fetching value returned with ret: " << ret;
        return ret;
    } else {
        LOG(DEBUG) << __func__ << "Parameter fetched successfully! ret: " << params.value[0];
    }

    LOG(DEBUG) << "Exit " << __func__;

    return params.value[0];
}

RetCode AmbianceContext::setParameter(uint32_t cmd, int32_t param_value) {
    LOG(DEBUG) << "Enter " << __func__;
    return setAmbianceProfile(param_value);
}

RetCode AmbianceContext::getParameter(effect_param_t* param, uint32_t *size) {
    LOG(DEBUG) << "Enter " << __func__;

    uint64_t cmd;
    memcpy(&cmd, param->data, param->psize);
    LOG(DEBUG) << __func__ << " cmd: "<< cmd;

    int32_t voffset = ((param->psize - 1) / sizeof(int32_t) + 1) * sizeof(int32_t);
    void *value = param->data + voffset;

    param->status = 0;
    param->vsize = sizeof(uint64_t);
    *size = sizeof(effect_param_t) + voffset + param->vsize;
    *(int32_t *)value = getAmbianceProfile();

    if (*(int32_t *)value < MIN_PROFILE_VALUE) {
        LOG(ERROR) << __func__ << " Unsupported parameter ";
        return RetCode::ERROR_ILLEGAL_PARAMETER;
    }

    LOG(DEBUG) << "Exit " << __func__;

    return RetCode::SUCCESS;
}

RetCode AmbianceContext::setOutputDevice(
        const std::vector<aidl::android::media::audio::common::AudioDeviceDescription>& device) {
    LOG(DEBUG) << "Enter " << __func__;

    std::lock_guard lg(mMutex);
    mOutputDevice = device;

    if (deviceSupportsEffect(mOutputDevice)) {
        mTempDisabled = false;
    } else if (!mTempDisabled) {
        mTempDisabled = true;
    }

    LOG(DEBUG) << "Exit " << __func__;

    return RetCode::SUCCESS;
}

int AmbianceContext::updatePalParameters(struct AmbianceParams *params) {
    LOG(DEBUG) << "Enter " << __func__;

    pal_awx_param_t *pal_param = (pal_awx_param_t *)malloc(sizeof(pal_awx_param_t));

    if (pal_param == NULL) {
        LOG(ERROR) << __func__ << " Error while allocating memory ";
        return -1;
    }

    pal_param->param_id = PARAM_ID_AMBIANCE;
    pal_param->param_size = sizeof(AmbianceParams);

    // setting bit0 of eq_mask for ambiance mode
    params->eq_mask = 0;
    params->eq_mask |= BIT0;
    pal_param->data = (void *)params;

    // Defining CAPI param Type
    effect_type type = ASYNC;
    LOG(DEBUG) << __func__ << "Successfully created pal_param";

    ::qti::audio::core::AWX_set_param(pal_param, type);
    if (pal_param) {
        free(pal_param);
    }

    LOG(DEBUG) << "Exit " << __func__;

    return 0;
}
} // namespace aidl::ampere::effects
