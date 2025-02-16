/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL_Effect_BassBoost_Rsl"

#include <Utils.h>
#include <cstddef>

#include "RslContext.h"
#include "RslTypes.h"
#include "extensions/AudioExtension.h"

#define MIN_BASS_BOOST_VALUE 0
#define MAX_BASS_BOOST_VALUE 1

namespace aidl::ampere::effects {

using aidl::android::media::audio::common::AudioDeviceDescription;
using aidl::android::media::audio::common::AudioDeviceType;

BassBoostContext::BassBoostContext(const Parameter::Common& common,
                                   const RslEffectType& type, bool processData)
    : RslContext(common, type, processData) {
    LOG(DEBUG) << "Enter " <<__func__ << type << " ioHandle " << common.ioHandle;

    init(); // init default state

    mState = EffectState::INITIALIZED;
}

BassBoostContext::~BassBoostContext() {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();
    deInit();
}

void BassBoostContext::init() {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();
    memset(&mBassBoostParams, 0, sizeof(struct AmbianceParams));
}

void BassBoostContext::deInit() {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();
    stop();
}

RetCode BassBoostContext::start() {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();

    std::lock_guard lg(mMutex);

    return RetCode::SUCCESS;
}

RetCode BassBoostContext::stop() {
    std::lock_guard lg(mMutex);
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();

    struct param_type2_t bassBoostParam = {0}; // by default enable bit is 0

    return RetCode::SUCCESS;
}

RetCode BassBoostContext::setBassBoost(int bass) {
    std::lock_guard lg(mMutex);
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();

    mBassBoostParams.value[1] = bass;

    if (updatePalParameters(&mBassBoostParams) == 0) {
        return RetCode::SUCCESS;
    }

    LOG(DEBUG) << "Exit " << __func__;

    return RetCode::ERROR_NULL_POINTER;
}

int BassBoostContext::getBassBoost(){
    LOG(DEBUG) << "Enter " << __func__;

    int ret = -1;
    pal_awx_param_t pal_param;
    AmbianceParams params;

    memset(&pal_param, 0, sizeof(pal_awx_param_t));
    memset(&params, 0, sizeof(AmbianceParams));

    pal_param.param_id = PARAM_ID_AMBIANCE;
    pal_param.param_size = sizeof(AmbianceParams);
    pal_param.data = &params;

    // Defining CAPI param Type
    effect_type type = ASYNC;
    ret = ::qti::audio::core::AWX_get_param(&pal_param, type);

    if(ret < 0) {
        LOG(ERROR) << __func__ << "Error while fetching value returned with ret: " << ret;
        return ret;
    } else {
        LOG(DEBUG) << __func__ << "Parameter fetched successfully! ret: " << params.value[1];
    }

    LOG(DEBUG) << "Exit " << __func__;

    return params.value[1];
}

RetCode BassBoostContext::setParameter(uint32_t cmd, int32_t param_value) {
    LOG(DEBUG) << "Enter " << __func__ << " cmd: " << cmd << " value " << param_value;

    if (param_value < MIN_BASS_BOOST_VALUE || param_value > MAX_BASS_BOOST_VALUE) {
        LOG(DEBUG) << __func__ << "Error in setting value, not in range 0 to 3 ";
        return RetCode::ERROR_ILLEGAL_PARAMETER;
    }

    LOG(DEBUG) << "Exit " << __func__;

    return setBassBoost(param_value);
}

RetCode BassBoostContext::getParameter(effect_param_t* param, uint32_t *size) {
    LOG(DEBUG) << "Enter " << __func__;

    uint64_t cmd;
    memcpy(&cmd, param->data, param->psize);

    int32_t voffset = ((param->psize - 1) / sizeof(int32_t) + 1) * sizeof(int32_t);
    void *value = param->data + voffset;

    param->status = 0;
    param->vsize = sizeof(uint64_t);
    *size = sizeof(effect_param_t) + voffset + param->vsize;
    *(int32_t *)value = getBassBoost();

    if (*(int32_t *)value < MIN_BASS_BOOST_VALUE) {
        LOG(ERROR) << __func__ << " Unsupported parameter ";
        return RetCode::ERROR_ILLEGAL_PARAMETER;
    }

    LOG(DEBUG) << " Exit " << __func__;

    return RetCode::SUCCESS;
}

RetCode BassBoostContext::setOutputDevice(
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

int BassBoostContext::updatePalParameters(struct AmbianceParams *params) {
    LOG(DEBUG) << "Enter " << __func__;

    pal_awx_param_t *pal_param = (pal_awx_param_t *)malloc(sizeof(pal_awx_param_t));

    if (pal_param == NULL) {
        LOG(ERROR) << __func__ << " Error while memory allocation ";
        return -1;
    }

    pal_param->param_id = PARAM_ID_AMBIANCE;
    pal_param->param_size = sizeof(AmbianceParams);

    // setting bit0 of eq_mask for ambiance mode
    params->eq_mask = 0;
    params->eq_mask |= 0x2;
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
