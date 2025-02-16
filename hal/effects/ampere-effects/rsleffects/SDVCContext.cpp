/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL_Effect_SDVC_Rsl"

#include <Utils.h>
#include <cstddef>

#include "RslContext.h"
#include "RslTypes.h"
#include "extensions/AudioExtension.h"

#define MAX_SDVC_PROFILE_VALUE 5
#define MIN_SDVC_PROFILE_VALUE 0

namespace aidl::ampere::effects {

using aidl::android::media::audio::common::AudioDeviceDescription;
using aidl::android::media::audio::common::AudioDeviceType;

SDVCContext::SDVCContext(const Parameter::Common& common,
                                   const RslEffectType& type, bool processData)
    : RslContext(common, type, processData) {
    LOG(DEBUG) << "Enter " <<__func__ << type << " ioHandle " << common.ioHandle;

    init(); // init default state

    mState = EffectState::INITIALIZED;
}

SDVCContext::~SDVCContext() {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();
    deInit();
}

void SDVCContext::init() {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();
    memset(&mSdvcParams, 0, sizeof(struct param_type2_t));
}

void SDVCContext::deInit() {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();
    stop();
}

RetCode SDVCContext::start() {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();

    std::lock_guard lg(mMutex);

    return RetCode::SUCCESS;
}

RetCode SDVCContext::stop() {
    std::lock_guard lg(mMutex);
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();
    struct param_type2_t sdvcParam = {0}; // by default enable bit is 0
    return RetCode::SUCCESS;
}

RetCode SDVCContext::setSdvcCurrentProfile(int profile) {
    std::lock_guard lg(mMutex);
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();

    mSdvcParams.value = profile;

    if (updatePalParameters(&mSdvcParams) == 0) {
        return RetCode::SUCCESS;
    }

    return RetCode::ERROR_NULL_POINTER;
}

RetCode SDVCContext::setParameter(uint32_t cmd, int32_t param_value) {
    LOG(DEBUG) << "Enter " << __func__ << " cmd: " << cmd << " value " << param_value;

    if ( param_value < MIN_SDVC_PROFILE_VALUE || param_value > MAX_SDVC_PROFILE_VALUE ) {
        LOG(DEBUG) << __func__ << "Error in setting value, not in range 0 to 5 ";
        return RetCode::ERROR_ILLEGAL_PARAMETER;
    }

    return setSdvcCurrentProfile(param_value);
}

RetCode SDVCContext::getParameter(effect_param_t* param, uint32_t *size) {
    LOG(DEBUG) << " Enter " << __func__;

    uint64_t cmd;
    memcpy(&cmd, param->data, param->psize);

    int32_t voffset = ((param->psize - 1) / sizeof(int32_t) + 1) * sizeof(int32_t);
    void *value = param->data + voffset;

    param->status = 0;
    param->vsize = sizeof(uint64_t);
    *size = sizeof(effect_param_t) + voffset + param->vsize;
    *(int32_t *)value = getSdvcCurrentProfile();

    if (*(int32_t *)value < MIN_SDVC_PROFILE_VALUE) {
        return RetCode::ERROR_ILLEGAL_PARAMETER;
    }

    LOG(DEBUG) << " Exit " << __func__;

    return RetCode::SUCCESS;
}

RetCode SDVCContext::setOutputDevice(
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

int SDVCContext::getSdvcCurrentProfile() {
    LOG(DEBUG) << "Enter " << __func__;

    int ret = -1;
    pal_awx_param_t pal_param;
    param_type2_t params;

    memset(&pal_param, 0, sizeof(pal_awx_param_t));
    memset(&params, 0, sizeof(param_type2_t));

    pal_param.param_id = PARAM_ID_SDVC;
    pal_param.param_size = sizeof(param_type2_t);
    pal_param.data = &params;

    // Defining CAPI param Type
    effect_type type = SYNC_WITHOUT_AUDIO_BUS;
    ret = ::qti::audio::core::AWX_get_param(&pal_param, type);

    if (ret < 0) {
        LOG(ERROR) << __func__ << "Error while fetching value returned with ret: " << ret;
        return ret;
    } else {
        LOG(DEBUG) << __func__ << "Parameter fetched successfully! ret: " << params.value;
    }

    LOG(DEBUG) << "Exit " << __func__;
    return params.value;
}

int SDVCContext::updatePalParameters(struct param_type2_t *params) {
    LOG(DEBUG) << "Enter " << __func__;

    pal_awx_param_t *pal_param = (pal_awx_param_t *)malloc(sizeof(pal_awx_param_t));

    if (pal_param == NULL) {
        LOG(ERROR) << __func__ << " Error while allocating memory ";
        return -1;
    }

    pal_param->param_id = PARAM_ID_SDVC;
    pal_param->param_size = sizeof(param_type2_t);
    pal_param->data = (void *)params;

    // Defining CAPI param Type
    effect_type type = SYNC_WITHOUT_AUDIO_BUS;
    LOG(DEBUG) << __func__ << "Successfully created pal_param";

    ::qti::audio::core::AWX_set_param(pal_param, type);
    if (pal_param) {
        free(pal_param);
    }

    LOG(DEBUG) << "Exit " << __func__;
    return 0;
}
} // namespace aidl::ampere::effects
