 /*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once
#include <array>

#include <aidl/android/hardware/audio/effect/BnEffect.h>
#include "effect-impl/EffectTypes.h"
#include "effect-impl/EffectUUID.h"
#include <aidl/ampere/hardware/audio/effect/Ambiance.h>
#include <aidl/ampere/hardware/audio/effect/Sdvc.h>
#include <aidl/ampere/hardware/audio/effect/SteadyVolume.h>

using aidl::android::hardware::audio::effect::Descriptor;
using aidl::android::hardware::audio::effect::Capability;
using aidl::android::hardware::audio::effect::Flags;
using aidl::android::hardware::audio::effect::Range;
using aidl::ampere::hardware::audio::effect::Ambiance;
using aidl::ampere::hardware::audio::effect::Sdvc;
using aidl::ampere::hardware::audio::effect::SteadyVolume;
using aidl::android::hardware::audio::effect::BassBoost;
using aidl::android::hardware::audio::effect::Equalizer;

namespace aidl::ampere::effects {
enum class RslEffectType {
    AMBIANCE,
    SDVC,
    STEADY_VOLUME,
    BMT,
    BASS_BOOST,
    NONE
};


inline std::ostream& operator<<(std::ostream& out, const RslEffectType& type) {
    out << " Type ";
    switch (type) {
        case RslEffectType::AMBIANCE:
            return out << "AMBIANCE";
        case RslEffectType::SDVC:
            return out << "SDVC";
        case RslEffectType::STEADY_VOLUME:
            return out << "STEADY_VOLUME";
        case RslEffectType::BMT:
            return out << "BASS_MID_TREBEL";
        case RslEffectType::BASS_BOOST:
            return out << "BASS_BOOST";
    }
    return out << "EnumRslEffectTypeError";
}

static const std::string kAmbianceEffectName = "AMBIANCE";
static const Descriptor kAmbianceDescriptor = {
                .common = {.id = {.type = qti::effects::kAmbianceTypeUUID,
                        .uuid = qti::effects::kAmbianceUUID,
                        .proxy = std::nullopt},
                        .flags = {.type = Flags::Type::POST_PROC,
                          .hwAcceleratorMode = Flags::HardwareAccelerator::TUNNEL,
                          .deviceIndication = true },
                        .name = kAmbianceEffectName,
                        .implementor = "Qualcomm Technologies Inc."}
};

struct AmbianceParams {
    uint16_t eq_mask;
    uint16_t status;
    uint32_t value[16];
};

static const std::string kSdvcEffectName = "SDVC";
static const Descriptor kSdvcDescriptor = {
                .common = {.id = {.type = qti::effects::kSdvcTypeUUID,
                        .uuid = qti::effects::kSdvcUUID,
                        .proxy = std::nullopt},
                        .flags = {.type = Flags::Type::POST_PROC,
                          .hwAcceleratorMode = Flags::HardwareAccelerator::TUNNEL,
                          .deviceIndication = true },
                        .name = kSdvcEffectName,
                        .implementor = "Qualcomm Technologies Inc."}
};

struct param_type2_t {
    int32_t value;
};

static const std::string kSteadyVolumeEffectName = "STEADY_VOLUME";
static const Descriptor kSteadyVolumeDescriptor = {
                .common = {.id = {.type = qti::effects::kSteadyVolumeTypeUUID,
                        .uuid = qti::effects::kSteadyVolumeUUID,
                        .proxy = std::nullopt},
                        .flags = {.type = Flags::Type::POST_PROC,
                          .hwAcceleratorMode = Flags::HardwareAccelerator::TUNNEL,
                          .deviceIndication = true },
                        .name = kSteadyVolumeEffectName,
                        .implementor = "Qualcomm Technologies Inc."}
};
#define MIN_BMT_VALUE -9
#define MAX_BMT_VALUE  9
const std::vector<Equalizer::Preset> kPresets = {};
const std::vector<Range::EqualizerRange> kEqRanges = {
        MAKE_RANGE(Equalizer, preset, 0, 0),
        MAKE_RANGE(Equalizer, bandLevels,
                   std::vector<Equalizer::BandLevel>{
                           Equalizer::BandLevel({.index = 0, .levelMb = MIN_BMT_VALUE})},
                   std::vector<Equalizer::BandLevel>{
                           Equalizer::BandLevel({.index = /* max nb bands= */ 3 - 1,
                                                 .levelMb = MAX_BMT_VALUE})}),
        MAKE_RANGE(Equalizer, presets, kPresets, kPresets)};
static const Capability kEqCap = {.range = kEqRanges};

static const std::string kBMTEffectName = "BASS_MID_TREBEL";
static const Descriptor kBMTDescriptor = {
                .common = {.id = {.type = qti::effects::kEqualizerTypeUUID,
                        .uuid = qti::effects::kEqualizerBundleImplUUID,
                        .proxy = std::nullopt},
                        .flags = {.type = Flags::Type::POST_PROC,
                          .hwAcceleratorMode = Flags::HardwareAccelerator::TUNNEL,
                          .offloadIndication = true,
                          .deviceIndication = true },
                        .name = kBMTEffectName,
                        .implementor = "Qualcomm Technologies Inc."},
                        .capability = kEqCap
};

enum EffectBMTParams
{
    EFFECT_BMT_PARAM_BASS,
    EFFECT_BMT_PARAM_MID,
    EFFECT_BMT_PARAM_TREBEL,
};

const std::vector<Range::BassBoostRange> kBassBoostRanges = {
         MAKE_RANGE(BassBoost, strengthPm, 2, 0)};

const Capability kBassBoostCap = {.range = {kBassBoostRanges}};

static const std::string kBassBoostEffectName = "BASS_BOOST";
static const Descriptor kBassBoostDescriptor = {
                .common = {.id = {.type = qti::effects::kBassBoostTypeUUID,
                        .uuid = qti::effects::kBassBoostBundleImplUUID,
                        .proxy = std::nullopt},
                        .flags = {.type = Flags::Type::POST_PROC,
                          .hwAcceleratorMode = Flags::HardwareAccelerator::TUNNEL,
                          .offloadIndication = true,
                          .deviceIndication = true },
                        .name = kBassBoostEffectName,
                        .implementor = "Qualcomm Technologies Inc."},
                        .capability =  kBassBoostCap
};

#define PARAM_ID_AMBIANCE 0x11112550
#define PARAM_ID_SDVC 0x11112523
#define PARAM_ID_STEADY_VOLUME 0x11112522
#define PARAM_ID_BASS 0x11112527
#define PARAM_ID_MID 0x11112528
#define PARAM_ID_TREBEL 0x11112529
#define PARAM_ID_BASS_MANAGER 0x1111252A

#define MAX_SDVC_PROFILE  6
#define MAX_AMBIANCE_PROFILE 4
#define DEFAULT_SDVC_PROFILE 0
#define DEFAULT_AMBIANCE_PROFILE 2

constexpr inline size_t MAX_NUM_BANDS = 3;
constexpr inline size_t MAX_NUM_BANDS_8 = 8;

} // namespace aidl::ampere::effects
