/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once

namespace qti::audio::core {

struct PlatformGlobalCallback {
    virtual ~PlatformGlobalCallback() = default;
    /* Just a placeholder API */
    virtual void onSoundDose(void* const eventData) = 0;
};

}  // namespace qti::audio::core