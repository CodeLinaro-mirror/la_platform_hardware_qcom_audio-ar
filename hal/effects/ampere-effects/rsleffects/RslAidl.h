 /*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once
#include <functional>
#include <map>
#include <memory>

#include <aidl/android/hardware/audio/effect/BnEffect.h>
#include <android-base/logging.h>

#include "effect-impl/EffectImpl.h"
#include "effect-impl/EffectUUID.h"

#include "GlobalRslSession.h"
#include "RslContext.h"
#include "RslTypes.h"

using aidl::qti::effects::EffectImpl;
using aidl::qti::effects::AudioUuid;
using aidl::ampere::effects::kAmbianceDescriptor;

namespace aidl::ampere::effects {
    class RslAidl final : public EffectImpl {
        public:
            explicit RslAidl(const AudioUuid& uuid);
            ~RslAidl() override;

            ndk::ScopedAStatus getDescriptor(
                    aidl::android::hardware::audio::effect::Descriptor* _aidl_return) override;

            ndk::ScopedAStatus setParameterSpecific(
                   const aidl::android::hardware::audio::effect::Parameter::Specific& specific) override;
            ndk::ScopedAStatus getParameterSpecific(
                   const aidl::android::hardware::audio::effect::Parameter::Id& id,
                   aidl::android::hardware::audio::effect::Parameter::Specific* specific) override;

            ndk::ScopedAStatus setParameterCommon(const Parameter& param) override;

            std::shared_ptr<EffectContext> createContext(
                    const aidl::android::hardware::audio::effect::Parameter::Common& common,
                    bool processData) override;

            RetCode releaseContext() override;

            ndk::ScopedAStatus commandImpl(
                    aidl::android::hardware::audio::effect::CommandId command) override;

            std::string getEffectName() override { return *mEffectName; }

        private:
            void stopEffectIfNeeded(const Parameter::Common& common);
            std::shared_ptr<RslContext> mContext;
            RslEffectType mType = RslEffectType::AMBIANCE;
            aidl::android::hardware::audio::effect::IEffect::Status status(binder_status_t status,
                                                                   size_t consumed,
                                                                   size_t produced);

            ndk::ScopedAStatus setParameterVendorEffect(
            const aidl::android::hardware::audio::effect::Parameter::Specific& specific);
            ndk::ScopedAStatus getParameterVendorEffect(
            const aidl::android::hardware::audio::effect::Parameter::Id& id,
            aidl::android::hardware::audio::effect::Parameter::Specific* specific);
            ndk::ScopedAStatus setParameterBMT(
            const aidl::android::hardware::audio::effect::Parameter::Specific& specific);
            ndk::ScopedAStatus getParameterBMT(
            const aidl::android::hardware::audio::effect::Equalizer::Id& id,
            aidl::android::hardware::audio::effect::Parameter::Specific* specific);

    };
} // namespace aidl::ampere::effects
