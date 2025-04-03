/*
* Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include <include/extensions/AudioHalFocusManager.h>
#include <android-base/logging.h>
#include <utility>
#include <include/extensions/AudioConfig.h>

#define MIN_GAIN -9000.0
#define XML_FILE_PATH "/vendor/etc/audio_ar/duck_configuration.xml"
namespace qti::audio::core {

AudioFocusService* mHalFocusService = nullptr;


    std::unordered_map<StreamType,
            std::unordered_map<StreamType, float> > AudioFocusService::configuration;

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

    bool AudioFocusService::checkIfExists(StreamType usage1,
                                            StreamType usage2) {
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

    int32_t AudioFocusService::getNearestIndex(int32_t gain) {
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

    std::unordered_map<std::string, std::pair<float, std::unordered_set<Reasons> > > reportInfo;
    std::vector< std::pair<std::string, std::pair<float, std::unordered_set<Reasons> > > > tempReportInfo;

    void AudioFocusService::reportGainChanges() {
        std::vector<Reasons> duckReasons;
        std::vector<AudioGainConfigInfo> duckAgcis;

        for (auto entry: tempReportInfo) {
        auto address = entry.first;
        auto tempGain = entry.second.first;
        auto tempReasons = entry.second.second;

        if (reportInfo.count(address)) {
            auto &gain = reportInfo[address].first;
            auto &reasons =  reportInfo[address].second;
            if (gain == tempGain) {
                reasons.insert(tempReasons.begin(), tempReasons.end());
            } else if (gain > tempGain) {
                gain = tempGain;
                reasons.clear();
                reasons.insert(tempReasons.begin(), tempReasons.end());
            }
        } else {
            reportInfo[address] = std::make_pair(tempGain, tempReasons);
        }
        }

        {   //compile the reasons list
            std::unordered_set<Reasons> reasons;
            for (auto entry: reportInfo) {
                auto reasonsTemp = entry.second.second;
                for (auto reason: reasonsTemp) {
                        reasons.insert(reason);
                }
            }

            for (auto reason: reasons) {
                duckReasons.push_back(reason);
            }
        }

        for (auto entry: reportInfo) {

            if (auto address = entry.first; !address.empty()) {
                auto gain = entry.second.first;
                AudioGainConfigInfo agci{
                        .zoneId = 0,
                        .devicePortAddress = address,
                        .volumeIndex = getNearestIndex((int32_t)gain),
                };
                duckAgcis.push_back(agci);
            }
        }


        if (duckAgcis.size()/* have one entry with gain change*/) {
                LOG(INFO) << "report device gain changed";
                if (const auto &audioControlService = getAudioControlService();
                                                        audioControlService != nullptr) {
                    audioControlService->reportAudioDeviceGainChanged(duckReasons, duckAgcis);
                } else {
                    LOG(ERROR) << "Faild to report device gain change";
                }
        }
        tempReportInfo.clear();
        reportInfo.clear();
    }

    bool AudioFocusService::isThermalFocusId(int64_t focusId) {
        if (!registeredFocusCallbacks.count(focusId)) {
            return false;
        }
        auto in_usage = registeredFocusCallbacks[focusId].usage;
        if (std::holds_alternative<std::string>(in_usage) &&
                        get<std::string>(in_usage) == "THERMAL_MITIGATION") {
            return true;
        }
        return false;
    }


    void AudioFocusService::populateReasonsnGains(int64_t focusId) {

        auto activeFocusInfo = registeredFocusCallbacks[focusId];
        auto address = activeFocusInfo.device.address.get<AudioDeviceAddress::Tag::id>();
        float mostCriticalGain = 0.0 /*activeFocusInfo.gain*/, gain;
        std::unordered_set<Reasons> reasons;

        if (address.empty()) {
            LOG(ERROR) << "Device Adress can't be empty, returning !!";
            return;
        }
        auto &usage1 = registeredFocusCallbacks[focusId].usage;
        auto &configuration = AudioFocusService::configuration;
        auto duckFocusIds = registeredStreams[focusId];
        LOG(INFO) << "outside gain " << mostCriticalGain;

        for (auto duckingFocusId : duckFocusIds) {
            auto &usage2 = registeredFocusCallbacks[duckingFocusId].usage;
            auto isExternalGain = registeredFocusCallbacks[duckingFocusId].isExternalGain;
            if (checkIfExists(usage2, usage1) && reasonsMap.count(usage2)) {

                if (isExternalGain) {
                    gain = registeredFocusCallbacks[duckingFocusId].gain;
                } else if (true) { /*read attenuation target from SO*/
                    ::qti::audio::oem::config::AudioConfigType
                            req = ::qti::audio::oem::config::AUDIO_CONFIG_ATTENUATION_TARGET;
                    ::qti::audio::oem::config::AudioConfigData configData;
                    ::qti::audio::oem::config ::AudioConfigManager::getInstance().
                                                    getAudioConfigValue(req, &configData);
                    gain = (float)(configData.defaultValue);
                    LOG(INFO) << "Attenuation info: " << gain;
                } else {
                    gain = configuration[usage2][usage1];
                }
                //TODO: Handle the case where ducking gain is same for multiple duck focusIds
                //TODO: Handle if reaonsMap doesn't exist, don't override
                LOG(INFO) << "mostCritical gain " << mostCriticalGain << "cur gain" << gain;

                if (gain == mostCriticalGain) {
                    reasons.insert(reasonsMap[usage2]);
                } else if (gain < mostCriticalGain) {
                    reasons.clear();
                    reasons.insert(reasonsMap[usage2]);
                    mostCriticalGain = gain;
                }
            }
            else LOG(ERROR) << "Error!! mapping doesn't exist between two usages";
        }
        tempReportInfo.push_back(std::make_pair(address, std::make_pair(mostCriticalGain, reasons)));

    }

    bool AudioFocusService::nonReportReasons(StreamType usage) {
        if (reasonsMap.find(usage) != reasonsMap.end() &&
                (reasonsMap[usage] == Reasons::ADAS_DUCKING ||
                reasonsMap[usage] == Reasons::NAV_DUCKING ||
                reasonsMap[usage] == Reasons::PROJECTION_DUCKING)) {
            return true;
        }
        return false;
    }

    void AudioFocusService::handleFocusRequest(int64_t focusId) {
        std::unordered_set<int64_t> duckFocusIds;

        if (registeredStreams.find(focusId) == registeredStreams.end()) {
            // add an entry to the list, and update the per session list for all other entries
            for (auto entry: registeredStreams) { //loop through active focus sessions for streams
                auto activeFocusId = entry.first;
                auto activeFocusInfo = registeredFocusCallbacks[activeFocusId];
                auto focusInfo = registeredFocusCallbacks[focusId];
                if (checkIfExists(focusInfo.usage, activeFocusInfo.usage)) {
                    registeredStreams[activeFocusId].insert(focusId);
                    LOG(INFO) << "1 calling populateReasonsnGains on " << activeFocusId;
                    populateReasonsnGains(activeFocusId);
                } else if (checkIfExists(activeFocusInfo.usage, focusInfo.usage)) {
                  if (!nonReportReasons(activeFocusInfo.usage)) {
                         duckFocusIds.insert(activeFocusId);
                  }
                }
            }
            registeredStreams[focusId] = duckFocusIds;
            if (duckFocusIds.size()) {
                LOG(INFO) << "2 calling populateReasonsnGains on " << focusId;
                LOG(INFO) << "duckFocusIds size " << duckFocusIds.size();
                populateReasonsnGains(focusId);
            }
            //incoming request is active for now, so add an entry in globalactive sessions,
            //so duck could be triggered if needed
            globalActiveFocusSessions.insert(focusId);
            reportGainChanges();

        } else {
            // assert as focus request is raised only during start
            LOG(ERROR) << "focus ID for focus request already exists";
        }
        return;
    }

    void AudioFocusService::handleFocusAbandon(int64_t focusId) {

        if (globalActiveFocusSessions.find(focusId) != globalActiveFocusSessions.end())
            globalActiveFocusSessions.erase(focusId);

        if (registeredStreams.find(focusId) != registeredStreams.end()) {
            // delete the entry from the list, and update per session list for all other entries
            //TODO: handle when media ends first
            if (registeredStreams[focusId].size()) {
                auto &duckFocusIds = registeredStreams[focusId];
                auto it = duckFocusIds.begin();
                while (it != duckFocusIds.end()) {
                    auto activeFocusInfo = registeredFocusCallbacks[*it];
                    if (nonReportReasons(activeFocusInfo.usage)) {
                        it = duckFocusIds.erase(it);
                    } else {
                        ++it;
                    }
                }
                populateReasonsnGains(focusId);
            }
            registeredStreams.erase(focusId);
            for (auto &entry: registeredStreams) { //loop through active focus sessions
                auto &duckFocusIds = entry.second;
                auto curFocusId = entry.first;
                if (duckFocusIds.find(focusId) != duckFocusIds.end()) {
                        duckFocusIds.erase(focusId);
                        if (isThermalFocusId(focusId)) {
                            auto curIndex = getNearestIndex(registeredFocusCallbacks[curFocusId].gain);
                            auto index = getNearestIndex(registeredFocusCallbacks[focusId].gain);
                            if (curIndex == index) {
                                //thermal was the one ducking the system currently, thermal's at same index
                                //let's set persistent volume to thermal itself and report it.
                                //TODO: report thermal
                            LOG(INFO) << "report device gain changed";
                            if (const auto &audioControlService = getAudioControlService();
                                                                    audioControlService != nullptr) {
                                AudioGainConfigInfo agci{
                                        .zoneId = 0,
                                        .devicePortAddress = "BUS00_MEDIA",
                                        .volumeIndex = index,
                                };
                                audioControlService->reportAudioDeviceGainChanged({Reasons::EXTERNAL_AMP_VOL_FEEDBACK},
                                                                                    {agci});
                            } else {
                                LOG(ERROR) << "Faild to report device gain change";
                            }
                            }

                        }
                        populateReasonsnGains(curFocusId);
                }

            }
            reportGainChanges();

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

void AudioFocusService::handleVolumeChange(int64_t focusId, float gain, bool isExternalGain) {
    auto duckFocusIds = registeredStreams[focusId];
    registeredFocusCallbacks[focusId].gain = gain;
    registeredFocusCallbacks[focusId].isExternalGain = isExternalGain;
    LOG(INFO) << "Volume updated for focus ID " << focusId;
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
                auto gain = focusAction.gain;
                auto isExternalGain = focusAction.isExternalGain;

                switch (focusCommand) {
                    case FocusCommand::REQUEST_FOCUS:
                        LOG(INFO) << "Focus Requested " << focusId;
                        focusService->handleFocusRequest(focusId);
                        break;

                    case FocusCommand::ABANDON_FOCUS:
                        LOG(INFO) << "Focus Abandoned " << focusId;
                        focusService->handleFocusAbandon(focusId);
                        break;
                    case FocusCommand::UPDATE_VOLUME:
                        LOG(INFO) << "Volume updated for: " << focusId;
                        focusService->handleVolumeChange(focusId, gain, isExternalGain);
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

        { //parse configuration from xml
            BusDuckConfigParser parser;
            FILE* file = NULL;
            file = fopen(XML_FILE_PATH, "r");
            if(!file){
                LOG(ERROR) << __func__ <<  "File not present: " << XML_FILE_PATH;
                return;
            }
            else{
                LOG(ERROR) << __func__ <<  "File present" << XML_FILE_PATH;
            }
            fclose(file);

            if (parser.parseConfig(XML_FILE_PATH)) {
                const auto& properties = parser.getProperties();
                for(const auto& prop : properties)
                {
                    LOG(INFO) << __func__ << " In_src:" << prop.in_src
                            << " running_src:" << prop.running_src;
                }
                parser.populateAudioFocusConfig(AudioFocusService::configuration);
            } else {
                LOG(ERROR) << __func__ <<  "Failed to parse configuration" ;
            }
        }

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

    int32_t AudioFocusService::generateUUID() {

        static int32_t id = 0;
        return (id++) % INT_MAX;
    }

    int32_t AudioFocusService::requestFocus(const FocusInfo focusInfo, int64_t* focusId) {
        {
            std::lock_guard<std::mutex> _lock(mtx);
            if (*focusId == -1) {
                *focusId = (int64_t)generateUUID();
                *focusId = *focusId | (1ULL << 32);
            }
            registeredFocusCallbacks[*focusId] = focusInfo;
            focusQueue.push(FocusAction(FocusCommand::REQUEST_FOCUS, *focusId));
        }
        cv.notify_one();
        return 0;
    }

    int32_t AudioFocusService::updateVolume(const int64_t focusId, const float gain, const bool isExternalGain) {
        {
            std::lock_guard<std::mutex> _lock(mtx);
            if (registeredFocusCallbacks.find(focusId) == registeredFocusCallbacks.end()) {
                LOG(ERROR) << "FocusId invalid, couldn't update gain";
                return -1;
            }
            focusQueue.push(FocusAction(FocusCommand::UPDATE_VOLUME, focusId, gain, isExternalGain));
        }
        cv.notify_one();
        return 0;
    }

    int32_t AudioFocusService::abandonFocus(const int64_t focusId) {

        {
            std::lock_guard<std::mutex> _lock(mtx);
            focusQueue.push(FocusAction(FocusCommand::ABANDON_FOCUS, focusId));
        }
        cv.notify_one();
        return 0;
    }


    extern "C" __attribute__((visibility("default")))
    int32_t requestFocus(const FocusInfo focusInfo, int64_t* focusId)  {

        if (focusId == nullptr) {
            LOG(INFO) << "Invalid focus Id address";
            return -1;
        }
        return AudioFocusService::getFocusServiceInstance()
                                .requestFocus(focusInfo, focusId);
    }

    extern "C" __attribute__((visibility("default")))
    int32_t updateVolume(const int64_t focusId, const float gain, const bool isExternalGain) {
        if (focusId < 0) {
            LOG(INFO) << "Invalid focus Id" << focusId;
            return -1;
        }
        return AudioFocusService::getFocusServiceInstance()
                            .updateVolume(focusId, gain, isExternalGain);
    }

    extern "C" __attribute__((visibility("default")))
    int32_t abandonFocus(const int64_t focusId){
        if (focusId < 0) {
            LOG(INFO) << "Invalid focus Id" << focusId;
            return -1;
        }
        return AudioFocusService::getFocusServiceInstance()
                            .abandonFocus(focusId);
    }

}

