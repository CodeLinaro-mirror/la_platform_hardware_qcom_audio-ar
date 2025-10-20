/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL_Effect_BassBoost_Rsl"

#include <Utils.h>
#include <cstddef>

#include "RslContext.h"
#include "RslTypes.h"
#include "PalParamDelegator.h"

#define MIN_BASS_BOOST_VALUE 0
#define MAX_BASS_BOOST_VALUE 1

namespace aidl::ampere::effects {

using aidl::android::media::audio::common::AudioDeviceDescription;
using aidl::android::media::audio::common::AudioDeviceType;
using namespace ::aidl::qti::awx;

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
    memset(&mBassBoostSyncParams, 0, sizeof(struct param_type2_t));
}

void BassBoostContext::deInit() {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();
    stop();
}

RetCode BassBoostContext::enable() {
    std::lock_guard lg(mMutex);
    LOG(DEBUG) << __func__ << " ioHandle " << getIoHandle();
    if (isEffectActive())
     return RetCode::ERROR_ILLEGAL_PARAMETER;
    mState = EffectState::ACTIVE;
    mBassBoostSyncParams.value = MAX_BASS_BOOST_VALUE ;
    setOffloadParameters();
    return RetCode::SUCCESS;
}

RetCode BassBoostContext::disable() {
    std::lock_guard lg(mMutex);
    LOG(DEBUG) << __func__ << " ioHandle " << getIoHandle();
    if (!isEffectActive()) return RetCode::ERROR_ILLEGAL_PARAMETER;
    mState = EffectState::INITIALIZED;
    mBassBoostSyncParams.value = MIN_BASS_BOOST_VALUE ;
    setOffloadParameters();
    return RetCode::SUCCESS;
}

RetCode BassBoostContext::start(pal_stream_handle_t* palHandle) {
    std::lock_guard lg(mMutex);
    LOG(DEBUG) << __func__ << " ioHandle " << getIoHandle() ;
    mPalHandle = palHandle;
    if (isEffectActive()) {
        setOffloadParameters();
    } else {
        LOG(DEBUG) << "Not yet enabled";
    }
    return RetCode::SUCCESS;
}


RetCode BassBoostContext::stop() {
    std::lock_guard lg(mMutex);
    struct param_type2_t BassBoostSyncParams = { 0 }; // Value 0 is disable

    LOG(DEBUG) << __func__ << " ioHandle " << getIoHandle();
    updatePalParameters(&BassBoostSyncParams);
    mPalHandle = nullptr;
    return RetCode::SUCCESS;
}

int BassBoostContext::setOffloadParameters() {

    LOG(DEBUG) << " Bass BOOST value " << mBassBoostSyncParams.value;
    setBassBoost(mBassBoostSyncParams.value);

    return 0;
}

RetCode BassBoostContext::setBassBoost(int bass) {

    LOG(VERBOSE) << "Enter " << __func__ << " ioHandle " << getIoHandle();

    mBassBoostSyncParams.value = bass ;

    if (updatePalParameters(&mBassBoostSyncParams) == 0) {
        LOG(DEBUG) << "updatePalParameters Bass Boost Successful ";
    }

    LOG(VERBOSE) << "Exit " << __func__;

    return RetCode::SUCCESS;
}

int BassBoostContext::getBassBoostStrength(){
    LOG(DEBUG) << "Enter Strength Not Supported by AWX module return 0 " << __func__;

    return 0;
}
RetCode BassBoostContext::setBassBoostStrength(int strength){
    LOG(DEBUG) << __func__ << " strength not supported by AWX module Do Nothing " << strength;
    return RetCode::SUCCESS;
}
int BassBoostContext::getBassBoost(){
    LOG(DEBUG) << "Enter " << __func__;

    int ret = -1;
    pal_awx_param_t pal_param;

    struct param_type2_t Bassbootparams;

    memset(&pal_param, 0, sizeof(pal_awx_param_t));
    memset(&Bassbootparams, 0, sizeof(Bassbootparams));


    pal_param.param_id = PARAM_ID_BASS_MANAGER;
    pal_param.param_size = sizeof(Bassbootparams);
    pal_param.data = &Bassbootparams;

    // Defining CAPI param Type
    effect_type type = SYNC_WITHOUT_AUDIO_BUS;

    if (mPalHandle != NULL) {
        ret = PalParamDelegator::AWX_get_param_handle(mPalHandle,&pal_param, type);
    } else {
        ret = PalParamDelegator::AWX_get_param(&pal_param, type);
        LOG(DEBUG) << "PAL handle is NULL " << __func__;
    }

    if(ret < 0) {
        LOG(ERROR) << __func__ << "Error while fetching value returned with ret: " << ret;
        return ret;
    } else {
        LOG(DEBUG) << __func__ << "Parameter fetched successfully! ret: " << Bassbootparams.value;
    }

    LOG(DEBUG) << "Exit " << __func__;

    return Bassbootparams.value;
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

int BassBoostContext::updatePalParameters(struct param_type2_t *params) {
    LOG(DEBUG) << "Enter " << __func__;

    pal_awx_param_t *pal_param = (pal_awx_param_t *)malloc(sizeof(pal_awx_param_t));

    if (pal_param == NULL) {
        LOG(ERROR) << __func__ << "Memory not assigned properly";
        return -1;
    }

    pal_param->param_id = PARAM_ID_BASS_MANAGER;
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

    if (pal_param) {
        free(pal_param);
    }
    LOG(DEBUG) << "Exit " << __func__;
    return 0;
}
} // namespace aidl::ampere::effects
