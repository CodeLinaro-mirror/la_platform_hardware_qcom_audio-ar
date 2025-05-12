/*
* Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include <IVhalClient.h>
#include <utils/Log.h>
#include <utils/Errors.h>
#include <android-base/logging.h>

#include <vector>

/*
 * This class listens for asynchronous updates from the Vehicle HAL.
 */
class AudioVHALListener final :
      public android::frameworks::automotive::vhal::ISubscriptionCallback {
public:
    void onPropertyEvent(const std::vector<std::unique_ptr<android::frameworks::automotive::vhal::IHalPropValue>>& values) override;
    void onPropertySetError(
            [[maybe_unused]] const std::vector<android::frameworks::automotive::vhal::HalPropError>&
                    errors) override {
        LOG(ERROR) << "onPropertySetError: failed to set VHAL property";
        return;
    }
};

