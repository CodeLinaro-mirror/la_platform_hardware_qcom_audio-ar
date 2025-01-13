/*
* Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#pragma once

#include <aidl/android/hardware/audio/focus/BnAudioFocusService.h>
#include <aidl/android/hardware/audio/focus/IStreamUpdateCallback.h>
#include <aidl/android/media/audio/common/AudioUsage.h>
#include <aidl/android/hardware/automotive/audiocontrol/Reasons.h>
#include <aidl/android/hardware/automotive/audiocontrol/AudioGainConfigInfo.h>
#include <aidl/alliance/hardware/automotive/audiocontrol/internal/IAudioControlInternal.h>

#include <android/binder_manager.h>
#include <android/binder_process.h>

#include <vector>
#include <map>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <thread>

using ::aidl::android::media::audio::common::AudioUsage;
using ::aidl::android::media::audio::common::AudioDevice;
using ::aidl::android::media::audio::common::AudioDeviceAddress;
using ::aidl::android::hardware::automotive::audiocontrol::Reasons;
using ::aidl::android::hardware::automotive::audiocontrol::AudioGainConfigInfo;
using ::aidl::android::hardware::audio::focus::IFocusSession;
using ::aidl::android::hardware::audio::focus::IStreamUpdateCallback;
using ::aidl::alliance::hardware::automotive::audiocontrol::internal::IAudioControlInternal;


namespace aidl::android::hardware::audio::focus {

enum FocusCommand {
    REQUEST_FOCUS = 0,
    ABANDON_FOCUS,
    EXIT,
};

struct FocusAction {
    FocusAction(FocusCommand command, int focusId) {
        this->command = command;
        this->focusId = focusId;
    }
    int focusId;
    FocusCommand command;
};

struct FocusInfo {
        AudioUsage usage;
        std::shared_ptr<IStreamUpdateCallback> callback;
        AudioDevice device;
        float gain;
};


class AudioFocusService : public BnAudioFocusService {


    public:
        std::mutex mtx;
        std::condition_variable cv;
        std::queue<FocusAction> focusQueue;
        std::unique_ptr<std::thread> worker_thread;

        static std::unordered_map< AudioUsage,
            std::unordered_map<AudioUsage, float> > configuration;
        AudioFocusService();
        ~AudioFocusService();

        void handleFocusRequest(int focusId);
        void handleFocusAbandon(int focusId);

        ::ndk::ScopedAStatus requestFocus(
                                    const std::shared_ptr<IStreamUpdateCallback>& in_callback,
                                    AudioUsage in_usage,
                                    const AudioDevice& in_device,
                                    float in_gain,
                                    IFocusSession* _aidl_return) override;
        ::ndk::ScopedAStatus abandonFocus(const IFocusSession& in_focusSession) override;
    private:

        std::unordered_map<int, FocusInfo> registeredFocusCallbacks;
        std::unordered_map<int, std::unordered_set<int> > registeredStreams;
        std::unordered_set<int> globalActiveFocusSessions;
        std::map<AudioUsage, Reasons>
            reasonsMap = {{ AudioUsage::ASSISTANCE_NAVIGATION_GUIDANCE,
                            Reasons::NAV_DUCKING}};
        std::vector<AudioGainConfigInfo> agcis;
        const std::map<int,int> volumeMap = {
            {-9000, 0}, {-7900, 1}, {-6822, 2}, {-6132, 3}, {-5643, 4}, {-5264, 5},
            {-4954, 6}, {-4692, 7}, {-4500, 8}, {-4300, 9}, {-4100, 10}, {-3900, 11},
            {-3700, 12}, {-3500, 13}, {-3380, 14}, {-3250, 15}, {-3120, 16}, {-2990, 17},
            {-2860, 18}, {-2730, 19}, {-2600, 20}, {-2470, 21}, {-2340, 22}, {-2210, 23},
            {-2080, 24}, {-1950, 25}, {-1820, 26}, {-1690, 27}, {-1560, 28}, {-1430, 29},
            {-1300, 30}, {-1170, 31}, {-1040, 32}, {-910, 33}, {-780, 34}, {-650, 35},
            {-520, 36}, {-390, 37}, {-260, 38}, {-130, 39}, {0, 40}};

        std::shared_ptr<IAudioControlInternal> mAudioControlInternalService;
        std::shared_ptr<IAudioControlInternal> getAudioControlService();

        static void threadLoop(AudioFocusService* focusService);
        bool checkIfExists(AudioUsage usage1, AudioUsage usage2);
        void reportGainChange(int focusId);
        int generateUUID();
        float getMostCriticalGain(int focusId, std::unordered_set<int> &duckFocusIds);
        void triggerCallbacks();
        int getNearestIndex(int gain);


};

}

