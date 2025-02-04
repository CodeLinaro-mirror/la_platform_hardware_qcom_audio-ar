/*
 * Copyright (c) 2024 - 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once

using aidl::android::media::audio::common::AudioDevice;
using aidl::android::media::audio::common::AudioDeviceAddress;
using aidl::android::media::audio::common::AudioDeviceDescription;
using aidl::android::media::audio::common::AudioDeviceType;

namespace qti::audio::core {

struct PlatformGlobalCallback {
    virtual ~PlatformGlobalCallback() = default;
    virtual void onSoundDose(
            void* const eventData,
            const ::aidl::android::media::audio::common::AudioDevice& audioDevice) = 0;
};

}