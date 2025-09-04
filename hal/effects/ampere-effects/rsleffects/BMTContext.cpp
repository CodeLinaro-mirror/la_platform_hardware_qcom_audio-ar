/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL_Effect_BMT_Rsl"

#include <Utils.h>
#include <cstddef>

#include "RslContext.h"
#include "RslTypes.h"
#include "PalParamDelegator.h"
#include "AudioConfig.h"

namespace aidl::ampere::effects {

using aidl::android::media::audio::common::AudioDeviceDescription;
using aidl::android::media::audio::common::AudioDeviceType;
using namespace ::aidl::qti::awx;

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
    ::qti::audio::oem::config::AudioConfigType req = ::qti::audio::oem::config::AUDIO_CONFIG_TONE_CONTROLLER_BANDS ;
    ::qti::audio::oem::config::AudioConfigData ToneControllerConfig;

    ::qti::audio::oem::config::AudioConfigManager::getInstance().getAudioConfigValue(req,&ToneControllerConfig);

    LOG(DEBUG) << "Default Value of BMT Tone Controller Bands  " << ToneControllerConfig.defaultValue;

    if (ToneControllerConfig.defaultValue == MAX_TONE_CONTROLLER_BANDS)
    {
        mMaxBandLevel = MAX_NUM_BANDS_8;
    }
    else
    {
        mMaxBandLevel = MAX_NUM_BANDS;
    }

    memset(&mBMTParams, 0, sizeof(struct param_type2_t));
}

void BMTContext::deInit() {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();
    stop();
}
RetCode BMTContext::enable() {
    std::lock_guard lg(mMutex);
    LOG(DEBUG) << __func__ << " ioHandle " << getIoHandle();
    if (isEffectActive())
     return RetCode::ERROR_ILLEGAL_PARAMETER;
    mState = EffectState::ACTIVE;
    // Apply the cached value in case of Session Started Later
    updatePalParameters(EFFECT_BMT_PARAM_BASS, &mBMTLevel[0]) ;
    updatePalParameters(EFFECT_BMT_PARAM_MID, &mBMTLevel[1]) ;
    updatePalParameters(EFFECT_BMT_PARAM_TREBEL, &mBMTLevel[2]) ;
    return RetCode::SUCCESS;
}

RetCode BMTContext::disable() {
    std::lock_guard lg(mMutex);

    struct param_type2_t defaultBMTLevel[MAX_NUM_BANDS] = {0} ;

    LOG(DEBUG) << __func__ << " ioHandle " << getIoHandle();
    if (!isEffectActive()) return RetCode::ERROR_ILLEGAL_PARAMETER;
    mState = EffectState::INITIALIZED;
    // Apply the default value in case of Session Started Later
    updatePalParameters(EFFECT_BMT_PARAM_BASS, &defaultBMTLevel[0]) ;
    updatePalParameters(EFFECT_BMT_PARAM_MID, &defaultBMTLevel[1]) ;
    updatePalParameters(EFFECT_BMT_PARAM_TREBEL, &defaultBMTLevel[2]) ;
    return RetCode::SUCCESS;
}

RetCode BMTContext::start(pal_stream_handle_t* palHandle) {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << getIoHandle();
    mPalHandle = palHandle;
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
    mPalHandle = nullptr;;
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

RetCode BMTContext::setBMTBandLevels(
        const std::vector<Equalizer::BandLevel>& bandLevels) {
    LOG(VERBOSE) << "Enter " << __func__ << " ioHandle " << getIoHandle();
    std::lock_guard lg(mMutex);
    RETURN_VALUE_IF(bandLevels.size() > MAX_NUM_BANDS, RetCode::ERROR_ILLEGAL_PARAMETER,
                    "Exceeds Max Size");

    RETURN_VALUE_IF(bandLevels.empty(), RetCode::ERROR_ILLEGAL_PARAMETER, "Empty Bands");

    // Translation from existing implementation, first we update then send config to PAL.
    // ideally, send it to PAL and check if operation is successful then only update
    for (auto& bandLevel : bandLevels) {
        int level = bandLevel.levelMb;

        if ( level < MIN_BMT_VALUE || level > MAX_BMT_VALUE ) {
            LOG(DEBUG) << __func__ << "Error in setting value, not in range -9 to +9 " << level;
            return RetCode::SUCCESS;
        }

        mBMTLevel[bandLevel.index].value = bandLevel.levelMb;

        LOG(VERBOSE) << __func__ << " level " << bandLevel.index << " level" << bandLevel.levelMb
                     << " refined level" << level;

        mBMTParams.value = level;
        updatePalParameters(bandLevel.index, &mBMTParams) ;
    }

    LOG(VERBOSE) << " Exit " <<  __func__;

    return RetCode::SUCCESS;
}

std::vector<Equalizer::BandLevel> BMTContext::getBMTBandLevels() const {
    LOG(DEBUG) << "Enter " << __func__;

    std::vector<Equalizer::BandLevel> bandLevels;
    LOG(DEBUG) << "BandLevel " << mMaxBandLevel;
    bandLevels.reserve(mMaxBandLevel);

    for (std::size_t i = 0; i < mMaxBandLevel; i++) {
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
            return -EINVAL;
    }
    pal_param.param_size = sizeof(param_type2_t);
    pal_param.data = &params;

    // Defining CAPI param Type
    effect_type type = SYNC_WITHOUT_AUDIO_BUS;

    if (mPalHandle != NULL){
        ret = PalParamDelegator::AWX_get_param_handle(mPalHandle,&pal_param, type);
    } else {
        ret = PalParamDelegator::AWX_get_param(&pal_param, type);
        LOG(DEBUG) << "PAL handle is NULL " << __func__;
    }

    if(ret < 0) {
        LOG(ERROR) << __func__ << "Error while fetching value returned with ret: " << ret << "return Cached Value " << mBMTLevel[cmd].value;
        return mBMTLevel[cmd].value;
    } else {
        LOG(DEBUG) << __func__ << "Parameter fetched successfully! ret: " << params.value << "For Effect " <<std::to_string(cmd);
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

    if (mPalHandle != NULL){
        PalParamDelegator::AWX_set_param_handle(mPalHandle,pal_param, type);
    } else {
        PalParamDelegator::AWX_set_param(pal_param, type);
        LOG(DEBUG) << "PAL handle is NULL " << __func__;
    }
    LOG(DEBUG) << "Exit " << __func__;

    return 0;
}

int BMTContext::getEqualizerPreset() const
{
    LOG(WARNING) << __func__ << " Unsupported param ";
    return mPresetIndex;
}

RetCode BMTContext::setEqualizerPreset(const std::size_t presetIdx)
{
    LOG(WARNING) << __func__ << " Unsupported param by AWX module";
    mPresetIndex = presetIdx;
    return RetCode::SUCCESS;
}

std::vector<Equalizer::Preset> BMTContext::getPresets()
{
    std::vector<Equalizer::Preset> kPresets = {};
    LOG(WARNING) << __func__ << " Unsupported param by AWX module";
    return kPresets;
}


} // namespace aidl::ampere::effects
