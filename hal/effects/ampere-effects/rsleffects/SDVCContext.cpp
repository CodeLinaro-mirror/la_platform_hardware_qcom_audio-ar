/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL_Effect_SDVC_Rsl"

#include <Utils.h>
#include <cstddef>

#include "RslContext.h"
#include "RslTypes.h"
#include "PalParamDelegator.h"
#include <aidl/ampere/hardware/audio/effect/Ambiance.h>
#include <system/audio_effects/audio_effects_utils.h>
#include "aidl/android/hardware/audio/effect/DefaultExtension.h"
#include <system/audio_effect.h>

#define MAX_SDVC_PROFILE_VALUE 5
#define MIN_SDVC_PROFILE_VALUE 0

namespace aidl::ampere::effects {

using aidl::android::media::audio::common::AudioDeviceDescription;
using aidl::android::media::audio::common::AudioDeviceType;
using aidl::ampere::hardware::audio::effect::Sdvc;
using namespace ::android::effect::utils;
using aidl::android::hardware::audio::effect::DefaultExtension;
using namespace ::aidl::qti::awx;

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

RetCode SDVCContext::start(pal_stream_handle_t* palHandle) {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();

    std::lock_guard lg(mMutex);
    mPalHandle = palHandle;

    if (isEffectActive()) {
        setSdvcCurrentProfile(mCurrentProfile);
    } else {
        LOG(DEBUG) << "Not yet enabled";
    }

    return RetCode::SUCCESS;
}

RetCode SDVCContext::stop() {
    std::lock_guard lg(mMutex);
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();
    struct param_type2_t sdvcParam = {0}; // by default enable bit is 0
    setSdvcCurrentProfile(DEFAULT_SDVC_PROFILE);
    mPalHandle = nullptr;
    return RetCode::SUCCESS;
}

RetCode SDVCContext::enable() {
    std::lock_guard lg(mMutex);
    LOG(DEBUG) << __func__ << " ioHandle " << getIoHandle();
    if (isEffectActive())
     return RetCode::ERROR_ILLEGAL_PARAMETER;
    mState = EffectState::ACTIVE;
    setSdvcCurrentProfile(mCurrentProfile);
    return RetCode::SUCCESS;
}

RetCode SDVCContext::disable() {
    std::lock_guard lg(mMutex);
    LOG(DEBUG) << __func__ << " ioHandle " << getIoHandle();
    if (!isEffectActive()) return RetCode::ERROR_ILLEGAL_PARAMETER;
    mState = EffectState::INITIALIZED;
    setSdvcCurrentProfile(DEFAULT_SDVC_PROFILE);
    return RetCode::SUCCESS;
}


RetCode SDVCContext::setSdvcCurrentProfile(int profile) {

    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle() << " New Profile " << profile;

    mSdvcParams.value = profile;

    if (updatePalParameters(&mSdvcParams) == 0) {
        return RetCode::SUCCESS;
    }

    return RetCode::ERROR_NULL_POINTER;
}

RetCode SDVCContext::setParameter(const std::vector<uint8_t>& specific) {
    std::lock_guard lg(mMutex);
    LOG(ERROR) << __func__ << " SDVC";
    auto reader = EffectParamReader(*(effect_param_t*)specific.data());

    uint32_t type;
    if (::android::OK != reader.readFromParameter(&type)) {
        ALOGE("%s invalid param %s", __func__, reader.toString().c_str());
        LOG(ERROR) << __func__ << " invalid param " << reader.toString().c_str();
        return {};
    }

    Sdvc::Params paramId = static_cast<Sdvc::Params>(type);
    size_t psize = sizeof(paramId);
    size_t vsize = sizeof(mCurrentProfile);

    if (paramId == Sdvc::Params::PARAM_CURRENT_PROFILE) {
        LOG(DEBUG) << __func__ << " PARAM_CURRENT_PROFILE";

        if (reader.getValueSize() != vsize || reader.getParameterSize() != psize) {
            LOG(ERROR) << __func__ << " PARAM_CURRENT_PROFILE invalid size";
            return RetCode::ERROR_ILLEGAL_PARAMETER;
        }

        uint16_t newProfile;
        if (reader.readFromValue(&newProfile) != ::android::OK) {
            return RetCode::ERROR_ILLEGAL_PARAMETER;
        }

        if ( newProfile < MIN_SDVC_PROFILE_VALUE || newProfile > MAX_SDVC_PROFILE_VALUE ) {
            LOG(DEBUG) << __func__ << "Error in setting value, not in range 0 to 5 Value recieved " << newProfile;
            return RetCode::ERROR_ILLEGAL_PARAMETER;
        }

        mCurrentProfile = newProfile;

        auto ret = setSdvcCurrentProfile(mCurrentProfile);
        if (ret != RetCode::SUCCESS) {
            LOG(ERROR) << __func__ << " PARAM_CURRENT_PROFILE failed";
        }
    } else if (paramId == Sdvc::Params::PARAM_GET_NUM_OF_PROFILES) {
        LOG(DEBUG) << __func__ << " PARAM_GET_NUM_OF_PROFILES Set Not Expected Do Nothing ";
        return RetCode::SUCCESS;
    } else {
        LOG(ERROR) << __func__ << " unknown parameter requested=" << static_cast<int>(paramId);
        return RetCode::ERROR_ILLEGAL_PARAMETER;
    }

    return RetCode::SUCCESS;
}

std::vector<uint8_t> SDVCContext::getParameter(std::vector<uint8_t> identifier) {
    std::lock_guard lg(mMutex);
    LOG(DEBUG) << __func__ << " Entry";

    auto paramReader = EffectParamReader(*(effect_param_t*)identifier.data());
    auto paramWriter = EffectParamWriter(*(effect_param_t*)identifier.data());

    uint32_t paramType;
    if (::android::OK != paramReader.readFromParameter(&paramType)) {
        LOG(ERROR) << __func__ << " invalid parameter " << paramReader.toString().c_str();
        return {};
    }

    Sdvc::Params parameterId = static_cast<Sdvc::Params>(paramType);
    size_t paramSize = sizeof(parameterId);
    size_t valueSize;

    LOG(DEBUG) << __func__ << " param ID " << static_cast<int>(parameterId);

    switch (parameterId) {
        case Sdvc::Params::PARAM_CURRENT_PROFILE: {
            LOG(DEBUG) << __func__ << " SDVC_PARAM_CURRENT_PROFILE";

            int ret = getSdvcCurrentProfile();
            if (ret < 0) {
                LOG(ERROR) << __func__ << " SDVC_PARAM_CURRENT_PROFILE failed";
            } else {
                mCurrentProfile = ret;
            }

            valueSize = sizeof(mCurrentProfile);
            if (paramWriter.getValueSize() != valueSize || paramReader.getParameterSize() != paramSize) {
                LOG(ERROR) << __func__ << " SDVC_PARAM_CURRENT_PROFILE invalid size";
                paramWriter.setStatus(::android::BAD_VALUE);
                break;
            }

            paramWriter.writeToValue(&mCurrentProfile);
            paramWriter.setStatus(::android::OK);
            break;
        }
        case Sdvc::Params::PARAM_GET_NUM_OF_PROFILES: {
            LOG(DEBUG) << __func__ << " SDVC_PARAM_GET_NUM_OF_PROFILES";

            valueSize = sizeof(mNumProfiles);
            if (paramWriter.getValueSize() != valueSize || paramReader.getParameterSize() != paramSize) {
                LOG(ERROR) << __func__ << " SDVC_PARAM_GET_NUM_OF_PROFILES invalid size";
                paramWriter.setStatus(::android::BAD_VALUE);
                break;
            }

            paramWriter.writeToValue(&mNumProfiles);
            paramWriter.setStatus(::android::OK);
            break;
        }
        default:
            LOG(ERROR) << __func__ << " unknown parameter requested=" << static_cast<int>(parameterId);
            paramWriter.setStatus(::android::BAD_VALUE);
            break;
    }

    DefaultExtension responseExtension;
    size_t totalSize = paramWriter.getTotalSize();
    responseExtension.bytes.resize(totalSize);
    std::memcpy(responseExtension.bytes.data(), (void*)&paramWriter.getEffectParam(), totalSize);

    LOG(DEBUG) << __func__ << " Exit";
    return responseExtension.bytes;
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
    ret = PalParamDelegator::AWX_get_param_handle(mPalHandle,&pal_param, type);

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
