/*
* Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include <include/extensions/AudioHalFocusManager.h>
#include <android-base/logging.h>
#include <utility>
#include <include/extensions/AudioConfig.h>
#include <include/extensions/PalParamDelegator.h>
#include <PalApi.h>

#define ALL_BUS_VOLUMES 0x7F
#define BUS_COUNT 5
#define MIN_GAIN -9000.0
#define XML_FILE_PATH "/vendor/etc/audio_ar/duck_configuration.xml"
#define RAMP_MEDIUM_DURATION 90
#define MAX_RAMP_DURATION 1000
#define MIN_RAMP_DURATION 0
namespace qti::audio::core {

AudioFocusService* mHalFocusService = nullptr;


    std::unordered_map<StreamType,
            std::unordered_map<StreamType, ParseParams> > AudioFocusService::configuration;

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

    int32_t AudioFocusService::setRampParam(int32_t uptime, int32_t downtime, int32_t shape, pal_stream_handle_t* handle) {
        int32_t ret = 0;
        pal_awx_volume_data rampuptime={}, rampdowntime={}, ramptype={};
        aidl::qti::awx::pal_awx_param_t *pal_param = NULL;
        aidl::qti::awx::effect_type type = aidl::qti::awx::SYNC_WITH_AUDIO_BUS;

        LOG(DEBUG) << "Enter: " << __func__ << "uptime: " << uptime << " downtime: " << downtime << " shape: " << shape << " handle: " << handle;

        rampuptime.volume_func = 0x1;
        rampdowntime.volume_func = 0x1;
        ramptype.volume_func = 0x1;

        rampuptime.value[0] = uptime;
        rampdowntime.value[0] = downtime;
        ramptype.value[0] = shape;

        if(handle == NULL)
        {
            LOG(ERROR) << __func__ << "Invalid Handle!";
            return -1;
        }

        pal_param = (aidl::qti::awx::pal_awx_param_t *)malloc(sizeof(aidl::qti::awx::pal_awx_param_t));
        if (pal_param == NULL) {
            LOG(ERROR) << __func__ << "Memory not assigned properly";
            return -1;
        }

        pal_param->param_size = sizeof(pal_awx_volume_data);
        pal_param->param_id= AWX_VOLUME_RAMP_UP_TIME;
        pal_param->data = (void *)&rampuptime;
        aidl::qti::awx::PalParamDelegator::AWX_set_param_handle(handle, pal_param, type);

        pal_param->param_id= AWX_VOLUME_RAMP_DOWN_TIME;
        pal_param->data = (void *)&rampdowntime;
        aidl::qti::awx::PalParamDelegator::AWX_set_param_handle(handle, pal_param, type);

        pal_param->param_id= AWX_VOLUME_RAMP_SHAPE;
        pal_param->data = (void *)&ramptype;
        aidl::qti::awx::PalParamDelegator::AWX_set_param_handle(handle, pal_param, type);

        free(pal_param);
        return 0;

    }

    void AudioFocusService::enforceRampParameters(const std::vector<Reasons> duckReasons) {

        //check if we have pal handle for media
        //if no pal handle return;
        pal_stream_handle_t** mPalHandle = nullptr;
        for (auto entry: registeredFocusCallbacks) {
            auto focusId = entry.first;
            auto focusInfo = entry.second;
            if (std::holds_alternative<AudioUsage>(focusInfo.usage) &&
                        get<AudioUsage>(focusInfo.usage) == AudioUsage::MEDIA) {
                if (focusInfo.mPalHandle != nullptr) {
                    mPalHandle = focusInfo.mPalHandle;
                }
            }
        }
        if (mPalHandle == nullptr || *mPalHandle == nullptr) {
            LOG(INFO) << "PAL handle not found for media for enforcing RAMP params!!";
            return;
        }
        int ret = 0;
        if (duckReasons.size()) {
            for (auto reason: duckReasons) {
                //find if any active focus client has ramp attributes, if so enforce them on media
                //if ramp was enforced and no active reason requires it to be enforced, reset it
                if (reason == Reasons::PROJECTION_DUCKING) {
                    for (auto entry: registeredFocusCallbacks) {
                        auto focusId = entry.first;
                        auto focusInfo = entry.second;
                        if (reasonsMap.count(focusInfo.usage) &&
                            reasonsMap[focusInfo.usage] == Reasons::PROJECTION_DUCKING) {
                            if (focusInfo.rampDuration >= MIN_RAMP_DURATION &&
                                    focusInfo.rampDuration <= MAX_RAMP_DURATION) {
                                LOG(INFO) << "Enforcing CarPlay Duck Ramp Params ";
                                setRampParam(focusInfo.rampDuration, focusInfo.rampDuration,
                                    RAMP_SHAPE_EXP, *mPalHandle);
                            } else {
                                LOG(ERROR) << "RAMP Duration out of bounds, skipping RAMP";
                            }
                            isCarPlayRampEnforce = true;
                            return;
                        }
                    }
                } else if (reason == Reasons::NAV_DUCKING) {
                    for (auto entry: registeredFocusCallbacks) {
                        auto focusId = entry.first;
                        auto focusInfo = entry.second;
                        if (reasonsMap.count(focusInfo.usage) &&
                            reasonsMap[focusInfo.usage] == Reasons::NAV_DUCKING) {
                                LOG(INFO) << "Enforcing Ramp Params ";
                            setRampParam(RAMP_MEDIUM_DURATION, RAMP_MEDIUM_DURATION,
                                    RAMP_SHAPE_EXP, *mPalHandle);
                            isMediaRampEnforced = true;
                            return;
                        }
                    }
                }
            }
        }
        //unducking due carplay, ramp is still enforced in this case on media
        if (isCarPlayRampEnforce) {
            LOG(INFO) << "Enforcing CarPlay UnDuck Ramp Params ";
            for (auto entry: registeredFocusCallbacks) {
                auto focusId = entry.first;
                auto focusInfo = entry.second;
                if (reasonsMap.count(focusInfo.usage) &&
                    reasonsMap[focusInfo.usage] == Reasons::PROJECTION_DUCKING) {
                    if (focusInfo.rampDuration >= MIN_RAMP_DURATION &&
                            focusInfo.rampDuration <= MAX_RAMP_DURATION) {
                        LOG(INFO) << "Enforcing CarPlay Duck Ramp Params ";
                        setRampParam(focusInfo.rampDuration, focusInfo.rampDuration,
                            RAMP_SHAPE_EXP, *mPalHandle);
                        isCarPlayRampEnforce = false;
                    } else {
                        LOG(ERROR) << "RAMP Duration out of bounds, skipping RAMP";
                    }
                    return;
                }
            }
            return;
        }
        if (isMediaRampEnforced) {
            //reset ramp paramters to default
            LOG(INFO) << "Restoring Ramp Params to Default";
            auto rampParams = rampMap[AudioUsage::UNKNOWN];
            setRampParam(rampParams.rampUpTime, rampParams.rampDownTime,
                            rampParams.rampShape, *mPalHandle);
            isMediaRampEnforced = false;
        }
        return;

    }

    std::vector<int32_t> getVolumeProfile(uint16_t bus_mask) {
        LOG(DEBUG) << "Enter " << __func__;
        int ret;
        std::vector<int32_t> volumes(7, -1);

        ::aidl::qti::awx::pal_awx_param_t pal_param;
        memset(&pal_param, 0, sizeof(::aidl::qti::awx::pal_awx_param_t));

        ::aidl::qti::awx::VolumeParams params;
        params.eq_mask = bus_mask;
        // params.eq_mask = SET; //setting all bus
        memset(&params, 0, sizeof(::aidl::qti::awx::VolumeParams));

        pal_param.param_id = PARAM_ID_VOLUME ;
        pal_param.param_size = sizeof(::aidl::qti::awx::VolumeParams);
        pal_param.data = &params;

        aidl::qti::awx::effect_type type = ::aidl::qti::awx::effect_type::SYNC_WITH_AUDIO_BUS;
        ret = ::aidl::qti::awx::PalParamDelegator::AWX_get_param(&pal_param, type);

        std::string busTypes[] = {
            "BUS00_MEDIA",             // bit0
            "BUS01_SYS_NOTIFICATION",  // bit1
            "BUS02_NAV_GUIDANCE",      // bit2
            "BUS03_PHONE",             // bit3
            "BUS0F_NAV_GUIDANCE2",     // bit4
            "BUS01_no_ASIL",           // bit5
            "BUS02_Road_ADAS"          // bit6
        };
        if (ret < 0) {
            LOG(ERROR) << __func__ << "Error while fetching value returned with ret: " << ret;
            return volumes;
        }
        else {
            for(int i=0; i<7; i++){
                if (bus_mask & (1 << (i))) {
                    int volValue = params.value[i] ;

                    if (volValue < MIN_VOLUME_VALUE || volValue > MAX_VOLUME_VALUE) {
                        LOG(ERROR) << __func__ << "Unsupported volume value " << "for " << busTypes[i];
                    }
                    else{
                        volumes[i] = params.value[i];
                        LOG(DEBUG) << __func__ << " " << busTypes[i] << " Volume fetched successfully! ret: " << params.value[i];
                    }
                }
            }
        }

        LOG(DEBUG) << "Exit " << __func__;
        return volumes;
    }

    void AudioFocusService::restoreBusVolumes() {
        //get bus volumes for all buses
        //TODO: fetch buses during runtime
        std::string busTypes[] = {
            "BUS00_MEDIA",             // bit0
            "BUS01_SYS_NOTIFICATION",  // bit1
            "BUS02_NAV_GUIDANCE",      // bit2
            "BUS03_PHONE",             // bit3
            "BUS0F_NAV_GUIDANCE2",     // bit4
        };
        std::vector<int32_t> busVolumes = getVolumeProfile(ALL_BUS_VOLUMES);
        std::vector<Reasons> reasons{};
        reasons.push_back(Reasons::EXTERNAL_AMP_VOL_FEEDBACK);
        std::vector<AudioGainConfigInfo> agcis{};
        for (int i = 0; i < BUS_COUNT; i++) {
            AudioGainConfigInfo agci{
                    .zoneId = 0,
                    .devicePortAddress = busTypes[i],
                    .volumeIndex = getNearestIndex((int)busVolumes[i]),
            };
            agcis.push_back(agci);
            for (auto &entry: registeredFocusCallbacks) {
                auto focusId = entry.first;
                auto &focusInfo = entry.second;
                auto in_usage = focusInfo.usage;
                if (auto address = focusInfo.device.address.get<AudioDeviceAddress::Tag::id>();
                        !address.empty() && address == agci.devicePortAddress) {
                    focusInfo.gain = busVolumes[i];
                    LOG(INFO) << "Synced duckVolume for focusID " << focusId;
                }
            }
            LOG(INFO) << "Restoring volume for bus "
                << agci.devicePortAddress << " to " << busVolumes[i] << "(index: " << agci.volumeIndex << ")";
        }

        if (getAudioControlService() == nullptr) {
            LOG(ERROR) << __func__ << ": Unable to report audio gain changed - "
                                    "IAudioControlInternal not registered";
            return;
        }
        auto ret = getAudioControlService()->reportAudioDeviceGainChanged(reasons, agcis);
        if (!ret.isOk()) {
            LOG(ERROR) << __func__ << ": Unable to report audio gain changed ";
            return;
        }
    }

    void AudioFocusService::syncVolumeChanges() {
        for (auto volumeInfo: tempReportInfo) {
                std::vector< std::pair<std::string, std::pair<float, std::unordered_set<Reasons> > > > tempReportInfo;
            std::string busAddress = volumeInfo.first;
            float gain = volumeInfo.second.first;
            //update the voumes for device entry also
            for (auto &entry: registeredFocusCallbacks) {
                auto focusId = entry.first;
                auto &focusInfo = entry.second;
                auto in_usage = focusInfo.usage;
                if (auto address = focusInfo.device.address.get<AudioDeviceAddress::Tag::id>();
                        !address.empty() && address == busAddress) {
                    focusInfo.gain = gain;
                    LOG(INFO) << "Synced duckVolume for focusID " << focusId;
                }
            }
        }
    }

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
                enforceRampParameters(duckReasons);
                syncVolumeChanges();
                audioControlService->reportAudioDeviceGainChanged(duckReasons, duckAgcis);
            } else {
                LOG(ERROR) << "Failed to report device gain change";
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
        if ((std::holds_alternative<std::string>(in_usage) &&
                        get<std::string>(in_usage) == "THERMAL_MITIGATION") ||
            (std::holds_alternative<std::string>(in_usage) &&
                        get<std::string>(in_usage) == "DEVICE_TEMPERATURE_STATUS")) {
            return true;
        }
        return false;
    }


    void AudioFocusService::populateReasonsnGains(int64_t focusId) {

        auto activeFocusInfo = registeredFocusCallbacks[focusId];
        auto address = activeFocusInfo.device.address.get<AudioDeviceAddress::Tag::id>();
        float mostCriticalGain = activeFocusInfo.gain, gain;
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
                } else if (configuration[usage2][usage1].vol_override){
                    ParseParams params = configuration[usage2][usage1];
                    gain = params.gain;
                } else { /*read attenuation target from SO*/
                    ::qti::audio::oem::config::AudioConfigType
                            req = ::qti::audio::oem::config::AUDIO_CONFIG_ATTENUATION_TARGET;
                    ::qti::audio::oem::config::AudioConfigData configData;
                    ::qti::audio::oem::config ::AudioConfigManager::getInstance().
                                                    getAudioConfigValue(req, &configData);
                    gain = (float)(configData.defaultValue);
                    LOG(INFO) << "Attenuation info: " << gain;
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
        //set ramp params
        auto mPalHandle = registeredFocusCallbacks[focusId].mPalHandle;
        if (mPalHandle != nullptr && *mPalHandle != nullptr &&
                std::holds_alternative<AudioUsage>(registeredFocusCallbacks[focusId].usage)) {
            auto usage = get<AudioUsage>(registeredFocusCallbacks[focusId].usage);
            RampParams rampParams = rampMap[AudioUsage::UNKNOWN];
            if (rampMap.count(usage)) {
                rampParams = rampMap[usage];
            }
            setRampParam(rampParams.rampUpTime, rampParams.rampDownTime,
                                rampParams.rampShape, *mPalHandle);
        }

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
                } else if (checkIfExists(activeFocusInfo.usage, focusInfo.usage)) {
                  if (!nonReportReasons(activeFocusInfo.usage)) {
                         duckFocusIds.insert(activeFocusId);
                  }
                }
                if (registeredStreams[activeFocusId].size()) {
                    populateReasonsnGains(activeFocusId);
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

    bool AudioFocusService::isMuteAbandonRequest(int64_t focusId) {
        auto focusInfo = registeredFocusCallbacks[focusId];
        if (std::holds_alternative<Type>(focusInfo.usage)) {
            return true;
        }
        return false;
    }

    void AudioFocusService::handleFocusAbandon(int64_t focusId) {
        if (isMuteAbandonRequest(focusId)) {
            restoreBusVolumes();
        }

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
                                for (auto &entry: registeredFocusCallbacks) {
                                    auto focusId = entry.first;
                                    auto &focusInfo = entry.second;
                                    auto in_usage = focusInfo.usage;
                                    if (auto address = focusInfo.device.address.get<AudioDeviceAddress::Tag::id>();
                                            !address.empty() && address == agci.devicePortAddress) {
                                        focusInfo.gain = registeredFocusCallbacks[curFocusId].gain;
                                        LOG(INFO) << "Synced duckVolume for focusID " << focusId;
                                    }
                                }
                                audioControlService->reportAudioDeviceGainChanged({Reasons::EXTERNAL_AMP_VOL_FEEDBACK},
                                                                                    {agci});
                            } else {
                                LOG(ERROR) << "Faild to report device gain change";
                            }
                            }

                    }
                    populateReasonsnGains(curFocusId);
                } else if (duckFocusIds.size() > 0) {
                    populateReasonsnGains(curFocusId);
                }

            }
            reportGainChanges();
            //in car play cases media might needs restored to slow
            //enforceRampParameters({});

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
    auto incomingGainIndex = getNearestIndex(gain);
    auto curGainIndex = getNearestIndex(registeredFocusCallbacks[focusId].gain);
    if (incomingGainIndex == curGainIndex &&
        registeredFocusCallbacks[focusId].isExternalGain == isExternalGain) {
            return;
    }
    registeredFocusCallbacks[focusId].gain = gain;
    registeredFocusCallbacks[focusId].isExternalGain = isExternalGain;
    LOG(INFO) << "Volume updated for focus ID " << focusId;
    auto &duckFocusIds = registeredStreams[focusId];
    //no need to recompute, since focus doesn't change
    if (duckFocusIds.size() == 0) {
        return;
    }
    auto it = duckFocusIds.begin();
    while (it != duckFocusIds.end()) {
        auto activeFocusInfo = registeredFocusCallbacks[*it];
        if (nonReportReasons(activeFocusInfo.usage)) {
            LOG(INFO) << "Removing focusId " << *it <<
                 "from ducking " << focusId;
            it = duckFocusIds.erase(it);
        } else {
            ++it;
        }
    }

    for (auto entry: registeredStreams) { //loop through active focus sessions for streams
        auto activeFocusId = entry.first;
        if (registeredStreams[activeFocusId].size()) {
            populateReasonsnGains(activeFocusId);
        }
    }
    reportGainChanges();
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
                            << " running_src:" << prop.running_src
                            << " vol_override" << prop.vol_override;
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

