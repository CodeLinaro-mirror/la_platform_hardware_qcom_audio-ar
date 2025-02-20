/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL_Effect_BMT_Rsl"

#include <Utils.h>
#include <cstddef>

#include "RslContext.h"
#include "RslTypes.h"
#include "extensions/AudioExtension.h"

#define MIN_BMT_VALUE -9
#define MAX_BMT_VALUE  9

namespace aidl::ampere::effects {

using aidl::android::media::audio::common::AudioDeviceDescription;
using aidl::android::media::audio::common::AudioDeviceType;

BMTContext::BMTContext(const Parameter::Common& common,
                                   const RslEffectType& type, bool processData)
    : RslContext(common, type, processData) {
    LOG(DEBUG) << "Enter " <<__func__ << type << " ioHandle " << common.ioHandle;

    init(); // init default state

    mState = EffectState::INITIALIZED;
}

BMTContext::~BMTContext() {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();
    deInit();
}

void BMTContext::init() {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();
    memset(&mBMTParams, 0, sizeof(struct param_type2_t));
}

void BMTContext::deInit() {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();
    stop();
}

RetCode BMTContext::start() {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();

    std::lock_guard lg(mMutex);
    struct param_type2_t BMTParams;
    mState = EffectState::ACTIVE;
    updatePalParameters(EFFECT_BMT_PARAM_BASS, &mBMTLevel[0]) ;
    updatePalParameters(EFFECT_BMT_PARAM_MID, &mBMTLevel[1]) ;
    updatePalParameters(EFFECT_BMT_PARAM_TREBEL, &mBMTLevel[2]) ;

    return RetCode::SUCCESS;
}

RetCode BMTContext::stop() {
    std::lock_guard lg(mMutex);
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();

    struct param_type2_t BMTParams = {0};
    mState = EffectState::INITIALIZED;
    updatePalParameters(EFFECT_BMT_PARAM_BASS, &BMTParams) ;
    updatePalParameters(EFFECT_BMT_PARAM_MID, &BMTParams) ;
    updatePalParameters(EFFECT_BMT_PARAM_TREBEL, &BMTParams) ;

    return RetCode::SUCCESS;
}

RetCode BMTContext::setOutputDevice(
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

RetCode BMTContext::setParameter(uint32_t cmd, int32_t param_value) {
    LOG(DEBUG) << "Enter " << __func__ << " cmd: " << cmd << " value " << param_value;

    std::lock_guard lg(mMutex);

    mBMTParams.value = param_value;

    if (updatePalParameters(cmd, &mBMTParams) == 0) {
        return RetCode::SUCCESS;
    }

    LOG(DEBUG) << " Exit " << __func__;

    return RetCode::ERROR_NULL_POINTER;
}

RetCode BMTContext::getParameter(effect_param_t* param, uint32_t *size) {
    LOG(DEBUG) << "Enter " << __func__;
    std::lock_guard lg(mMutex);
    uint64_t cmd;
    memcpy(&cmd, param->data, param->psize);

    int32_t voffset = ((param->psize - 1) / sizeof(int32_t) + 1) * sizeof(int32_t);
    void *value = param->data + voffset;

    param->status = 0;
    param->vsize = sizeof(uint64_t);
    *size = sizeof(effect_param_t) + voffset + param->vsize;

    *(int32_t *)value = getValueFromPalParam(cmd);

    if (*(int32_t *)value < (MIN_BMT_VALUE - 1)) {
        return RetCode::ERROR_ILLEGAL_PARAMETER;
    }

    LOG(DEBUG) << " Exit " <<  __func__;

    return RetCode::SUCCESS;
}

RetCode BMTContext::setBMTBandLevels(
        const std::vector<Equalizer::BandLevel>& bandLevels) {
    std::lock_guard lg(mMutex);
    RETURN_VALUE_IF(bandLevels.size() > MAX_NUM_BANDS, RetCode::ERROR_ILLEGAL_PARAMETER,
                    "Exceeds Max Size");

    RETURN_VALUE_IF(bandLevels.empty(), RetCode::ERROR_ILLEGAL_PARAMETER, "Empty Bands");

    // Translation from existing implementation, first we update then send config to PAL.
    // ideally, send it to PAL and check if operation is successful then only update
    for (auto& bandLevel : bandLevels) {
        int level = bandLevel.levelMb;

        if ( level < MIN_BMT_VALUE || level > MAX_BMT_VALUE ) {
            LOG(DEBUG) << __func__ << "Error in setting value, not in range -9 to +9 ";
            return RetCode::ERROR_ILLEGAL_PARAMETER;
        }

        mBMTLevel[bandLevel.index].value = bandLevel.levelMb;

        LOG(VERBOSE) << __func__ << " level " << bandLevel.index << " level" << bandLevel.levelMb
                     << " refined level" << level;

        mBMTParams.value = level;
        if (updatePalParameters(bandLevel.index, &mBMTParams) != 0) {
            return RetCode::ERROR_ILLEGAL_PARAMETER;
        }
    }

    LOG(DEBUG) << " Exit " <<  __func__;

    return RetCode::SUCCESS;
}

std::vector<Equalizer::BandLevel> BMTContext::getBMTBandLevels() const {
    LOG(DEBUG) << "Enter " << __func__;

    std::vector<Equalizer::BandLevel> bandLevels;
    bandLevels.reserve(MAX_NUM_BANDS);

    for (std::size_t i = 0; i < MAX_NUM_BANDS; i++) {
        bandLevels.emplace_back(
                Equalizer::BandLevel{static_cast<int32_t>(i), getValueFromPalParam(i)});
    }

    LOG(DEBUG) << " Exit " <<  __func__;

    return bandLevels;
}

int BMTContext::getValueFromPalParam(uint32_t cmd) const {
    LOG(DEBUG) << "Enter " << __func__ << " cmd: " << cmd;

    int ret = -1;
    pal_awx_param_t pal_param;
    struct param_type2_t params;

    memset(&pal_param, 0, sizeof(pal_awx_param_t));
    memset(&params, 0, sizeof(param_type2_t));

    switch (cmd) {
        case EFFECT_BMT_PARAM_BASS:
            pal_param.param_id = PARAM_ID_BASS;
            break;
        case EFFECT_BMT_PARAM_MID:
            pal_param.param_id = PARAM_ID_MID;
            break;
        case EFFECT_BMT_PARAM_TREBEL:
            pal_param.param_id = PARAM_ID_TREBEL;
            break;
        default:
            LOG(ERROR) << __func__ << " Unsupported param ";
            break;
    }
    pal_param.param_size = sizeof(param_type2_t);
    pal_param.data = &params;

    // Defining CAPI param Type
    effect_type type = SYNC_WITHOUT_AUDIO_BUS;
    ret = ::qti::audio::core::AWX_get_param(&pal_param, type);

    if(ret < 0) {
        LOG(ERROR) << __func__ << "Error while fetching value returned with ret: " << ret;
        return (MIN_BMT_VALUE - 1);
    } else {
        LOG(DEBUG) << __func__ << "Parameter fetched successfully! ret: " << params.value;
    }

    LOG(DEBUG) << "Exit " << __func__;

    return params.value;
}

int BMTContext::updatePalParameters(uint32_t cmd, struct param_type2_t *params) {
    LOG(DEBUG) << "Enter " << __func__;

    pal_awx_param_t *pal_param = (pal_awx_param_t *)malloc(sizeof(pal_awx_param_t));

    if(pal_param == NULL) {
        LOG(ERROR) << __func__ << " Memory allocation failed ";
        return -1;
    }

    pal_param->param_size = sizeof(param_type2_t);

    switch (cmd) {
        case EFFECT_BMT_PARAM_BASS:
            pal_param->param_id = PARAM_ID_BASS;
            break;
        case EFFECT_BMT_PARAM_MID:
            pal_param->param_id = PARAM_ID_MID;
            break;
        case EFFECT_BMT_PARAM_TREBEL:
            pal_param->param_id = PARAM_ID_TREBEL;
            break;
        default:
            LOG(ERROR) << __func__ << " Unsupported param ";
            break;
    }
    pal_param->data = (void *)params;

    // Defining CAPI param Type
    effect_type type = SYNC_WITHOUT_AUDIO_BUS;
    ::qti::audio::core::AWX_set_param(pal_param, type);

    LOG(DEBUG) << "Exit " << __func__;

    return 0;
}
} // namespace aidl::ampere::effects
