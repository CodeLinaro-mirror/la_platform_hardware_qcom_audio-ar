/*
* Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#define LOG_TAG "AHAL_PRIORITY_EXTENSTION_QTI"
#define LOG_NDDEBUG 0
#include <android-base/logging.h>
#include <cutils/properties.h>
#include <cutils/str_parms.h>
#include <string>
#include <errno.h>
#include <log/log.h>
#include <include/extensions/AudioVhalPriority.h>
#include "PalApi.h"
#include <include/extensions/AudioVHALListener.h>
#include <include/extensions/AudioHalFocusManager.h>
#include <include/extensions/BusDuckConfig.h>
#define XML_FILE_PATH "/vendor/etc/audio_ar/duck_configuration.xml"

#include <aidl/android/hardware/automotive/vehicle/SubscribeOptions.h>
#include <aidl/android/hardware/automotive/vehicle/VehicleProperty.h>
#include <android-base/strings.h>
#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <cutils/properties.h>
#include <utils/Errors.h>
#include <utils/Log.h>
#include <utils/StrongPointer.h>
#include <signal.h>
#include <stdio.h>
#include <aidl/alliance/hardware/automotive/audiocontrol/internal/IAudioControlInternal.h>
#include "include/extensions/AudioConfig.h"

#include <cstdlib>

#define MIN_RADIO_MUTE_VALUE 0
#define MAX_RADIO_MUTE_VALUE 1
#define DEFAULT_GAIN_VALUE -4000
// Define the macro for the Priority Focus library name
#define PRIORITY_LIB "libaudiohalpriorityextn.so"

namespace {
using aidl::android::hardware::automotive::vehicle::VehicleProperty;
using android::base::EqualsIgnoreCase;
using android::frameworks::automotive::vhal::ISubscriptionClient;
using android::frameworks::automotive::vhal::IVhalClient;
using ::android::hardware::automotive::vehicle::toInt;
using ::android::frameworks::automotive::vhal::VhalClientResult;
using ::android::frameworks::automotive::vhal::IHalPropValue;
using ::aidl::android::hardware::automotive::vehicle::RawPropValues;
using aidl::android::media::audio::common::AudioDeviceType;
using namespace ::qti::audio::oem::config;

#ifndef ENABLE_VHAL_TEST_WITH_KITCHENSINK
#define ENABLE_VHAL_TEST_WITH_KITCHENSINK
const VehicleProperty MuteRadioOrderByAAMId = VehicleProperty::HVAC_STEERING_WHEEL_HEAT;

#else
const VehicleProperty MuteRadioOrderByAAMId = VehicleProperty::HVAC_STEERING_WHEEL_HEAT;

#endif //ENABLE_VHAL_TEST_WITH_KITCHENSINK

void* FocusHandler::mHandle = nullptr;
std::map< std::string, std::vector<int64_t>> FocusHandler::focusIdMap;

FocusHandler::FocusHandler(const std::string& libName) {
    // Load the shared library
    mHandle = dlopen(libName.c_str(), RTLD_NOW);

    if (mHandle == nullptr) {
        const char *error = dlerror();
        LOG(ERROR) << __func__ << ": dlopen failed for " << libName.c_str() << " " << dlerror();
        return;
    }
    else{
        // Retrieve function pointers
        requestFocusFunc = (RequestFocusType)dlsym(mHandle, "requestFocus");
        abandonFocusFunc = (AbandonFocusType)dlsym(mHandle, "abandonFocus");
        const char* dlsym_error = dlerror();
        if (dlsym_error) {
            LOG(ERROR) << __func__ << ": dlsym failed for Focus functions " << dlerror();
            dlclose(mHandle);
            mHandle = nullptr;
            return;
        }
    }
}

FocusHandler::~FocusHandler() {
    if (mHandle) {
        dlclose(mHandle);
    }
    focusIdMap.clear();
}

int FocusHandler::requestFocus(const qti::audio::core::FocusInfo& focusInfo, int64_t* focusId) {
    if (requestFocusFunc) {
        return requestFocusFunc(focusInfo, focusId);
    }
    return -1;
}

bool FocusHandler::isValid() {
    return mHandle != nullptr;
}

int FocusHandler::abandonFocus(int64_t focusId) {
    if (abandonFocusFunc) {
        return abandonFocusFunc(focusId);
    }
    return -1;
}

float getAttenuationTarget(){
    // return -4000;
    LOG(DEBUG) << __func__ << ": Enter " ;
    ::qti::audio::oem::config::AudioConfigType req = AUDIO_CONFIG_ATTENUATION_TARGET ;
    ::qti::audio::oem::config::AudioConfigData configData;
    ::qti::audio::oem::config::AudioConfigManager::getInstance().getAudioConfigValue(req,&configData);
    LOG(DEBUG) << "Attenuation value returned from getAudioConfigValue is: " << configData.defaultValue;

    return configData.defaultValue;
}

FocusHandler g_focusHandler(PRIORITY_LIB);

// Helper to subscribe to VHal notifications
bool subscribeToVHal(ISubscriptionClient* client, VehicleProperty propertyId) {
    LOG(DEBUG) << __func__ << ": Enter " ;

    // Register for vehicle state change callbacks we care about
    std::vector<aidl::android::hardware::automotive::vehicle::SubscribeOptions> options = {
            {
                    .propId = static_cast<int32_t>(propertyId),
                    .areaIds = {},
            }
    };
    auto result = client->subscribe(options);
    if (!result.ok()) {
        LOG(ERROR) << "VHAL subscription for property " << static_cast<int32_t>(propertyId)
                     << "error" << result.error().message();
        return false;
    }
    else
    {
        LOG(DEBUG) << "VHAL subscription for propertyId = " << static_cast<int32_t>(propertyId) << " Success";
    }
    return true;
}
}

void AudioVHALListener::onPropertyEvent(const std::vector<std::unique_ptr<IHalPropValue>>& values) {
    LOG(DEBUG) << __func__ << ": Enter " ;
    if (values.empty()) {
        LOG(ERROR) << __func__ << ": Received empty property values vector";
        return;
    }

    for(const auto& value : values) {
        int32_t propId = value->getPropId();
        LOG(DEBUG) << __func__ << ": PropId : " << propId;
        int32_t area = value->getAreaId();
        LOG(DEBUG) << __func__ << ": areaId : " << area;


        if (value->getPropId() == static_cast<int32_t>(MuteRadioOrderByAAMId)) {
            if (value->getInt32Values().size() < 1) {
                LOG(ERROR) << "Invalid Radio Mute getInt32Values size, empty value :" << value->getInt32Values().size();
                goto exit;
            } else {
                if (value->getInt32Values()[0] < MIN_RADIO_MUTE_VALUE || value->getInt32Values()[0] > MAX_RADIO_MUTE_VALUE) {
                    LOG(ERROR) << "Invalid Radio Mute value :" << value->getInt32Values()[0];
                }
                else{
                    LOG(DEBUG) << "Event Notify: New Radio Mute event received. Val:" << value->getInt32Values()[0];
                    handler_radioMute(value->getInt32Values()[0]);
                }
            }
        }
    }
exit:
    LOG(DEBUG) << __func__ << ": Exit ";
}

extern "C" __attribute__((visibility("default"))) void handler_radioMute(int32_t radio_mute_byAAM_value){
    LOG(DEBUG) << __func__ << ": Enter " ;

    qti::audio::core::FocusSession focusSessionInfo;
    qti::audio::core::FocusInfo focusInfo;
    focusInfo.usage = "RADIO_AAM_MUTE_ORDER";
    focusInfo.isExternalGain = true;
    float gainValue = DEFAULT_GAIN_VALUE;
    if(getAttenuationTarget() != -1.0f){
        gainValue = getAttenuationTarget();
    }
    focusInfo.gain = gainValue;
    LOG(DEBUG) << "Gain volume:" << gainValue;

    if(radio_mute_byAAM_value == 1){
        LOG(DEBUG) << "RADIO_AAM_MUTE_ORDER property is activated, calling Focus Service to Duck Entertainment to volume:" << gainValue;
        if (!g_focusHandler.isValid()) {
            LOG(ERROR) << "Failed to initialize g_focusHandler";
            return;
        }
        g_focusHandler.requestFocus(focusInfo, &focusSessionInfo.FocusId);
        LOG(INFO) << "Focus Id: " << focusSessionInfo.FocusId;
        const auto& it = FocusHandler::focusIdMap.find("RADIO_AAM_MUTE_ORDER");
        if(it != FocusHandler::focusIdMap.end())
            it->second.push_back(focusSessionInfo.FocusId);
        else
            FocusHandler::focusIdMap.insert({"RADIO_AAM_MUTE_ORDER", {focusSessionInfo.FocusId}});
        }
    else{
        LOG(DEBUG) << "RADIO_AAM_MUTE_ORDER property is deactivated, calling Focus Service to unduck" ;

        const auto& it = FocusHandler::focusIdMap.find("RADIO_AAM_MUTE_ORDER");
        if(it != FocusHandler::focusIdMap.end() && !it->second.empty()) {
            focusSessionInfo.FocusId = it->second.back(); // Retrieving the last Id value
            it->second.pop_back();
        }
        else{
            LOG(ERROR) << "No RADIO_AAM_MUTE_ORDER Focus Session to unduck";
            goto exit;
        }
        LOG(DEBUG) << __func__ << ": Releasing RADIO_AAM_MUTE_ORDER audio focus: " << focusSessionInfo.FocusId;
        if (!g_focusHandler.isValid()) {
            LOG(ERROR) << "Failed to initialize g_focusHandler";
            return;
        }
        g_focusHandler.abandonFocus(focusSessionInfo.FocusId);
        focusSessionInfo.FocusId = 0;
    }
exit:
    LOG(DEBUG) << __func__ << ": Exit ";
}

extern "C" __attribute__((visibility("default")))int priority_init(void)
{
    LOG(DEBUG) << __func__ << ": Enter " ;
    // Construct our async helper object

    std::shared_ptr<AudioVHALListener> pAudioListener = std::make_shared<AudioVHALListener>();

    // Connect to the Vehicle HAL so we can monitor state
    std::shared_ptr<IVhalClient> pVnet;
    LOG(INFO) << "Connecting to Vehicle HAL";
    pVnet = IVhalClient::create();
    if (pVnet == nullptr) {
        LOG(ERROR) << "Vehicle HAL getService returned NULL.  Exiting.";
        return EXIT_FAILURE;
    } else {
        auto subscriptionClient = pVnet->getSubscriptionClient(pAudioListener);
        // Register for vehicle state change callbacks we care about
        // Changes in these values are what will trigger a reconfiguration.
        if (!subscribeToVHal(subscriptionClient.get(), MuteRadioOrderByAAMId)) {
            LOG(ERROR) << "Didn't register for RADIO_AAM_MUTE_ORDER notification, Exiting.";
            return EXIT_FAILURE;
        }
        else
        {
            LOG(ERROR) << "Register for RADIO_AAM_MUTE_ORDER done.";
        }
    }
    return EXIT_SUCCESS;
}
