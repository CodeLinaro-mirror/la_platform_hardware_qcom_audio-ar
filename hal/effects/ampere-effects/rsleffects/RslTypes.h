 /*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once
#include <array>

#include <aidl/android/hardware/audio/effect/BnEffect.h>
#include "effect-impl/EffectTypes.h"
#include "effect-impl/EffectUUID.h"

using aidl::android::hardware::audio::effect::Descriptor;
using aidl::android::hardware::audio::effect::Capability;
using aidl::android::hardware::audio::effect::Flags;
using aidl::android::hardware::audio::effect::Range;
using aidl::ampere::hardware::audio::effect::Ambiance;
using aidl::ampere::hardware::audio::effect::Sdvc;
using aidl::ampere::hardware::audio::effect::SteadyVolume;

namespace aidl::ampere::effects {
enum class RslEffectType {
    AMBIANCE,
    SDVC,
    STEADY_VOLUME,
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
                        .implementor = "Ampere"}
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
                        .implementor = "Ampere"}
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
                        .implementor = "Ampere"}
};

#define PARAM_ID_AMBIANCE 0x11112550
#define PARAM_ID_SDVC 0x11112523
#define PARAM_ID_STEADY_VOLUME 0x11112522

} // namespace aidl::ampere::effects
