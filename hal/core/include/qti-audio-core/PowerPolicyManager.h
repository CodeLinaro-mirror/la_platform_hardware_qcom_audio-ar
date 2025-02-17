/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include <qti-audio-core/Stream.h>
#include <qti-audio-core/StreamInPrimary.h>
#include <qti-audio-core/StreamOutPrimary.h>
namespace qti::audio::core {
class PowerPolicyManager {
public:
    static PowerPolicyManager& getInstance() {
        static const auto kPowerPolicyManager = []() {
            std::unique_ptr<PowerPolicyManager> audioExt{new PowerPolicyManager()};
            return std::move(audioExt);
        }();
        return *(kPowerPolicyManager.get());
    }
    void updateStreamOutPrimaryList(const std::shared_ptr<StreamOutPrimary> streamOutPrimary) {
        mStreamOutPrimaryList_.push_back(streamOutPrimary);
    }
    void updateStreamInPrimaryList(const std::shared_ptr<StreamInPrimary> streamInPrimary) {
        mStreamInPrimaryList_.push_back(streamInPrimary);
    }
    std::vector <std::weak_ptr<::qti::audio::core::StreamOutPrimary>>& getStreamOutPrimaryList() {
        return mStreamOutPrimaryList_;
    }
    std::vector <std::weak_ptr<::qti::audio::core::StreamInPrimary>>& getStreamInPrimaryList() {
        return mStreamInPrimaryList_;
    }
    explicit PowerPolicyManager() = default;
    PowerPolicyManager(const PowerPolicyManager&) = delete;
    PowerPolicyManager& operator=(const PowerPolicyManager& x) = delete;
    PowerPolicyManager(PowerPolicyManager&& other) = delete;
    PowerPolicyManager& operator=(PowerPolicyManager&& other) = delete;
private:
    std::vector <std::weak_ptr<::qti::audio::core::StreamOutPrimary>> mStreamOutPrimaryList_;
    std::vector <std::weak_ptr<::qti::audio::core::StreamInPrimary>> mStreamInPrimaryList_;
};

}  // namespace qti::audio::core
