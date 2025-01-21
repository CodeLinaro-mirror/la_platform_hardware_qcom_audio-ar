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

namespace aidl::ampere::effects {
enum class RslEffectType {
    AMBIANCE,
};

inline std::ostream& operator<<(std::ostream& out, const RslEffectType& type) {
    out << " Type ";
    switch (type) {
        case RslEffectType::AMBIANCE:
            return out << "AMBIANCE";
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

#define PARAM_ID_AMBIANCE 0x11112550

} // namespace aidl::ampere::effects
