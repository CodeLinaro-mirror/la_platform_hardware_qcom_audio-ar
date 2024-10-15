 /*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once

#include <android-base/logging.h>
#include <android-base/thread_annotations.h>
#include <array>
#include <cstddef>
#include <errno.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "PalApi.h"
#include "kvh2xml.h"

#include "RslTypes.h"
#include "PalDefs.h"
#include "effect-impl/EffectContext.h"
#include "effect-impl/EffectUUID.h"
#include <hardware/audio_effect.h>

using aidl::android::media::audio::common::AudioDeviceDescription;
using aidl::qti::effects::EffectContext;
using aidl::qti::effects::RetCode;
using aidl::android::hardware::audio::effect::Equalizer;
enum class EffectState {
    UNINITIALIZED,
    INITIALIZED,
    ACTIVE,
};

namespace aidl::ampere::effects {
class RslContext : public EffectContext {
  public:
    RslContext(const Parameter::Common& common, const RslEffectType& type,
                         bool processData)
        : EffectContext(common, processData), mType(type) {
    }

    virtual ~RslContext() override {
    }

    // Generic APIS
    RslEffectType getEffectType() const { return mType; }

    // Each effect context needs to implement these methods
    virtual void deInit() = 0;
    virtual void init() = 0;
    virtual RetCode start() = 0;
    virtual RetCode stop() = 0;
    virtual RetCode setParameter(uint32_t cmd, int32_t param_value) { return RetCode::SUCCESS; }
    virtual RetCode getParameter(effect_param_t *param, uint32_t *size) { return RetCode::SUCCESS; }

    // Ambiance methods, implement in AmbianceContext
    virtual RetCode setAmbianceProfile(int profile) { return RetCode::ERROR_ILLEGAL_PARAMETER; }
    virtual int getAmbianceProfile() { return 0; }
    virtual int updatePalParameters(struct AmbianceParams *param) { return 0; }

    // Sdvc methods, implement in SdvcContext
    virtual RetCode setSdvcCurrentProfile(int profile) { return RetCode::ERROR_ILLEGAL_PARAMETER; }
    virtual int getSdvcCurrentProfile() { return 0; }

    // SteadyVolume methods, implement in SteadyVolumeContext
    virtual RetCode setSteadyVolume(int value) { return RetCode::ERROR_ILLEGAL_PARAMETER; }
    virtual int getSteadyVolume() { return 0; }

    // BMT methods, implement in BMTContext
    virtual int getValueFromPalParam(uint32_t cmd) const { return 0; }
    virtual int updatePalParameters(uint32_t cmd, struct param_type2_t *param) { return 0; }
    virtual RetCode setBMTBandLevels(const std::vector<Equalizer::BandLevel>& bandLevels) {
        return RetCode::ERROR_ILLEGAL_PARAMETER;
    }
    virtual std::vector<Equalizer::BandLevel> getBMTBandLevels() const { return {}; }

    virtual int updatePalParameters(struct param_type2_t *param) { return 0; }

    virtual bool deviceSupportsEffect(const std::vector<AudioDeviceDescription>& device) {
        return true;
    }

    protected:
    std::mutex mMutex;
    const RslEffectType mType;
    EffectState mState = EffectState::UNINITIALIZED;
    bool isEffectActive() { return mState == EffectState::ACTIVE; }
};

class AmbianceContext final : public RslContext {
  public:
    AmbianceContext(const Parameter::Common& common, const RslEffectType& type,
                     bool processData);
    ~AmbianceContext() override;
    virtual void deInit() override;
    virtual void init() override;
    virtual RetCode start() override;
    virtual RetCode stop() override;
    RetCode setParameter(uint32_t cmd, int32_t param_value) override;
    RetCode setOutputDevice(const std::vector<AudioDeviceDescription>& device) override;
    RetCode setAmbianceProfile(int profile) override;
    int getAmbianceProfile() override;
    RetCode getParameter(effect_param_t *param, uint32_t *size) override;
    int updatePalParameters(struct AmbianceParams *param);

  private:
    struct AmbianceParams mAmbianceParams;
    bool mTempDisabled = false;
};

class SDVCContext final : public RslContext {
  public:
    SDVCContext(const Parameter::Common& common, const RslEffectType& type,
                     bool processData);
    ~SDVCContext() override;
    virtual void deInit() override;
    virtual void init() override;
    virtual RetCode start() override;
    virtual RetCode stop() override;
    RetCode setParameter(uint32_t cmd, int32_t param_value) override;
    RetCode setOutputDevice(const std::vector<AudioDeviceDescription>& device) override;
    RetCode setSdvcCurrentProfile(int profile) override;
    int getSdvcCurrentProfile() override;
    int updatePalParameters(struct param_type2_t *param);
    RetCode getParameter(effect_param_t *param, uint32_t *size) override;

  private:
    struct param_type2_t mSdvcParams;
    bool mTempDisabled = false;
};

class SteadyVolumeContext final : public RslContext {
  public:
    SteadyVolumeContext(const Parameter::Common& common, const RslEffectType& type,
                     bool processData);
    ~SteadyVolumeContext() override;
    virtual void deInit() override;
    virtual void init() override;
    virtual RetCode start() override;
    virtual RetCode stop() override;
    RetCode setParameter(uint32_t cmd, int32_t param_value) override;
    RetCode setOutputDevice(const std::vector<AudioDeviceDescription>& device) override;
    RetCode setSteadyVolume(int profile) override;
    int getSteadyVolume() override;
    int updatePalParameters(struct param_type2_t *param);
    RetCode getParameter(effect_param_t *param, uint32_t *size) override;

  private:
    struct param_type2_t mSteadyVolumeParams;
    bool mTempDisabled = false;
};

class BMTContext final : public RslContext {
  public:
    BMTContext(const Parameter::Common& common, const RslEffectType& type,
                     bool processData);
    ~BMTContext() override;
    virtual void deInit() override;
    virtual void init() override;
    virtual RetCode start() override;
    virtual RetCode stop() override;
    RetCode setParameter(uint32_t cmd, int32_t param_value) override;
    RetCode setOutputDevice(const std::vector<AudioDeviceDescription>& device) override;
    int getValueFromPalParam(uint32_t cmd) const override;
    RetCode setBMTBandLevels(const std::vector<Equalizer::BandLevel>& bandLevels) override;
    std::vector<Equalizer::BandLevel> getBMTBandLevels() const override;
    int updatePalParameters(uint32_t cmd, struct param_type2_t *param);
    RetCode getParameter(effect_param_t *param, uint32_t *size) override;

  private:
    struct param_type2_t mBMTParams;
    bool mTempDisabled = false;
};

} // namespace aidl::ampere::effects
