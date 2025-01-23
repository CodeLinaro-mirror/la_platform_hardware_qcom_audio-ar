/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once

using aidl::android::media::audio::common::AudioDevice;
using aidl::android::media::audio::common::AudioDeviceType;
using aidl::android::media::audio::common::AudioDeviceDescription;
using aidl::android::media::audio::common::AudioDeviceAddress;

namespace qti::audio::core {

struct PlatformGlobalCallback {
    virtual ~PlatformGlobalCallback() = default;
    virtual void onSoundDose(void* const eventData) = 0;
    virtual void updateActiveDevicesMap(const AudioDeviceAddress& address, pal_device_id_t palDeviceId) = 0;
};

}