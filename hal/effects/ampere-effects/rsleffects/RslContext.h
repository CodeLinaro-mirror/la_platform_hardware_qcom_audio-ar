 /*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
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
#include <android/binder_manager.h>
#include <binder/IServiceManager.h>
#include <aidl/android/hardware/audio/core/IModule.h>
#include <aidl/qti/audio/core/VString.h>
using aidl::android::hardware::audio::core::IModule;
using aidl::android::hardware::audio::core::VendorParameter;
using aidl::qti::audio::core::VString;

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
    virtual RetCode start(pal_stream_handle_t* palHandle) = 0;
    virtual RetCode stop() = 0;
    virtual RetCode enable() = 0;
    virtual RetCode disable() = 0;

    virtual RetCode setParameter(const std::vector<uint8_t>& params) { return RetCode::SUCCESS; }
    virtual std::vector<uint8_t> getParameter(std::vector<uint8_t> id)  { return id; }

    // BMT methods, implement in BMTContext
    virtual int getValueFromPalParam(uint32_t cmd) const { return 0; }
    virtual int updatePalParameters(uint32_t cmd, struct param_type2_t *param) { return 0; }
    virtual RetCode setBMTBandLevels(const std::vector<Equalizer::BandLevel>& bandLevels) {
        return RetCode::ERROR_ILLEGAL_PARAMETER;
    }
    virtual std::vector<Equalizer::BandLevel> getBMTBandLevels() const { return {}; }

    virtual RetCode setEqualizerPreset(const std::size_t presetIdx)
    {
      return RetCode::ERROR_ILLEGAL_PARAMETER;
    }
    virtual int getEqualizerPreset() const
    {
      return 0;
    }
    virtual std::vector<Equalizer::Preset> getPresets()
    {
      return {};
    }

    // BassBoost methods, implement in BassBoostContext
    virtual RetCode setBassBoost(int bass) { return RetCode::ERROR_ILLEGAL_PARAMETER; }
    virtual int getBassBoost() { return 0; }
    virtual int getBassBoostStrength() { return 0; }
    virtual RetCode setBassBoostStrength(int strength) { return RetCode::ERROR_ILLEGAL_PARAMETER; }
    virtual int updatePalParameters(struct param_type2_t *param) { return 0; }

    virtual bool deviceSupportsEffect(const std::vector<AudioDeviceDescription>& device) {
        return true;
    }

    protected:
    std::mutex mMutex;
    const RslEffectType mType;
    EffectState mState = EffectState::UNINITIALIZED;
    bool isEffectActive() { return mState == EffectState::ACTIVE; }
    pal_stream_handle_t* mPalHandle = NULL;
};

class AmbianceContext final : public RslContext {
  public:
    AmbianceContext(const Parameter::Common& common, const RslEffectType& type,
                     bool processData);
    ~AmbianceContext() override;
    virtual void deInit() override;
    virtual void init() override;
    virtual RetCode start(pal_stream_handle_t* palHandle) override;
    virtual RetCode stop() override;
    virtual RetCode enable() override;
    virtual RetCode disable() override;

    RetCode setOutputDevice(const std::vector<AudioDeviceDescription>& device) override;
    RetCode setAmbianceProfile(int profile) ;
    int getAmbianceProfile() ;
    int updatePalParameters(struct AmbianceParams *param);
    virtual RetCode setParameter(const std::vector<uint8_t>& params)  override ;
    virtual std::vector<uint8_t> getParameter(std::vector<uint8_t> id)  override;

  private:
    struct AmbianceParams mAmbianceParams;
    bool mTempDisabled = false;
    uint16_t mNumProfiles = MAX_AMBIANCE_PROFILE;
    uint16_t mCurrentProfile = DEFAULT_AMBIANCE_PROFILE;
    int32_t mAsyncTransationStatus = 0 ;
};

class SDVCContext final : public RslContext {
  public:
    SDVCContext(const Parameter::Common& common, const RslEffectType& type,
                     bool processData);
    ~SDVCContext() override;
    virtual void deInit() override;
    virtual void init() override;
    virtual RetCode start(pal_stream_handle_t* palHandle) override;
    virtual RetCode stop() override;
    virtual RetCode enable() override;
    virtual RetCode disable() override;

    RetCode setOutputDevice(const std::vector<AudioDeviceDescription>& device) override;
    RetCode setSdvcCurrentProfile(int profile) ;
    int getSdvcCurrentProfile() ;
    int updatePalParameters(struct param_type2_t *param);

    virtual RetCode setParameter(const std::vector<uint8_t>& params)  override ;
    virtual std::vector<uint8_t> getParameter(std::vector<uint8_t> id)  override;

  private:
    struct param_type2_t mSdvcParams;
    bool mTempDisabled = false;
    uint16_t mNumProfiles = MAX_SDVC_PROFILE;
    uint16_t mCurrentProfile = DEFAULT_SDVC_PROFILE;
    std::shared_ptr<IModule> mModule;
    int getDeviceInstance() ;
};

class SteadyVolumeContext final : public RslContext {
  public:
    SteadyVolumeContext(const Parameter::Common& common, const RslEffectType& type,
                     bool processData);
    ~SteadyVolumeContext() override;
    virtual void deInit() override;
    virtual void init() override;
    virtual RetCode start(pal_stream_handle_t* palHandle) override;
    virtual RetCode stop() override;
    virtual RetCode enable() override;
    virtual RetCode disable() override;

    RetCode setOutputDevice(const std::vector<AudioDeviceDescription>& device) override;
    RetCode setSteadyVolume(int profile) ;
    int getSteadyVolume() ;
    int updatePalParameters(struct param_type2_t *param);
    virtual RetCode setParameter(const std::vector<uint8_t>& params)  override ;
    virtual std::vector<uint8_t> getParameter(std::vector<uint8_t> id)  override;

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
    virtual RetCode start(pal_stream_handle_t* palHandle) override;
    virtual RetCode stop() override;
    virtual RetCode enable() override;
    virtual RetCode disable() override;

    RetCode setOutputDevice(const std::vector<AudioDeviceDescription>& device) override;
    int getValueFromPalParam(uint32_t cmd) const override;
    RetCode setBMTBandLevels(const std::vector<Equalizer::BandLevel>& bandLevels) override;
    std::vector<Equalizer::BandLevel> getBMTBandLevels() const override;
    int updatePalParameters(uint32_t cmd, struct param_type2_t *param);

    RetCode setEqualizerPreset(const std::size_t presetIdx) override;
    std::vector<Equalizer::Preset> getPresets() override;
    int getEqualizerPreset() const override ;

  private:
    struct param_type2_t mBMTParams;
    bool mTempDisabled = false;
    int mMaxBandLevel;
    struct param_type2_t mBMTLevel[MAX_NUM_BANDS];
    int mPresetIndex = 0;
  };

class BassBoostContext final : public RslContext {
  public:
    BassBoostContext(const Parameter::Common& common, const RslEffectType& type,
                     bool processData);
    ~BassBoostContext() override;
    virtual void deInit() override;
    virtual void init() override;
    virtual RetCode start(pal_stream_handle_t* palHandle) override;
    virtual RetCode stop() override;
    virtual RetCode enable() override;
    virtual RetCode disable() override;
    RetCode setOutputDevice(const std::vector<AudioDeviceDescription>& device) override;
    RetCode setBassBoost(int bass) override;
    int getBassBoost() override;
    int getBassBoostStrength() override;
    RetCode setBassBoostStrength(int strength) override;
    int updatePalParameters(struct param_type2_t *param);
    int setOffloadParameters() ;

  private:
    struct param_type2_t mBassBoostSyncParams;
    bool mTempDisabled = false;
};

} // namespace aidl::ampere::effects
