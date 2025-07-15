/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include <include/extensions/AudioHalFocusManager.h>
#include <android-base/logging.h>
#include <utility>
#include <include/extensions/AudioConfig.h>
#include <include/extensions/PalParamDelegator.h>
#include <PalApi.h>

#include "android_audio_policy_configuration.h"
#include <libxml/parser.h>
#include <libxml/tree.h>

#define ALL_BUS_VOLUMES 0x7F
#define TOTAL_BUS_COUNT 7
#define FVM_BUS_COUNT 5
#define MEDIA_BUS_INDEX 0
#define MIN_GAIN -9000.0
#define XML_FILE_PATH "/vendor/etc/audio_ar/duck_configuration.xml"
#define RAMP_MEDIUM_DURATION 90
#define MAX_RAMP_DURATION 1000
#define MIN_RAMP_DURATION 0
#define DEFAULT_VOLUME_STR "DefaultVolume"
#define MEDIA_BUS "BUS00_MEDIA"
namespace qti::audio::core {

    template <class T>
    constexpr void (*xmlDeleter)(T* t);
    template <>
    constexpr auto xmlDeleter<xmlDoc> = xmlFreeDoc;
    template <>
    auto xmlDeleter<xmlChar> = [](xmlChar *s) { xmlFree(s); };
    template <class T>
    constexpr auto make_xmlUnique(T *t) {
        auto deleter = [](T *t) { xmlDeleter<T>(t); };
        return std::unique_ptr<T, decltype(deleter)>{t, deleter};
    }

    std::map<int32_t, int32_t> globalVolumeMap;
    std::optional<::android::audio::policy::configuration::Volumes>
                                volProfileRead(const char* configFile) {
        auto doc = make_xmlUnique(xmlParseFile(configFile));
        if (doc == nullptr) {
            return std::nullopt;
        }
        xmlNode* _child = xmlDocGetRootElement(doc.get());
        if (_child == nullptr) {
            return std::nullopt;
        }
        if (xmlXIncludeProcess(doc.get()) < 0) {
            return std::nullopt;
        }

        if (!xmlStrcmp(_child->name, reinterpret_cast<const xmlChar*>("volumes"))) {
            ::android::audio::policy::configuration::Volumes
                _value = ::android::audio::policy::configuration::Volumes::read(_child);
            return _value;
        }
        return std::nullopt;
    }
    AudioFocusService* mHalFocusService = nullptr;
    std::unordered_map<StreamType,
                std::unordered_map<MuterOderType, Reasons>>  AudioFocusService::reasonsMap;
    const std::unordered_map<
        std::string, std::unordered_map<std::string, StreamType>>
             AudioFocusService::usageMap = {
            {
                "AudioUsage", {
                    {"ASSISTANCE_NAVIGATION_GUIDANCE",
                        AudioUsage::ASSISTANCE_NAVIGATION_GUIDANCE},
                    {"MEDIA", AudioUsage::MEDIA},
                }
            }, {
                "String", {
                    {"BUS00_MEDIA", "BUS00_MEDIA"},
                    {"BUS01_SYS_NOTIFICATION", "BUS01_SYS_NOTIFICATION"},
                    {"BUS02_NAV_GUIDANCE", "BUS02_NAV_GUIDANCE"},
                    {"BUS03_PHONE", "BUS03_PHONE"},
                    {"BUS0F_NAV_GUIDANCE2","BUS0F_NAV_GUIDANCE2"},
                    {"THERMAL_MITIGATION","THERMAL_MITIGATION"},
                    {"RADIO_AAM_MUTE_ORDER","RADIO_AAM_MUTE_ORDER"},
                    {"NIGHT_MODE","NIGHT_MODE"},
                    {"DEVICE_TEMPERATURE_STATUS","DEVICE_TEMPERATURE_STATUS"},
                    {"CP_DUCK","CP_DUCK"}
                }
            }, {
                "UseCase", {
                        {"WELCOME_SEQUENCE", UseCase::WELCOME_SEQUENCE},
                        {"ROAD_ADAS", UseCase::ROAD_ADAS},
                        {"VEHICLE_WARNING", UseCase::VEHICLE_WARNING},
                        {"VEHICLE_SAFETY_WARNING", UseCase::VEHICLE_SAFETY_WARNING},
                }
            }, {
                "Type", {
                    {"CYBER", Type::CYBER},
                    {"TCU", Type::TCU},
                    {"STATIC_POWER_LIMITATION", Type::STATIC_POWER_LIMITATION},
                    {"DELIVERY_MODE", Type::DELIVERY_MODE},
                }
            }
        };
    const std::unordered_map<std::string, MuterOderType>
             AudioFocusService::muteOrderStrtoAidl = {
        {"BLOCKED", MuterOderType::BLOCKED},
        {"UNBLOCKED", MuterOderType::UNBLOCKED},
        {"NONE", MuterOderType::NONE},
    };
    const std::unordered_map<std::string, Reasons>
             AudioFocusService::reasonStrtoAidl = {
        {"NAV_DUCKING", Reasons::NAV_DUCKING},
        {"ADAS_DUCKING", Reasons::ADAS_DUCKING},
        {"TCU_MUTE", Reasons::TCU_MUTE},
        {"FORCED_MASTER_MUTE", Reasons::FORCED_MASTER_MUTE},
        {"PROJECTION_DUCKING", Reasons::PROJECTION_DUCKING},
        {"THERMAL_LIMITATION", Reasons::THERMAL_LIMITATION},
        {"REMOTE_MUTE", Reasons::REMOTE_MUTE},
    };
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
        auto it = globalVolumeMap.lower_bound(gain);
        if (it == globalVolumeMap.begin()) {
            return it->second;
        }
        if (it == globalVolumeMap.end()) {
            return std::prev(it)->second;
        }
        auto prevIt = std::prev(it);
        if (std::abs(gain - prevIt->first) <= std::abs(gain - it->first)) {
            return prevIt->second;
        } else {
            return it->second;
        }
    }

    std::unordered_map<std::string,
        std::pair<float, std::unordered_set<Reasons> > > reportInfo;
    std::vector<std::pair<std::string,
        std::pair<float, std::unordered_set<Reasons> > > > tempReportInfo;

    int32_t AudioFocusService::setRampParam(int32_t uptime,
            int32_t downtime, int32_t shape, pal_stream_handle_t* handle) {
        int32_t ret = 0;
        pal_awx_volume_data rampuptime={}, rampdowntime={}, ramptype={};
        aidl::qti::awx::pal_awx_param_t *pal_param = NULL;
        aidl::qti::awx::effect_type type = aidl::qti::awx::SYNC_WITH_AUDIO_BUS;

        LOG(DEBUG) << "Enter: " << __func__ << "uptime: " << uptime <<" downtime: "
                        << downtime << " shape: " << shape << " handle: " << handle;

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

        pal_param = (aidl::qti::awx::pal_awx_param_t *)malloc(
                            sizeof(aidl::qti::awx::pal_awx_param_t));
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
                            reasonsMap[focusInfo.usage][focusInfo.muteOrderType]
                                                    == Reasons::PROJECTION_DUCKING) {
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
                            reasonsMap[focusInfo.usage][focusInfo.muteOrderType]
                                                            == Reasons::NAV_DUCKING) {
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
                    reasonsMap[focusInfo.usage][focusInfo.muteOrderType]
                                                == Reasons::PROJECTION_DUCKING) {
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
        std::vector<int32_t> volumes(TOTAL_BUS_COUNT, -1);
        LOG(DEBUG) << "Enter " << __func__;
        {
            auto &configManager = ::qti::audio::oem::config::AudioConfigManager::getInstance();
            ::qti::audio::oem::config::AudioConfigType
                type = configManager.getTypeFromName(DEFAULT_VOLUME_STR);
            ::qti::audio::oem::config::AudioConfigData configData;
            if (type != ::qti::audio::oem::config::AudioConfigType::AUDIO_CONFIG_MAX) {
                configManager.getAudioConfigValue(type, &configData);
                LOG(INFO) << "Volume post MUTE restored to : " << configData.defaultValue;
                for (int i = 0; i < FVM_BUS_COUNT; i++) {
                    volumes[i] = configData.defaultValue;
                }
                //override media bus volume with attenuation target
                {
                    ::qti::audio::oem::config::AudioConfigType
                            req = ::qti::audio::oem::config::AUDIO_CONFIG_ATTENUATION_TARGET;
                    ::qti::audio::oem::config::AudioConfigData configData;
                    ::qti::audio::oem::config ::AudioConfigManager::getInstance().
                                                    getAudioConfigValue(req, &configData);
                    volumes[MEDIA_BUS_INDEX] = (float)(configData.defaultValue);
                    LOG(INFO) << "Overriding media volume restore"
                        " to attenuation target: " << volumes[MEDIA_BUS_INDEX];
                }
                return volumes;
            }
        }
        int ret;
        ::aidl::qti::awx::pal_awx_param_t pal_param;
        memset(&pal_param, 0, sizeof(::aidl::qti::awx::pal_awx_param_t));

        ::aidl::qti::awx::VolumeParams params;
        params.eq_mask = bus_mask;
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
            for(int i = 0; i < TOTAL_BUS_COUNT; i++){
                if (bus_mask & (1 << (i))) {
                    int volValue = params.value[i] ;

                    if (volValue < MIN_VOLUME_VALUE || volValue > MAX_VOLUME_VALUE) {
                        LOG(ERROR) << __func__
                            << "Unsupported volume value " << "for " << busTypes[i];
                    }
                    else{
                        volumes[i] = params.value[i];
                        LOG(DEBUG) << __func__ << " " << busTypes[i]
                            << " Volume fetched successfully! ret: " << params.value[i];
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
        for (int i = 0; i < FVM_BUS_COUNT; i++) {
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
                << agci.devicePortAddress << " to "
                    << busVolumes[i] << "(index: " << agci.volumeIndex << ")";
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
        } else {
            resetCachedMediaVolume();
        }
    }

    void AudioFocusService::syncVolumeChanges() {
        for (auto volumeInfo: tempReportInfo) {
            std::vector< std::pair<std::string,
                std::pair<float, std::unordered_set<Reasons> > > > tempReportInfo;
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

    void AudioFocusService::getAllActiveReasons(std::vector<Reasons> &activeReasons) {
        for (auto &entry: registeredStreams) {
            auto focusId = entry.first;
            auto duckFocusIds = entry.second;
            if (!duckFocusIds.empty()) {
                for (auto duckFocusId : duckFocusIds) {
                    auto usage = registeredFocusCallbacks[duckFocusId].usage;
                    auto muteOrderType = registeredFocusCallbacks[duckFocusId].muteOrderType;
                    if (reasonsMap.count(usage) && reasonsMap[usage].count(muteOrderType)) {
                        auto reason = reasonsMap[usage][muteOrderType];
                        if (std::count(activeReasons.begin(), activeReasons.end(), reason) == 0) {
                            activeReasons.push_back(reason);
                        }
                    }
                }
            }
        }
        return;
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

        if (!duckAgcis.empty()) {
            LOG(INFO) << "report device gain changed";
            if (const auto &audioControlService = getAudioControlService();
                                                    audioControlService != nullptr) {
                enforceRampParameters(duckReasons);
                if (duckReasons.empty()) {
                    restoreMediaCachedVolume();
                } else {
                    {//handle thermal case
                        if (std::count(duckReasons.begin(), duckReasons.end(), Reasons::THERMAL_LIMITATION)) {
                            duckReasons.clear();
                            getAllActiveReasons(duckReasons);
                        }
                    }
                    cacheMediaVolume();
                    syncVolumeChanges();
                }
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
        float mostCriticalGain = 0.0;
        float gain;
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
            auto &muteOrdertype2 = registeredFocusCallbacks[duckingFocusId].muteOrderType;
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
                    reasons.insert(reasonsMap[usage2][muteOrdertype2]);
                } else if (gain < mostCriticalGain) {
                    reasons.clear();
                    reasons.insert(reasonsMap[usage2][muteOrdertype2]);
                    mostCriticalGain = gain;
                }
            }
            else LOG(ERROR) << "Error!! mapping doesn't exist between two usages";
        }
        tempReportInfo.push_back(std::make_pair(
                address, std::make_pair(mostCriticalGain, reasons)));

    }

    bool AudioFocusService::nonBlockingReasons(StreamType usage, MuterOderType muteOrderType) {
        //exception PROJECTION DUCKING,
        //needed to be report irrespecitive of MEDIA playback
        if (reasonsMap.find(usage) != reasonsMap.end() &&
                (reasonsMap[usage][muteOrderType] == Reasons::ADAS_DUCKING)) {
            return true;
        }
        return false;
    }


    bool AudioFocusService::isAudioOnMediaBus(int64_t focusId) {
        auto &focusInfo = registeredFocusCallbacks[focusId];
        if (auto address = focusInfo.device.address.get<AudioDeviceAddress::Tag::id>();
                !address.empty() && address == MEDIA_BUS) {
            return true;
        }
        return false;
    }

    void AudioFocusService::resetNonBlockingReasons(int64_t focusId) {
        //get usage BUS00_MEDIA focusId
        int64_t mediaFocusId = -1;
        for (auto &entry: registeredStreams) {
            auto focusId = entry.first;
            auto focusInfo = registeredFocusCallbacks[focusId];
            auto in_usage = focusInfo.usage;
            if (std::holds_alternative<std::string>(in_usage)
                    && std::get<std::string>(in_usage) == MEDIA_BUS) {
                mediaFocusId = focusId;
                break;
            }
        }
        if (mediaFocusId == -1) {
            LOG(ERROR) << "Focus Id for MEDIA BUS unavailable";
            return;
        }
        //if any non blocking reasons active, remove them from ducking media
        //except projection ducking
        auto &duckFocusIds = registeredStreams[mediaFocusId];
        for (auto itr = duckFocusIds.begin(); itr != duckFocusIds.end();) {
                auto &focusInfo = registeredFocusCallbacks[*itr];
            if (nonBlockingReasons(focusInfo.usage, focusInfo.muteOrderType)) {
                LOG(INFO) << "Resetting ADAS_DUCKING";
                itr = duckFocusIds.erase(itr);
                populateReasonsnGains(mediaFocusId);
            } else {
                ++itr;
            }
        }
        return;
    }

    void AudioFocusService::handleUpdateFocusRequest() {
        std::unordered_set<int64_t> duckFocusIds;
        for (auto entry: registeredStreams) { //loop through active focus sessions for streams
            auto activeFocusId = entry.first;
            if (registeredStreams[activeFocusId].size()) {
                populateReasonsnGains(activeFocusId);
            }
        }
        reportGainChanges();
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
                }
                if (registeredStreams[activeFocusId].size()) {
                    populateReasonsnGains(activeFocusId);
                }
            }
            registeredStreams[focusId] = duckFocusIds;
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

    void AudioFocusService::reportThermalReason(int64_t thermalFocusId, int32_t index) {
            //thermal was the one ducking the system currently, thermal's at same index
            //let's set persistent volume to thermal itself and report it.
            LOG(INFO) << "report device gain changed";
            if (const auto &audioControlService = getAudioControlService();
                                                    audioControlService != nullptr) {
                AudioGainConfigInfo agci{
                        .zoneId = 0,
                        .devicePortAddress = MEDIA_BUS,
                        .volumeIndex = index,
                };
                for (auto &entry: registeredFocusCallbacks) {
                    auto focusId = entry.first;
                    auto &focusInfo = entry.second;
                    auto in_usage = focusInfo.usage;
                    if (auto address = focusInfo.device.address.get<AudioDeviceAddress::Tag::id>();
                            !address.empty() && address == agci.devicePortAddress) {
                        focusInfo.gain = registeredFocusCallbacks[thermalFocusId].gain;
                        LOG(INFO) << "Synced duckVolume for focusID " << focusId;
                    }
                }
            std::vector<Reasons> activeReasons;
            getAllActiveReasons(activeReasons); //thermal should also captured with this call
            activeReasons.insert(activeReasons.end(), {Reasons::EXTERNAL_AMP_VOL_FEEDBACK});
            auto ret = audioControlService->reportAudioDeviceGainChanged(activeReasons, {agci});
                if (!ret.isOk()) {
                    LOG(ERROR) << __func__ << ": Unable to report audio gain changed ";
                    return;
                } else {
                    resetCachedMediaVolume();
                }
            } else {
                LOG(ERROR) << "Faild to report device gain change";
        }
    }

    void AudioFocusService::resetDuckFocusId(int64_t focusId) {
        for (auto &entry: registeredStreams) { //loop through active focus sessions
            auto &duckFocusIds = entry.second;
            auto curFocusId = entry.first;
            if (duckFocusIds.find(focusId) != duckFocusIds.end()) {
                duckFocusIds.erase(focusId);
            }
        }
    }
    void AudioFocusService::resetCachedMediaVolume() {
        cachedMediaVolume = 1.0;
    }

    bool AudioFocusService::cacheMediaVolume() {
        if (cachedMediaVolume != 1.0) {
            LOG(INFO) << "Volume Cached already, ignoring";
            return false;
        }
        for (auto &entry: registeredFocusCallbacks) {
            auto focusId = entry.first;
            auto &focusInfo = entry.second;
            auto in_usage = focusInfo.usage;
            if (auto address = focusInfo.device.address.get<AudioDeviceAddress::Tag::id>();
                    !address.empty() && address == MEDIA_BUS) {
                cachedMediaVolume = focusInfo.gain;
                LOG(INFO) << "Cached volume for focusID " << focusId << " to " 
                    << cachedMediaVolume;
                return true;
            }
        }
        LOG(ERROR) << "Couldn't cache volume for Media Bus";
        return false;
    }

    bool AudioFocusService::restoreMediaCachedVolume() {
        if (cachedMediaVolume == 1.0) {
            LOG(ERROR) << "Cached Volume restore attempted without caching Volume";
            return false;
        }
        for (auto &entry: registeredFocusCallbacks) {
            auto focusId = entry.first;
            auto &focusInfo = entry.second;
            auto in_usage = focusInfo.usage;
            if (auto address = focusInfo.device.address.get<AudioDeviceAddress::Tag::id>();
                    !address.empty() && address == MEDIA_BUS) {
                focusInfo.gain = cachedMediaVolume;
                LOG(INFO) << "Cache volume restored for focusID " << focusId << " to " 
                    << cachedMediaVolume;
                cachedMediaVolume = 1.0;
                return true;
            }
        }
        LOG(ERROR) << "Couldn't restore cached volume for Media Bus";
        return false;
    }
    void AudioFocusService::handleFocusAbandon(int64_t focusId) {
        restoreMediaCachedVolume();
        if (isMuteAbandonRequest(focusId)) {
            restoreBusVolumes();
            resetDuckFocusId(focusId);
        }
        if (globalActiveFocusSessions.find(focusId) != globalActiveFocusSessions.end())
            globalActiveFocusSessions.erase(focusId);

        if (registeredStreams.find(focusId) != registeredStreams.end()) {
            // delete the entry from the list
            // and update per session list for all other entries
            registeredStreams.erase(focusId);
            for (auto &entry: registeredStreams) { //loop through active focus sessions
                auto &duckFocusIds = entry.second;
                auto curFocusId = entry.first;
                if (duckFocusIds.find(focusId) != duckFocusIds.end()) {
                    duckFocusIds.erase(focusId);
                    populateReasonsnGains(curFocusId);
                } else if (duckFocusIds.size() > 0) {
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

    //TODO: handle for cases with external gain
    void AudioFocusService::handleVolumeChange(int64_t focusId, float gain, bool isExternalGain) {
        auto incomingGainIndex = getNearestIndex(gain);
        auto curGainIndex = getNearestIndex(registeredFocusCallbacks[focusId].gain);
        //check if incoming request is audio over MEDIA bus
        if (isAudioOnMediaBus(focusId)) {
            //thermal update needed always when volume change happens
            if (incomingGainIndex != curGainIndex) {
                resetNonBlockingReasons(focusId);
                reportGainChanges();
            }

            if (registeredStreams.count(focusId)) { //loop through active focus sessions for streams
                auto &duckFocusIds = registeredStreams[focusId];
                for (auto focusIdEntry: duckFocusIds) {
                    if (isThermalFocusId(focusIdEntry)) {
                        auto thermalGainIndex = getNearestIndex(registeredFocusCallbacks[focusIdEntry].gain);
                        if (incomingGainIndex >= thermalGainIndex) { //strictly higher should never happen
                            reportThermalReason(focusIdEntry, thermalGainIndex);
                            return;
                        }
                    }
                }
            } else {
                LOG(ERROR) << "Volume change on focusId that's not registered";
                return;
            }
        }
        if (incomingGainIndex == curGainIndex &&
            registeredFocusCallbacks[focusId].isExternalGain == isExternalGain) {
                return;
        }
        registeredFocusCallbacks[focusId].gain = gain;
        registeredFocusCallbacks[focusId].isExternalGain = isExternalGain;
        LOG(INFO) << "Volume updated for focus ID " << focusId;
        resetCachedMediaVolume();
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
                    case FocusCommand::UPDATE_FOCUS_REQUEST:
                        LOG(INFO) << "Update Focus Requested for: " << focusId;
                        focusService->handleUpdateFocusRequest();
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

    void AudioFocusService::processVolumePoints(const std::vector<std::string> &points) {
        std::map<int32_t, int32_t> volumeMap;
        int32_t index, gain;
        LOG(INFO) << "AMPERE Volume Curve:";
        for (auto point: points) {
            size_t p = point.find(',');
            if (p != std::string::npos) {
                index = std::stoi(point.substr(0, p));
                gain = std::stoi(point.substr(p + 1));
                volumeMap[gain] = index;
                LOG(INFO) << index << " : " << gain;
            }
        }
        globalVolumeMap = volumeMap;
        return;
    }

    void AudioFocusService::parseVolumeProfile() {
        const std::optional<::android::audio::policy::configuration::Volumes>
                                            volumeInfo(volProfileRead(externalVolumeConfiguration));
        if (volumeInfo.has_value() && volumeInfo->hasReference()) {
            const std::vector<::android::audio::policy::configuration::Reference>
                references = volumeInfo->getReference();
            for (auto reference: references) {
                if (reference.hasName() && reference.getName() == "DEFAULT_VOLUME_STEPS_CURVE") {
                    if (reference.hasPoint()) {
                        const std::vector<std::string> points = reference.getPoint();
                        processVolumePoints(points);
                    }
                    break;
                }
            }
        }
        if (globalVolumeMap.empty()) {
            LOG(INFO) << "Falling back to QC volume curve for audio priority management";
            globalVolumeMap = volumeMap;
        }
        return ;
    }

    static std::string getXmlAttribute(const xmlNode *cur, const char *attribute) {
        auto xmlValue = make_xmlUnique(
                xmlGetProp(cur, reinterpret_cast<const xmlChar*>(attribute)));
        if (xmlValue == nullptr) {
            return "";
        }
        std::string value(reinterpret_cast<const char*>(xmlValue.get()));
        return value;
    }
    StreamType getUsageFromStr(std::string usageType, std::string usageStr) {
        auto &usageMap = AudioFocusService::usageMap;
        StreamType streamType = AudioUsage::INVALID;
        auto usageTypeIt = usageMap.find(usageType);
        if (usageTypeIt != usageMap.end()) {
            auto usageStrIt = usageTypeIt->second.find(usageStr);
            if (usageStrIt != usageTypeIt->second.end()) {
                streamType = usageStrIt->second;
            } else {
                LOG(ERROR) << "Can't find usageStr " << usageStr;
            }
        } else {
            LOG(ERROR) << "Cant' find usageType " << usageType;
        }
        return streamType;
    }

    void parseDuckingRules(xmlNode* _root) {
        auto &configMap = AudioFocusService::configuration;
        for (xmlNode* _child = _root->xmlChildrenNode; _child != nullptr; _child = _child->next) {
            if (!xmlStrcmp(_child->name, reinterpret_cast<const xmlChar*>("rule"))) {
                LOG(INFO) << "rule parsing";
                std::string sourceType, targetType;
                std::string source, target, duck, gain, volOverride;
                sourceType = getXmlAttribute(_child, "sourceType");
                targetType = getXmlAttribute(_child, "targetType");
                source = getXmlAttribute(_child, "source");
                target = getXmlAttribute(_child, "target");
                duck = getXmlAttribute(_child, "duck");
                gain = getXmlAttribute(_child, "gain");
                volOverride = getXmlAttribute(_child, "volumeOverride");
                if (!sourceType.empty() && !targetType.empty()) {

                    StreamType inSrcUsage = getUsageFromStr(sourceType, source);
                    StreamType runningSrcUsage = getUsageFromStr(targetType, target);

                    if (duck == "true") {
                        configMap[inSrcUsage][runningSrcUsage] = ParseParams {
                                .gain = static_cast<float>(std::stoi(gain)),
                                .vol_override = (volOverride == "true" ? true : false),
                        };
                    }
                    LOG(INFO) << "Source: " << source << " "
                        << "Target: " << target << " " << duck << " " << volOverride;
                } else {
                    LOG(ERROR) << "duck rule parsing failed";
                }
            }
        }
    }

    void parseRampMap(xmlNode* _root) {
        return;
    }

    void parseReasonsMap(xmlNode* _root) {
        auto &reasonsMap = AudioFocusService::reasonsMap;
        auto &usageMap = AudioFocusService::usageMap;
        auto &muteOrderStrtoAidl = AudioFocusService::muteOrderStrtoAidl;
        auto &reasonStrtoAidl = AudioFocusService::reasonStrtoAidl;

        for (xmlNode* _child = _root->xmlChildrenNode; _child != nullptr; _child = _child->next) {
            if (!xmlStrcmp(_child->name, reinterpret_cast<const xmlChar*>("entry"))) {
                LOG(INFO) << "entry parsing";
                std::string sourcetypeStr;
                sourcetypeStr = getXmlAttribute(_child, "sourceType");
                std::string streamTypeStr, reasonStr, muteOrderTypeStr = "NONE";
                streamTypeStr = getXmlAttribute(_child, "streamType");
                reasonStr = getXmlAttribute(_child, "reason");
                if (sourcetypeStr == "UseCase") {
                    muteOrderTypeStr = getXmlAttribute(_child, "MuteOrderType");
                }
                StreamType usage = usageMap.at(sourcetypeStr).at(streamTypeStr);
                MuterOderType muterOderType = muteOrderStrtoAidl.at(muteOrderTypeStr);
                Reasons reason = reasonStrtoAidl.at(reasonStr);
                reasonsMap[usage][muterOderType] = reason;
                LOG(INFO) << "streamTypeStr: " << streamTypeStr
                    << "muterOderType: " << muteOrderTypeStr << " reasonStr: " << reasonStr;
            }
        }
        return;
    }

    bool parseConfig(const char* xmlFilePath) {
        xmlDocPtr doc = xmlParseFile(xmlFilePath);
        if (doc == nullptr) {
            return false;
        }
        xmlNode* _root = xmlDocGetRootElement(doc);
        if (_root == nullptr) {
            return false;
        }
        if (!xmlStrcmp(_root->name,
                reinterpret_cast<const xmlChar*>("CarAudioDuckingConfiguration"))) {
            //do the parsing here
            LOG(INFO) << "Parsing CarAudioDuckingConfiguration";
            for (xmlNode* _child = _root->xmlChildrenNode; _child != nullptr;
                                                            _child = _child->next) {
                if (!xmlStrcmp(_child->name,
                        reinterpret_cast<const xmlChar*>("duckingRules"))) {
                    LOG(INFO) << "parseDuckingRules called";
                    parseDuckingRules(_child);
                } else if (!xmlStrcmp(_child->name,
                        reinterpret_cast<const xmlChar*>("rampMap"))) {
                    LOG(INFO) << "parseRampMap called";
                    parseRampMap(_child);
                } else if (!xmlStrcmp(_child->name,
                        reinterpret_cast<const xmlChar*>("reasonsMap"))) {
                    LOG(INFO) << "parseReasonsMap called";
                    parseReasonsMap(_child);
                }
            }
        }
        return true;
    }

    AudioFocusService::AudioFocusService() {

        { //parse configuration from xml
            if (parseConfig(XML_FILE_PATH)) {
                LOG(ERROR) << __func__ <<  " Parsed " << XML_FILE_PATH;
            } else {
                LOG(ERROR) << __func__ <<  " Failed to parse configuration";
            }
        }
        parseVolumeProfile();
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

    int32_t AudioFocusService::updateVolume(const int64_t focusId,
                        const float gain, const bool isExternalGain) {
        {
            std::lock_guard<std::mutex> _lock(mtx);
            if (registeredFocusCallbacks.find(focusId) == registeredFocusCallbacks.end()) {
                LOG(ERROR) << "FocusId invalid, couldn't update gain";
                return -1;
            }
            focusQueue.push(FocusAction(FocusCommand::UPDATE_VOLUME,
                                        focusId, gain, isExternalGain));
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
    int32_t AudioFocusService::updateFocusRequest(const int64_t focusId, float gain){

        {
            std::lock_guard<std::mutex> _lock(mtx);
            if (registeredFocusCallbacks.count(focusId)) {
                registeredFocusCallbacks[focusId].gain = gain;
            } else {
                LOG(ERROR) << "focusId : " << focusId << " Invalid";
                return -1;
            }
            focusQueue.push(FocusAction(FocusCommand::UPDATE_FOCUS_REQUEST, focusId));
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

    extern "C" __attribute__((visibility("default")))
    int32_t updateFocusRequest(const int64_t focusId, float gain){
        if (focusId < 0) {
            LOG(INFO) << "Invalid focus Id" << focusId;
            return -1;
        }
        return AudioFocusService::getFocusServiceInstance()
                            .updateFocusRequest(focusId, gain);
    }

}

