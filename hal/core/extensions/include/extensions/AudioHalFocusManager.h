/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#pragma once

#include <aidl/android/media/audio/common/AudioUsage.h>
#include <aidl/android/media/audio/common/AudioDevice.h>

#include <aidl/android/hardware/automotive/audiocontrol/Reasons.h>
#include <aidl/android/hardware/automotive/audiocontrol/AudioGainConfigInfo.h>
#include <aidl/alliance/hardware/automotive/audiocontrol/internal/IAudioControlInternal.h>
#include <aidl/ampere/hardware/interfaces/automotive/audioparameterparser/AudioControlVendorParameterExt.h>
#include <aidl/ampere/hardware/interfaces/automotive/audioparameterparser/RadioVendorParameterExt.h>

#include <android/binder_manager.h>
#include <android/binder_process.h>

#include <vector>
#include <map>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <thread>
#include "BusDuckConfig.h"
#include <PalDefs.h>
#include "PalParamDelegator.h"

using ::aidl::android::media::audio::common::AudioUsage;
using ::aidl::android::media::audio::common::AudioDevice;
using ::aidl::android::media::audio::common::AudioDeviceAddress;
using ::aidl::android::hardware::automotive::audiocontrol::Reasons;
using ::aidl::android::hardware::automotive::audiocontrol::AudioGainConfigInfo;
using ::aidl::alliance::hardware::automotive::audiocontrol::internal::IAudioControlInternal;
using UseCase = ::aidl::ampere::hardware::interfaces::automotive::audioparameterparser::AudioControlVendorParameterExt::AudioFocusRequest::UseCase;
using MuterOderType = ::aidl::ampere::hardware::interfaces::automotive::audioparameterparser::AudioControlVendorParameterExt::AudioFocusRequest::MuterOderType;
using Type = ::aidl::ampere::hardware::interfaces::automotive::audioparameterparser::AudioControlVendorParameterExt::MasterMuteRequest::Type;
using RadioAudioSource = ::aidl::ampere::hardware::interfaces::automotive::audioparameterparser::RadioVendorParameterExt::AudioSource;
#define AWX_VOLUME_RAMP_SHAPE 0x11112504
#define AWX_VOLUME_RAMP_UP_TIME 0x11112502
#define AWX_VOLUME_RAMP_DOWN_TIME 0x11112503
#define RAMP_SHAPE_LINEAR 1
#define RAMP_SHAPE_EXP 2
typedef struct pal_awx_volume_data {
    uint16_t volume_func;
    uint16_t reserved;
    int32_t value[16];
} pal_awx_volume_data_t;

namespace qti::audio::core {

enum FocusCommand{
    REQUEST_FOCUS = 0,
    ABANDON_FOCUS,
    UPDATE_VOLUME,
    UPDATE_FOCUS_REQUEST,
    EXIT,
};

struct FocusSession{
    FocusSession(int64_t focusId) {
        this->FocusId = focusId;
    }
    FocusSession() {
    }

    int64_t FocusId = -1;
};

struct FocusAction {
    FocusAction(FocusCommand command, int64_t focusId) {
        this->command = command;
        this->focusId = focusId;
    }
    FocusAction(FocusCommand command, int64_t focusId, float gain) {
        this->command = command;
        this->focusId = focusId;
        this->gain = gain;
    }
    FocusAction(FocusCommand command, int64_t focusId, float gain, bool isExternalGain) {
        this->command = command;
        this->focusId = focusId;
        this->gain = gain;
        this->isExternalGain = isExternalGain;
    }
    int64_t focusId;
    float gain = 0.0;
    bool isExternalGain = false;
    FocusCommand command;
};

class FocusStreamUpdateCallback {
    public:
        ndk::ScopedAStatus onMetadataUpdated(bool doDuck, float gain){
            LOG(INFO) << "onMetaupdated : gain " << gain;
            return ndk::ScopedAStatus::ok();
        }
};

struct FocusInfo {
        StreamType usage;
        std::shared_ptr<FocusStreamUpdateCallback> callback;
        AudioDevice device;
        float gain;
        bool doTriggerCallback = false; //volume setting can happen externally,
                                       // don't trigger callback in such cases by setting this to false
        bool isExternalGain = false;
        Reasons reason;
        pal_stream_handle_t** mPalHandle = nullptr;
        int rampDuration = -1;
        MuterOderType muteOrderType = MuterOderType::NONE;
};

struct RampParams {
    int32_t rampShape;
    int32_t rampUpTime;
    int32_t rampDownTime;;
};

class AudioFocusService {
    public:
        std::mutex mtx;
        std::condition_variable cv;
        std::queue<FocusAction> focusQueue;
        std::unique_ptr<std::thread> worker_thread;
        static std::unordered_map<StreamType,
            std::unordered_map<StreamType, ParseParams>> configuration;
        std::vector<AudioDevice> activeDevices;
        AudioFocusService();
        ~AudioFocusService();
        static AudioFocusService& getFocusServiceInstance() {
            static const auto kAudioFocusService = []() {
                std::unique_ptr<AudioFocusService> audioExt{new AudioFocusService()};
                return std::move(audioExt);
            }();
            return *(kAudioFocusService.get());
        }
        void handleFocusRequest(int64_t focusId);
        void handleFocusAbandon(int64_t focusId);
        void handleVolumeChange(int64_t focusId, float gain, bool isExternalGain);
        void handleUpdateFocusRequest();
        int32_t requestFocus(const FocusInfo focusInfo, int64_t* focusId);
        int32_t abandonFocus(const int64_t focusId);
        int32_t updateVolume(const int64_t focusId, const float gain, const bool isExternalGain);
        int32_t updateFocusRequest(const int64_t focusId, float gain);
        int32_t getNearestIndex(int32_t gain);
        static const std::unordered_map<std::string,
            std::unordered_map<std::string, StreamType>> usageMap;
        static std::unordered_map<StreamType,
            std::unordered_map<MuterOderType, Reasons>> reasonsMap;
        static const std::unordered_map<std::string, MuterOderType> muteOrderStrtoAidl;
        static const std::unordered_map<std::string, Reasons> reasonStrtoAidl;
    private:
        const char* externalVolumeConfiguration = "/vendor/etc/audio_policy_engine_default_volumes.xml";
        std::unordered_map<int64_t, FocusInfo> registeredFocusCallbacks;
        std::unordered_map<int64_t, std::unordered_set<int64_t> > registeredStreams;
        std::unordered_set<int64_t> globalActiveFocusSessions;
        std::map<AudioUsage, RampParams> rampMap = {
            {AudioUsage::NOTIFICATION_TELEPHONY_RINGTONE, {RAMP_SHAPE_EXP, 20,20}},
            {AudioUsage::VOICE_COMMUNICATION, {RAMP_SHAPE_EXP,20,20}},
            {AudioUsage::VOICE_COMMUNICATION_SIGNALLING, {RAMP_SHAPE_EXP,20,20}},
            {AudioUsage::CALL_ASSISTANT, {RAMP_SHAPE_EXP,20,20}},
            {AudioUsage::ASSISTANCE_NAVIGATION_GUIDANCE, {RAMP_SHAPE_EXP,20,20}},
            {AudioUsage::UNKNOWN, {RAMP_SHAPE_EXP,250,250}},
        };
        bool isMediaRampEnforced = false;
        bool isCarPlayRampEnforce = false;
        float cachedMediaVolume = 1.0;
        bool restoreMediaCachedVolume();
        bool cacheMediaVolume();
        void resetCachedMediaVolume();
        void enforceRampParameters(const std::vector<Reasons> duckReasons);
        std::vector<AudioGainConfigInfo> agcis;
        const std::map<int32_t, int32_t> volumeMap = {
            {-9000, 0}, {-8775, 1}, {-8550, 2}, {-8325, 3},
            {-8100, 4}, {-7875, 5}, {-7650, 6}, {-7425, 7},
            {-7200, 8}, {-6975, 9}, {-6750, 10}, {-6525, 11},
            {-6300, 12}, {-6075, 13}, {-5850, 14}, {-5625, 15},
            {-5400, 16}, {-5175, 17}, {-4950, 18}, {-4725, 19},
            {-4500, 20}, {-4275, 21}, {-4050, 22}, {-3825, 23},
            {-3600, 24}, {-3375, 25}, {-3150, 26}, {-2925, 27},
            {-2700, 28}, {-2475, 29}, {-2250, 30}, {-2025, 31},
            {-1800, 32}, {-1575, 33}, {-1350, 34}, {-1125, 35},
            {-900, 36}, {-675, 37}, {-450, 38}, {-225, 39}, {0, 40}};
        void parseVolumeProfile();
        void processVolumePoints(const std::vector<std::string> &points);
        void getAllActiveReasons(std::vector<Reasons> &activeReasons);
        std::shared_ptr<IAudioControlInternal> mAudioControlInternalService;
        std::shared_ptr<IAudioControlInternal> getAudioControlService();
        int32_t setRampParam(int32_t uptime, int32_t downtime, int32_t shape, pal_stream_handle_t* handle);
        void restoreBusVolumes();
        bool isMuteAbandonRequest(int64_t focusId);
        void syncVolumeChanges();
        bool nonBlockingReasons(StreamType usage, MuterOderType muteOrderType);
        static void threadLoop(AudioFocusService* focusService);
        bool checkIfExists(StreamType usage1, StreamType usage2);
        int32_t generateUUID();
        void reportGainChanges();
        void populateReasonsnGains(int64_t focusId);
        bool isThermalFocusId(int64_t focusId);
        bool isAudioOnMediaBus(int64_t focusId);
        void resetNonBlockingReasons(int64_t focusId);
        void resetDuckFocusId(int64_t focusId);
        void reportThermalReason(int64_t thermalFocusId, int32_t index);
    };
}

