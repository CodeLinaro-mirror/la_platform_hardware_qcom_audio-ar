/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL_Effect_SteadyVolume_Rsl"

#include <Utils.h>
#include <cstddef>

#include "RslContext.h"
#include "RslTypes.h"
#include "extensions/AudioExtension.h"
#include <system/audio_effects/audio_effects_utils.h>
#include "aidl/android/hardware/audio/effect/DefaultExtension.h"
#include <system/audio_effect.h>

#define MIN_STEADY_VOLUME_VALUE 0
#define MAX_STEADY_VOLUME_VALUE 1

namespace aidl::ampere::effects {

using aidl::android::media::audio::common::AudioDeviceDescription;
using aidl::android::media::audio::common::AudioDeviceType;
using namespace ::android::effect::utils;
using aidl::android::hardware::audio::effect::DefaultExtension;


SteadyVolumeContext::SteadyVolumeContext(const Parameter::Common& common,
                                   const RslEffectType& type, bool processData)
    : RslContext(common, type, processData) {
    LOG(DEBUG) << "Enter " <<__func__ << type << " ioHandle " << common.ioHandle;

    init(); // init default state

    mState = EffectState::INITIALIZED;
}

SteadyVolumeContext::~SteadyVolumeContext() {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();
    deInit();
}

void SteadyVolumeContext::init() {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();
    memset(&mSteadyVolumeParams, 0, sizeof(struct param_type2_t));
}

void SteadyVolumeContext::deInit() {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();
    stop();
}

RetCode SteadyVolumeContext::start() {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();

    std::lock_guard lg(mMutex);
    RetCode code = setSteadyVolume(MIN_STEADY_VOLUME_VALUE);
    mState = EffectState::ACTIVE;
    if (RetCode::SUCCESS == code)
    {
        LOG(DEBUG) << "Set SteadyVolume is success  " << code ;
    }
    else
    {
        LOG(DEBUG) << "Set SteadyVolume is failed  " << code ;
    }
    return RetCode::SUCCESS;
}

RetCode SteadyVolumeContext::stop() {
    std::lock_guard lg(mMutex);
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();

    struct param_type2_t steadyVolumeParam = {0}; // by default enable bit is 0
    mState = EffectState::UNINITIALIZED;
    RetCode code = setSteadyVolume(MIN_STEADY_VOLUME_VALUE);
    if (RetCode::SUCCESS == code)
    {
        LOG(DEBUG) << "Set SteadyVolume is success  " << code ;
    }
    else
    {
        LOG(DEBUG) << "Set SteadyVolume is failed  " << code ;
    }
    return RetCode::SUCCESS;
}

RetCode SteadyVolumeContext::setSteadyVolume(int value) {

    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();

    mSteadyVolumeParams.value = value;

    if (updatePalParameters(&mSteadyVolumeParams) == 0) {
        return RetCode::SUCCESS;
    }

    LOG(DEBUG) << "Exit " << __func__;

    return RetCode::ERROR_NULL_POINTER;
}

RetCode SteadyVolumeContext::setParameter(const std::vector<uint8_t>& specific)
{
    std::lock_guard lg(mMutex);
    LOG(DEBUG) << __func__ << ": SteadyVolume Enter";
    auto reader = EffectParamReader(*(effect_param_t*)specific.data());

    uint32_t type;
    if (::android::OK != reader.readFromParameter(&type)) {
        LOG(ERROR) << __func__ << " invalid param " << reader.toString().c_str();
        return {};
    }
    LOG(DEBUG) << __func__ << ": Setting SteadyVolume = " << type;

    if (setSteadyVolume(type) != RetCode::SUCCESS)
    {
        LOG(ERROR) << __func__ << " setSteadyVolume Failed " ;
    }
    LOG(DEBUG) << __func__ << ":  SteadyVolume Exit";

return RetCode::SUCCESS;
}

std::vector<uint8_t> SteadyVolumeContext::getParameter(std::vector<uint8_t> id)
{
    std::lock_guard lg(mMutex);
    int32_t steadyvolume_switch;
    LOG(DEBUG) << "Enter " << __func__;
    auto reader = EffectParamReader(*(effect_param_t*)id.data());
    auto paramWriter = EffectParamWriter(*(effect_param_t*)id.data());

    uint32_t type;
    if (::android::OK != reader.readFromParameter(&type)) {
        LOG(ERROR) << __func__ << " invalid param " << reader.toString().c_str();
        return {};
    }

    if (type < MIN_STEADY_VOLUME_VALUE || type > MAX_STEADY_VOLUME_VALUE ) {
        LOG(ERROR) << __func__ << "not getting 0(OFF) or 1(ON), fetching error in steadyvolume";
        return {};
    }

    size_t valueSize;
    int ret = getSteadyVolume();
    if (ret < 0) {
        LOG(ERROR) << __func__ << ": get steadyvolume failed " << ret;
        return {};
    } else {
        steadyvolume_switch = ret;
    }

    valueSize = sizeof(steadyvolume_switch);
    LOG(DEBUG) << __func__ << ": valueSize = " << valueSize <<
    " steadyvolume effect get paramWrite structure =  " << paramWriter.toString().c_str();
    paramWriter.writeToValue(&steadyvolume_switch);
    paramWriter.setStatus(::android::OK);

    DefaultExtension replyExt;
    size_t len = paramWriter.getTotalSize();
    replyExt.bytes.resize(len);
    std::memcpy(replyExt.bytes.data(), (void *) &paramWriter.getEffectParam(), len);
    LOG(DEBUG) << "Exit " << __func__;
    return replyExt.bytes;
}

RetCode SteadyVolumeContext::setParameter(uint32_t cmd, int32_t param_value) {
    std::lock_guard lg(mMutex);
    LOG(DEBUG) << "Enter " << __func__ << " cmd: " << cmd << " value " << param_value;

    if ( param_value < MIN_STEADY_VOLUME_VALUE || param_value > MAX_STEADY_VOLUME_VALUE ) {
        LOG(DEBUG) << __func__ << "Error in setting value, not in range 0 to 1 ";
        return RetCode::ERROR_ILLEGAL_PARAMETER;
    }

    LOG(DEBUG) << "Exit " << __func__;

    return setSteadyVolume(param_value);
}

RetCode SteadyVolumeContext::getParameter(effect_param_t* param, uint32_t *size) {
    LOG(DEBUG) << "Enter " << __func__;
    std::lock_guard lg(mMutex);
    uint64_t cmd;
    memcpy(&cmd, param->data, param->psize);

    int32_t voffset = ((param->psize - 1) / sizeof(int32_t) + 1) * sizeof(int32_t);
    void *value = param->data + voffset;

    param->status = 0;
    param->vsize = sizeof(uint64_t);
    *size = sizeof(effect_param_t) + voffset + param->vsize;
    *(int32_t *)value = getSteadyVolume();

    if (*(int32_t *)value < MIN_STEADY_VOLUME_VALUE) {
        LOG(ERROR) << __func__ << " Invalid param value";
        return RetCode::ERROR_ILLEGAL_PARAMETER;
    }

    LOG(DEBUG) << " Exit " << __func__;

    return RetCode::SUCCESS;
}

RetCode SteadyVolumeContext::setOutputDevice(
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

int SteadyVolumeContext::getSteadyVolume(){
    LOG(DEBUG) << "Enter " << __func__;

    int ret =-1;
    pal_awx_param_t pal_param;
    struct param_type2_t params;

    memset(&pal_param, 0, sizeof(pal_awx_param_t));
    memset(&params, 0, sizeof(param_type2_t));

    pal_param.param_id = PARAM_ID_STEADY_VOLUME;
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

int SteadyVolumeContext::updatePalParameters(struct param_type2_t *params) {
    LOG(DEBUG) << "Enter " << __func__;

    pal_awx_param_t *pal_param = (pal_awx_param_t *)malloc(sizeof(pal_awx_param_t));

    if (pal_param == NULL) {
        LOG(ERROR) << __func__ << "Memory not assigned properly";
        return -1;
    }

    pal_param->param_id = PARAM_ID_STEADY_VOLUME;
    pal_param->param_size = sizeof(param_type2_t);
    pal_param->data = (void *)params;

    // Defining CAPI param Type
    effect_type type = SYNC_WITHOUT_AUDIO_BUS;
    LOG(DEBUG) << __func__ << "Successfully created pal_param";

    ::qti::audio::core::AWX_set_param(pal_param, type);
    LOG(DEBUG) << "Exit " << __func__;

    if (pal_param) {
        free(pal_param);
    }

    return 0;
}
} // namespace aidl::ampere::effects
