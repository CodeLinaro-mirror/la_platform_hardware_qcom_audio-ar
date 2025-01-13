/*
* Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/


#include <AudioHalFocusManager.h>
#include <android-base/logging.h>


namespace aidl::android::hardware::audio::focus {


    std::unordered_map<AudioUsage,
            std::unordered_map<AudioUsage, float> > AudioFocusService::configuration;

    std::shared_ptr<IAudioControlInternal> AudioFocusService::getAudioControlService() {

        if (mAudioControlInternalService == nullptr) {
            std::string serviceName =
                    std::string().append(IAudioControlInternal::descriptor).append("/default");

            if (!AServiceManager_isDeclared(serviceName.c_str())) {
                LOG(ERROR) <<"IAudioControlInternal not declared, exiting";
                return nullptr;
            }

            AIBinder* binder = AServiceManager_waitForService(serviceName.c_str());
            if (binder != nullptr) {
                ndk::SpAIBinder spBinder(binder);
                std::shared_ptr<IAudioControlInternal> service =
                                    IAudioControlInternal::fromBinder(spBinder);
                if (service != nullptr) {
                    mAudioControlInternalService = service;
                    LOG(INFO) << "Connected to IAudioControlInternal service";
                } else {
                    LOG(ERROR) << "Can't connect to IAudioControlInternal service";
                }
            } else {
                LOG(ERROR) << "Failed to get service handle for " << serviceName;
            }
        }
        return mAudioControlInternalService;
    }

    bool AudioFocusService::checkIfExists(AudioUsage usage1, AudioUsage usage2) {
        auto config = AudioFocusService::configuration;
        auto highPriority = config.find(usage1);
        if (highPriority != config.end()) {
            auto lowerPriority = highPriority->second.find(usage2);
            if (lowerPriority != highPriority->second.end()) {
                return true;
            }
        }
        return false;
    }

    void AudioFocusService::reportGainChange(int focusId) {
        std::vector<Reasons> duckReasons;
        //find most critical usage for passing reasons;
        AudioUsage usage = AudioUsage::UNKNOWN;

        if (registeredStreams.find(focusId) != registeredStreams.end()) {
            auto duckFocusIds = registeredStreams[focusId];
            usage = registeredFocusCallbacks[focusId].usage;
            for (auto duckFocusId: duckFocusIds) {
                auto &usageDucking = registeredFocusCallbacks[duckFocusId].usage;
                if (checkIfExists(usageDucking, usage)
                        && reasonsMap.find(usageDucking) != reasonsMap.end())
                    usage = usageDucking;
            }
        }

        if (reasonsMap.find(usage) != reasonsMap.end()) {
            duckReasons.push_back(reasonsMap[usage]);
        }
        if (agcis.size()/* have one entry with gain change*/) {
            LOG(INFO) << "report device gain changed";
            if (const auto &audioControlService = getAudioControlService();
                                                    audioControlService != nullptr) {
                audioControlService->reportAudioDeviceGainChanged(duckReasons, agcis);
            } else {
                LOG(ERROR) << "Faild to report device gain change";
            }
        }
        return;
    }

    float AudioFocusService::getMostCriticalGain(int focusId,
                                std::unordered_set<int> &duckFocusIds) {
        float mostCriticalGain = 0.0;
        auto &usage1 = registeredFocusCallbacks[focusId].usage;
        auto &configuration = AudioFocusService::configuration;
        for (auto duckingFocusId : duckFocusIds) {
            auto &usage2 = registeredFocusCallbacks[duckingFocusId].usage;
            if (checkIfExists(usage2, usage1))
                mostCriticalGain = fmin(mostCriticalGain, configuration[usage2][usage1]);
            else LOG(ERROR) << "Error!! mapping doesn't exist between two usages";
        }
        return mostCriticalGain;
    }

    int AudioFocusService::getNearestIndex(int gain) {
        auto it = volumeMap.lower_bound(gain);
        if (it == volumeMap.begin()) {
            return it->second;
        }
        if (it == volumeMap.end()) {
            return std::prev(it)->second;
        }
        auto prevIt = std::prev(it);
        if (std::abs(gain - prevIt->first) <= std::abs(gain - it->first)) {
            return prevIt->second;
        } else {
            return it->second;
        }
    }

    void AudioFocusService::triggerCallbacks() {
        //loop through each entry, any session with zero ducking focusIds
        for (auto entry: registeredStreams) {
            auto duckFocusIds = entry.second;
            auto activeFocusId = entry.first;
            if (duckFocusIds.size() == 0) {
                if (globalActiveFocusSessions.find(activeFocusId) ==
                                        globalActiveFocusSessions.end()) {
                    globalActiveFocusSessions.insert(activeFocusId);
                    //trigger callback to unduck
                    LOG(INFO) << "Triggering callback to unduck on: " << activeFocusId;
                    auto focusInfo = registeredFocusCallbacks[activeFocusId];
                    focusInfo.callback->onMetadataUpdated(false, focusInfo.gain);
                    //TODO: gain change can happen during unducking as well,need to adjust gain
                    // when a stream is ducked by less critical usage than previous one
                    auto device = registeredFocusCallbacks[activeFocusId].device;
                    if (auto address = device.address.get<AudioDeviceAddress::Tag::id>();
                                                                        !address.empty()) {
                        AudioGainConfigInfo agci{
                                .zoneId = 0,
                                .devicePortAddress = address,
                                .volumeIndex = getNearestIndex((int)focusInfo.gain),
                        };
                        agcis.push_back(agci);
                    }
                }
            } else if (globalActiveFocusSessions.find(activeFocusId)
                                        != globalActiveFocusSessions.end()) {
                    globalActiveFocusSessions.erase(activeFocusId);
                    //trigger callback to duck
                    LOG(INFO) << "Triggering callback to duck on: " << activeFocusId;
                    auto streamCallback = registeredFocusCallbacks[activeFocusId].callback;
                    float criticalGain = getMostCriticalGain(activeFocusId, duckFocusIds);
                    streamCallback->onMetadataUpdated(true, criticalGain);
                    //update
                    auto device = registeredFocusCallbacks[activeFocusId].device;
                    if (auto address = device.address.get<AudioDeviceAddress::Tag::id>();
                                                                        !address.empty()) {
                        AudioGainConfigInfo agci{
                                .zoneId = 0,
                                .devicePortAddress = address,
                                .volumeIndex = getNearestIndex((int)criticalGain),
                        };
                        agcis.push_back(agci);
                    }
            }
        }
    }

    void AudioFocusService::handleFocusRequest(int focusId) {
        std::unordered_set<int> duckFocusIds;

        if (registeredStreams.find(focusId) == registeredStreams.end()) {
            // add an entry to the list, and update the per session list for all other entries
            for (auto entry: registeredStreams) { //loop through active focus sessions for streams
                auto activeFocusId = entry.first;
                auto activeFocusInfo = registeredFocusCallbacks[activeFocusId];
                auto focusInfo = registeredFocusCallbacks[focusId];
                if (checkIfExists(focusInfo.usage, activeFocusInfo.usage)) {
                    registeredStreams[activeFocusId].insert(focusId);
                } else if (checkIfExists(activeFocusInfo.usage, focusInfo.usage)) {
                    duckFocusIds.insert(activeFocusId);
                }
            }
            registeredStreams[focusId] = duckFocusIds;
            //incoming request is active for now, so add an entry in globalactive sessions,
            //so duck could be triggered if needed
            globalActiveFocusSessions.insert(focusId);
            agcis.clear();
            triggerCallbacks();
            reportGainChange(focusId);

        } else {
            // assert as focus request is raised only during start
            LOG(ERROR) << "focus ID for focus request already exists";
        }
        return;
    }

    void AudioFocusService::handleFocusAbandon(int focusId) {

        if (globalActiveFocusSessions.find(focusId) != globalActiveFocusSessions.end())
            globalActiveFocusSessions.erase(focusId);

        if (registeredStreams.find(focusId) != registeredStreams.end()) {
            // delete the entry from the list, and update per session list for all other entries
            registeredStreams.erase(focusId);
            for (auto &entry: registeredStreams) { //loop through active focus sessions
                auto &duckFocusIds = entry.second;
                if (duckFocusIds.find(focusId) != duckFocusIds.end())
                        duckFocusIds.erase(focusId);
            }
            agcis.clear();
            triggerCallbacks();
            reportGainChange(focusId);

        } else {
            // assert as focus request is raised only during start
            LOG(ERROR) << "focus ID for abandon focus doesn't exist";
        }

        //remove the entry from registered callback as well
        if (registeredFocusCallbacks.find(focusId) != registeredFocusCallbacks.end()) {
            registeredFocusCallbacks.erase(focusId);
        } else {
            LOG(ERROR) << "callback wasn't registered, not expected";
        }
        return;
    }

    void AudioFocusService::threadLoop(AudioFocusService* focusService) {
        LOG(INFO) << "threadLoop started";
        std::mutex &mtx = focusService->mtx;
        std::queue<FocusAction> &focusQueue = focusService->focusQueue;
        std::condition_variable &cv = focusService->cv;

        std::unique_lock _lock(mtx);
        while (true) {
            cv.wait(_lock, [&focusQueue]{ return !focusQueue.empty(); });

            while(!focusQueue.empty()) {
                auto focusAction = focusQueue.front();
                focusQueue.pop();

                auto focusCommand = focusAction.command;
                auto focusId = focusAction.focusId;

                switch (focusCommand) {
                    case FocusCommand::REQUEST_FOCUS:
                        LOG(INFO) << "Focus Requested " << focusId;
                        focusService->handleFocusRequest(focusId);
                        break;

                    case FocusCommand::ABANDON_FOCUS:
                        LOG(INFO) << "Focus Abandoned " << focusId;
                        focusService->handleFocusAbandon(focusId);
                        break;

                    case FocusCommand::EXIT:
                        LOG(INFO) << "Exiting focus thread";
                        return;

                    default:
                        LOG(ERROR) << "Invalid Command";
                        break;
                }
            }
        }

    }

    AudioFocusService::AudioFocusService() {
        LOG(INFO) << "Creating worker thread for handling focus requests";
        this->worker_thread = std::make_unique<std::thread>(&AudioFocusService::threadLoop, this);
        if (!this->worker_thread) {
            LOG(ERROR) << "Thread creation failed";
        }
    }

    AudioFocusService::~AudioFocusService() {
        LOG(INFO) << "Destorying worker thread for handling focus requests";

        {
            std::lock_guard<std::mutex> _lock(mtx);
            focusQueue.push(FocusAction(FocusCommand::EXIT, -1));
        }
        cv.notify_one();
        this->worker_thread->join();
    }

    int AudioFocusService::generateUUID() {
        static int id = 0;
        return ++id;
        //TODO: limit to int_max and reset
    }

    ::ndk::ScopedAStatus AudioFocusService::requestFocus(
                            const std::shared_ptr<IStreamUpdateCallback>& in_callback,
                            AudioUsage in_usage,
                            const AudioDevice& in_device,
                            float in_gain, IFocusSession* _aidl_return) {
        {
            std::lock_guard<std::mutex> _lock(mtx);
            int uuid = generateUUID();
            _aidl_return->FocusId = uuid;
            FocusInfo focusInfo;
            focusInfo.usage = in_usage;
            focusInfo.callback = in_callback;
            focusInfo.device = in_device;
            focusInfo.gain = in_gain;
            registeredFocusCallbacks[uuid] = focusInfo;
            focusQueue.push(FocusAction(FocusCommand::REQUEST_FOCUS, uuid));
        }
        cv.notify_one();
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus AudioFocusService::abandonFocus(const IFocusSession& in_focusSession) {

        {
            std::lock_guard<std::mutex> _lock(mtx);
            focusQueue.push(FocusAction(FocusCommand::ABANDON_FOCUS, in_focusSession.FocusId));
        }
        cv.notify_one();
        return ndk::ScopedAStatus::ok();
    }

}

