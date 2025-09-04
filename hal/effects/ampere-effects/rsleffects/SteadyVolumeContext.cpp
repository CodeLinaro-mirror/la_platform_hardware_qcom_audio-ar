/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL_Effect_SteadyVolume_Rsl"

#include <Utils.h>
#include <cstddef>

#include "RslContext.h"
#include "RslTypes.h"
#include "PalParamDelegator.h"
#include <system/audio_effects/audio_effects_utils.h>
#include "aidl/android/hardware/audio/effect/DefaultExtension.h"
#include <system/audio_effect.h>
#include "AudioConfig.h"

#define MIN_STEADY_VOLUME_VALUE 0
#define MAX_STEADY_VOLUME_VALUE 1

namespace aidl::ampere::effects {

using aidl::android::media::audio::common::AudioDeviceDescription;
using aidl::android::media::audio::common::AudioDeviceType;
using namespace ::android::effect::utils;
using aidl::android::hardware::audio::effect::DefaultExtension;
using namespace ::aidl::qti::awx;


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
    ::qti::audio::oem::config::AudioConfigType req = ::qti::audio::oem::config::AUDIO_CONFIG_DEFAULT_AGC_STATE ;
    ::qti::audio::oem::config::AudioConfigData agcConfig;
    ::qti::audio::oem::config::AudioConfigManager::getInstance().getAudioConfigValue(req,&agcConfig);
    LOG(DEBUG) << "Default Value of Defautl AGC/Steady Voluem  " << agcConfig.defaultValue;
    memset(&mSteadyVolumeParams, 0, sizeof(struct param_type2_t));
    mSteadyVolumeParams.value = agcConfig.defaultValue;
}

void SteadyVolumeContext::deInit() {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();
    stop();
}

RetCode SteadyVolumeContext::start(pal_stream_handle_t* palHandle) {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();
    mPalHandle = palHandle;
    std::lock_guard lg(mMutex);
    if (isEffectActive()) {
        setSteadyVolume(MAX_STEADY_VOLUME_VALUE);
    } else {
        LOG(DEBUG) << "Not yet enabled";
    }
    return RetCode::SUCCESS;
}

RetCode SteadyVolumeContext::stop() {
    std::lock_guard lg(mMutex);
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();
    struct param_type2_t steadyVolumeParam = {0}; // by default enable bit is 0
    mState = EffectState::UNINITIALIZED;
    setSteadyVolume(MIN_STEADY_VOLUME_VALUE);
    return RetCode::SUCCESS;
}

RetCode SteadyVolumeContext::enable() {
    std::lock_guard lg(mMutex);
    LOG(DEBUG) << __func__ << " ioHandle " << getIoHandle();

    if (isEffectActive())
     return RetCode::ERROR_ILLEGAL_PARAMETER;

    RetCode code = setSteadyVolume(MAX_STEADY_VOLUME_VALUE);
    mState = EffectState::ACTIVE;
    if (RetCode::SUCCESS == code) {
        LOG(DEBUG) << "Set SteadyVolume is success  " << code ;
    } else {
        LOG(DEBUG) << "Set SteadyVolume is failed  " << code ;
    }

    return RetCode::SUCCESS;
}

RetCode SteadyVolumeContext::disable() {
    std::lock_guard lg(mMutex);
    LOG(DEBUG) << __func__ << " ioHandle " << getIoHandle();
    if (!isEffectActive()) return RetCode::ERROR_ILLEGAL_PARAMETER;
    mState = EffectState::INITIALIZED;
    RetCode code = setSteadyVolume(MIN_STEADY_VOLUME_VALUE);
    if (RetCode::SUCCESS == code) {
        LOG(DEBUG) << "Set SteadyVolume is success  " << code ;
    } else {
        LOG(DEBUG) << "Set SteadyVolume is failed  " << code ;
    }
    return RetCode::SUCCESS;
}


RetCode SteadyVolumeContext::setSteadyVolume(int value) {

    LOG(VERBOSE) << "Enter " << __func__ << " ioHandle " << getIoHandle();

    mSteadyVolumeParams.value = value;

    if (updatePalParameters(&mSteadyVolumeParams) == 0) {
        return RetCode::SUCCESS;
    }

    LOG(VERBOSE) << "Exit " << __func__;

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

    if (setSteadyVolume(type) != RetCode::SUCCESS) {
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

    if (mPalHandle != NULL) {
        ret = PalParamDelegator::AWX_get_param_handle(mPalHandle,&pal_param, type);
    } else {
        ret = PalParamDelegator::AWX_get_param(&pal_param, type);
        LOG(DEBUG) << "PAL handle is NULL " << __func__;
    }

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


    if (mPalHandle != NULL) {
        PalParamDelegator::AWX_set_param_handle(mPalHandle,pal_param, type);
    } else {
        PalParamDelegator::AWX_set_param(pal_param, type);
        LOG(DEBUG) << "PAL handle is NULL " << __func__;
    }
    LOG(DEBUG) << "Exit " << __func__;

    if (pal_param) {
        free(pal_param);
    }

    return 0;
}
} // namespace aidl::ampere::effects
