 /*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL_Effect_Ambiance_Rsl"

#include <Utils.h>
#include <cstddef>

#include "RslContext.h"
#include "RslTypes.h"
#include "PalParamDelegator.h"
#include <aidl/ampere/hardware/audio/effect/Ambiance.h>
#include <system/audio_effects/audio_effects_utils.h>
#include "aidl/android/hardware/audio/effect/DefaultExtension.h"
#include <system/audio_effect.h>
#include "AudioConfig.h"

#define MAX_PROFILE_VALUE 3
#define MIN_PROFILE_VALUE 0
#define BIT0 0x1

namespace aidl::ampere::effects {

using aidl::android::media::audio::common::AudioDeviceDescription;
using aidl::android::media::audio::common::AudioDeviceType;
using aidl::ampere::hardware::audio::effect::Ambiance;
using namespace ::android::effect::utils;
using aidl::android::hardware::audio::effect::DefaultExtension;
using namespace ::aidl::qti::awx;


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
    ::qti::audio::oem::config::AudioConfigType req = ::qti::audio::oem::config::AUDIO_CONFIG_TONE_CONTROLLER_BANDS ;
    ::qti::audio::oem::config::AudioConfigData ambianceConfig;
    ::qti::audio::oem::config::AudioConfigManager::getInstance().getAudioConfigValue(req,&ambianceConfig);
    mCurrentProfile = ambianceConfig.defaultValue;
    LOG(DEBUG) << "Default Value of Ambiance  " << mCurrentProfile;
}

void AmbianceContext::deInit() {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();
    stop();
}

RetCode AmbianceContext::start(pal_stream_handle_t* palHandle) {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();

    std::lock_guard lg(mMutex);
    mPalHandle = palHandle;

    if (isEffectActive()) {
        setAmbianceProfile(mCurrentProfile);
    } else {
        LOG(DEBUG) << __func__ << " Effect is not yet active " ;
    }

    return RetCode::SUCCESS;
}

RetCode AmbianceContext::stop() {
    std::lock_guard lg(mMutex);
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();

    struct AmbianceParams ambianceParam = {0}; // by default enable bit is 0

    // Deactivate the Ambiance
    if (updatePalParameters(&ambianceParam) == 0) {
        mAsyncTransationStatus = 0;
    }
    mPalHandle = NULL;
    return RetCode::SUCCESS;
}
RetCode AmbianceContext::enable() {
    std::lock_guard lg(mMutex);
    LOG(DEBUG) << __func__ << " ioHandle " << getIoHandle();

    // Effect cannot be active before disabling
    if (isEffectActive())
        return RetCode::ERROR_ILLEGAL_PARAMETER;
    mState = EffectState::ACTIVE;

    setAmbianceProfile(mCurrentProfile);

    return RetCode::SUCCESS;
}

RetCode AmbianceContext::disable() {
    std::lock_guard lg(mMutex);
    LOG(DEBUG) << __func__ << " ioHandle " << getIoHandle();
    if (!isEffectActive()) return RetCode::ERROR_ILLEGAL_PARAMETER;
    struct AmbianceParams ambianceParam = {0}; // by default enable bit is 0

    // Deactivate the Ambiance
    if (updatePalParameters(&ambianceParam) == 0) {
        mAsyncTransationStatus = 0;
    }

    mState = EffectState::INITIALIZED;
    return RetCode::SUCCESS;
}

RetCode AmbianceContext::setAmbianceProfile(int profile) {

    LOG(VERBOSE) << "Enter " << __func__ << " ioHandle " << getIoHandle();

    if ( profile < MIN_PROFILE_VALUE || profile > MAX_PROFILE_VALUE ) {
        LOG(DEBUG) << __func__ << " Error in setting value, not in range 0 to 3 ";
        return RetCode::ERROR_ILLEGAL_PARAMETER;
    }

    mAmbianceParams.value[0] = profile;
    mCurrentProfile = profile;

    if (updatePalParameters(&mAmbianceParams) == 0) {
        mAsyncTransationStatus = 0;
        LOG(VERBOSE) << "Exit " << __func__;
        return RetCode::SUCCESS;
    }

    mAsyncTransationStatus = static_cast<int32_t>(Ambiance::AsyncTransactionStatus::BUSY);;

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

    if (mPalHandle != NULL) {
        ret = PalParamDelegator::AWX_get_param_handle(mPalHandle, &pal_param, type);
    } else {
        ret = PalParamDelegator::AWX_get_param(&pal_param, type);
        LOG(DEBUG) << "PAL handle is NULL " << __func__;
    }

    if (ret < 0) {
        LOG(ERROR) << __func__ << "Error while fetching value returned with ret: " << ret;
        return ret;
    } else {
        LOG(DEBUG) << __func__ << "Parameter fetched successfully! Amabiance value:" << params.value[0] <<" status: "<< params.status ;
    }

    mAsyncTransationStatus =  params.status;
    LOG(DEBUG) << "Exit " << __func__;

    return params.value[0];
}
RetCode AmbianceContext::setParameter(const std::vector<uint8_t>& specific) {
    LOG(DEBUG) << __func__ << " Entry";
    std::lock_guard lg(mMutex);

    auto reader = EffectParamReader(*(effect_param_t*)specific.data());

    uint32_t type;
    if (::android::OK != reader.readFromParameter(&type)) {
        LOG(ERROR) << __func__ << " invalid param " << reader.toString().c_str();
        return RetCode::ERROR_ILLEGAL_PARAMETER;
    }

    if ( type < MIN_PROFILE_VALUE || type > MAX_PROFILE_VALUE ) {
        LOG(DEBUG) << __func__ << " Error in setting value, not in range 0 to 3 ";
        return RetCode::ERROR_ILLEGAL_PARAMETER;
    }

    Ambiance::Params paramId = static_cast<Ambiance::Params>(type);
    size_t psize = sizeof(paramId);
    size_t valueSize = reader.getValueSize();
    size_t paramSize = reader.getParameterSize();

    if (paramId == Ambiance::Params::PARAM_CURRENT_PROFILE ||
        paramId == Ambiance::Params::PARAM_CURRENT_PROFILE_ASYNC) {

        LOG(DEBUG) << __func__ << " PARAM_CURRENT_PROFILE_ASYNC";

        if (valueSize != sizeof(mCurrentProfile) || paramSize != psize) {
            LOG(ERROR) << __func__ << " PARAM_CURRENT_PROFILE_ASYNC invalid size";
            return RetCode::ERROR_ILLEGAL_PARAMETER;
        }

        uint16_t newProfile;
        if (reader.readFromValue(&newProfile) != ::android::OK) {
            LOG(ERROR) << __func__ << " PARAM_CURRENT_PROFILE_ASYNC invalid size";
            return RetCode::ERROR_ILLEGAL_PARAMETER;
        }

        mCurrentProfile = newProfile;

        auto ret = setAmbianceProfile(mCurrentProfile);

        // Reset  the ASYNC transaction status if Effect is not initialized and ret failure
        if ((ret != RetCode::SUCCESS) && (mState == EffectState::INITIALIZED))
        {
            mAsyncTransationStatus = 0;
        }
        LOG(DEBUG) << __func__ << " Set Ambiance Profile " << mCurrentProfile << " return " << static_cast<int32_t>(ret);
    } else {
        LOG(ERROR) << __func__ << " unknown parameter requested=" << static_cast<int>(paramId);
        return RetCode::ERROR_ILLEGAL_PARAMETER;
    }
    LOG(DEBUG) << __func__ << " Exit";
    return RetCode::SUCCESS;
}

std::vector<uint8_t> AmbianceContext::getParameter(std::vector<uint8_t> id) {
    std::lock_guard lg(mMutex);
    auto paramReader = EffectParamReader(*(effect_param_t*)id.data());
    auto paramWriter = EffectParamWriter(*(effect_param_t*)id.data());

    uint32_t paramType;
    if (::android::OK != paramReader.readFromParameter(&paramType)) {
        ALOGE("%s invalid param %s", __func__, paramReader.toString().c_str());
        LOG(ERROR) << __func__ << " invalid param " << paramReader.toString().c_str();
        return {};
    }

    Ambiance::Params paramId = static_cast<Ambiance::Params>(paramType);
    size_t paramSize = sizeof(paramId);

    size_t valueSize;

    switch (paramId) {
        case Ambiance::Params::PARAM_CURRENT_PROFILE_ASYNC:
        case Ambiance::Params::PARAM_CURRENT_PROFILE: {
            LOG(DEBUG) << __func__ << " PARAM_CURRENT_PROFILE_ASYNC/PARAM_CURRENT_PROFILE Param ID" << static_cast<int32_t>(paramId);

            auto ret = getAmbianceProfile();
            bool profileFailed = (ret < 0);

            if (!profileFailed) {
                mCurrentProfile = ret;
            }
            valueSize = sizeof(mCurrentProfile) + sizeof(mAsyncTransationStatus);
            if (paramWriter.getValueSize() != valueSize || paramReader.getParameterSize() != paramSize) {
                LOG(ERROR) << __func__ << " PARAM_CURRENT_PROFILE_ASYNC/PARAM_CURRENT_PROFILE invalid size";
                paramWriter.setStatus(::android::BAD_VALUE);
                break;
            }
            paramWriter.writeToValue(&mCurrentProfile);
            paramWriter.writeToValue(&mAsyncTransationStatus);
            paramWriter.setStatus(::android::OK);
            break;
        }
        case Ambiance::Params::PARAM_GET_NUM_OF_PROFILES: {
            LOG(ERROR) << __func__ << " AMBIANCE_PARAM_GET_NUM_OF_PROFILES";
            valueSize = sizeof(mNumProfiles);
            if (paramWriter.getValueSize() != valueSize || paramReader.getParameterSize() != paramSize) {
                LOG(ERROR) << __func__ << " AMBIANCE_PARAM_GET_NUM_OF_PROFILES invalid size";
                paramWriter.setStatus(::android::BAD_VALUE);
                break;
            }
            paramWriter.writeToValue(&mNumProfiles);
            paramWriter.setStatus(::android::OK);
            break;
        }
        default:
            LOG(ERROR) << __func__ << " unknown parameter requested=" << static_cast<int>(paramId);
            paramWriter.setStatus(::android::BAD_VALUE);
            break;
    }

    DefaultExtension responseExtension;
    size_t totalSize = paramWriter.getTotalSize();
    responseExtension.bytes.resize(totalSize);
    std::memcpy(responseExtension.bytes.data(), (void*)&paramWriter.getEffectParam(), totalSize);
    return responseExtension.bytes;
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

    //PalParamDelegator::AWX_set_param(pal_param, type);
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
