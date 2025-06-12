/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
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
#include "include/extensions/AudioVhalPriority.h"
#include "PalApi.h"
#include <include/extensions/AudioVHALListener.h>
#include <include/extensions/AudioHalFocusManager.h>
#include <include/extensions/BusDuckConfig.h>
#include <include/extensions/ThermalConfig.h>

#define XML_FILE_PATH "/vendor/etc/audio_ar/duck_configuration.xml"
#define TEMPERATURE_XML_PATH "/vendor/etc/audio_ar/thermal_config.xml"

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

#define MIN_NIGHT_MODE 0
#define MAX_NIGHT_MODE 1
#define MIN_DOOR_VALUE 0
#define MAX_DOOR_VALUE 1
#define MIN_RADIO_MUTE_VALUE 0
#define MAX_RADIO_MUTE_VALUE 1
#define DEFAULT_GAIN_VALUE -4000
#define MIN_THERMAL_VALUE 0
#define MAX_THERMAL_VALUE 120
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

#ifdef ENABLE_VHAL_TEST_WITH_KITCHENSINK
const int32_t ThermalPropertyId = 356517121; //VehicleProperty::HVAC_FAN_DIRECTION
const int32_t MuteRadioOrderByAAMId = 289408269; //VehicleProperty::HVAC_STEERING_WHEEL_HEAT
#ifdef ENABLE_QCOM_VHAL_NIGHTMODE
const int32_t NightModePropertyId = 339739916; //VehicleProperty::HVAC_SIDE_MIRROR_HEAT
const int32_t DriverDoorPropertyId = 373295872; //VehicleProperty::DOOR_POS
const int32_t FrontPassengerDoorPropertyId = 356517131; //VehicleProperty::HVAC_SEAT_TEMPERATURE
const int32_t RearLeftDoorPropertyId = 356517139; //VehicleProperty::HVAC_SEAT_VENTILATION
const int32_t RearRightDoorPropertyId = 322964416; //VehicleProperty::WINDOW_POS
#endif

#else
const int32_t ThermalPropertyId = 557909548;
const int32_t MuteRadioOrderByAAMId = 555747163;
#ifdef ENABLE_QCOM_VHAL_NIGHTMODE
const int32_t NightModePropertyId = 339739916;
const int32_t DriverDoorPropertyId = 373295872;
const int32_t FrontPassengerDoorPropertyId = 356517131;
const int32_t RearLeftDoorPropertyId = 356517139;
const int32_t RearRightDoorPropertyId = 322964416;
#endif

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
#ifdef ENABLE_QCOM_VHAL_NIGHTMODE
struct vhal_data* Vhal_Data::vhal_data = nullptr;
#endif

// Helper to subscribe to VHal notifications
bool subscribeToVHal(ISubscriptionClient* client, int32_t paramId){
    // VehicleProperty propertyId) {
    LOG(DEBUG) << __func__ << ": Enter " ;

    // Register for vehicle state change callbacks we care about
    std::vector<aidl::android::hardware::automotive::vehicle::SubscribeOptions> options = {
            {
                    .propId = paramId,
                    .areaIds = {},
            }
    };
    auto result = client->subscribe(options);
    if (!result.ok()) {
        LOG(ERROR) << "VHAL subscription for property " << paramId
                     << "error" << result.error().message();
        return false;
    }
    else
    {
        LOG(DEBUG) << "VHAL subscription for propertyId = " << paramId << " Success";
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
#ifdef ENABLE_QCOM_VHAL_NIGHTMODE
    struct vhal_data* vhal_data = Vhal_Data::vhal_get_params();
#endif
    for(const auto& value : values) {
        int32_t propId = value->getPropId();
        LOG(DEBUG) << __func__ << ": PropId : " << propId;
        int32_t area = value->getAreaId();
        LOG(DEBUG) << __func__ << ": areaId : " << area;

#ifdef ENABLE_QCOM_VHAL_NIGHTMODE
        if (value->getPropId() == NightModePropertyId) {
            if (value->getInt32Values().size() < 1) {
                LOG(ERROR) << "Invalid NIGHT_MODE getFloatValues size, empty value :" << value->getInt32Values().size();
                goto exit;
            } else {
                LOG(DEBUG) << "Event Notify: New NIGHT_MODE event received. Val:" << value->getInt32Values()[0];
                vhal_data->nightModeValue = value->getInt32Values()[0];
                Vhal_Data::vhal_set_params(vhal_data);
                handler_vhal();
            }
        }
        if (value->getPropId() == DriverDoorPropertyId) {
            if (value->getInt32Values().size() < 1) {
                LOG(ERROR) << "Invalid Driver Door getInt32Values size, empty value :" << value->getInt32Values().size();
                goto exit;
            } else {
                if (value->getInt32Values()[0] < MIN_DOOR_VALUE || value->getInt32Values()[0] > MAX_DOOR_VALUE) {
                    LOG(ERROR) << "Invalid Driver Door value :" << value->getInt32Values()[0];
                }
                else{
                    LOG(DEBUG) << "Event Notify: New Driver Door event received. Val:" << value->getInt32Values()[0];
                    vhal_data->driverDoor = value->getInt32Values()[0];
                    Vhal_Data::vhal_set_params(vhal_data);
                    handler_vhal();
                }
            }
        }
        if (value->getPropId() == FrontPassengerDoorPropertyId) {
            if (value->getInt32Values().size() < 1) {
                LOG(ERROR) << "Invalid Front Passenger Door getInt32Values size, empty value :" << value->getInt32Values().size();
                goto exit;
            } else {
                if (value->getInt32Values()[0] < MIN_DOOR_VALUE || value->getInt32Values()[0] > MAX_DOOR_VALUE) {
                    LOG(ERROR) << "Invalid Front Passenger Door value :" << value->getInt32Values()[0];
                }
                else{
                    LOG(DEBUG) << "Event Notify: New Front Passenger Door event received. Val:" << value->getInt32Values()[0];
                    vhal_data->frontPassengerDoor = value->getInt32Values()[0];
                    Vhal_Data::vhal_set_params(vhal_data);
                    handler_vhal();
                }
            }
        }if (value->getPropId() == RearLeftDoorPropertyId) {
            if (value->getInt32Values().size() < 1) {
                LOG(ERROR) << "Invalid Rear Left Door getInt32Values size, empty value :" << value->getInt32Values().size();
                goto exit;
            } else {
                if (value->getInt32Values()[0] < MIN_DOOR_VALUE || value->getInt32Values()[0] > MAX_DOOR_VALUE) {
                    LOG(ERROR) << "Invalid Rear Left Door value :" << value->getInt32Values()[0];
                }
                else{
                    LOG(DEBUG) << "Event Notify: New Rear Left Door event received. Val:" << value->getInt32Values()[0];
                    vhal_data->rearLeftDoor = value->getInt32Values()[0];
                    Vhal_Data::vhal_set_params(vhal_data);
                    handler_vhal();
                }
            }
        }
        if (value->getPropId() == RearRightDoorPropertyId) {
            if (value->getInt32Values().size() < 1) {
                LOG(ERROR) << "Invalid Rear Right Door getInt32Values size, empty value :" << value->getInt32Values().size();
                goto exit;
            } else {
                if (value->getInt32Values()[0] < MIN_DOOR_VALUE || value->getInt32Values()[0] > MAX_DOOR_VALUE) {
                    LOG(ERROR) << "Invalid Rear Right Door value :" << value->getInt32Values()[0];
                }
                else{
                    LOG(DEBUG) << "Event Notify: New Rear Right Door event received. Val:" << value->getInt32Values()[0];
                    vhal_data->rearRightDoor = value->getInt32Values()[0];
                    Vhal_Data::vhal_set_params(vhal_data);
                    handler_vhal();
                }
            }
        }
#endif
        if (value->getPropId() == MuteRadioOrderByAAMId) {
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
        if (value->getPropId() == ThermalPropertyId) {
            if (value->getInt32Values().size() < 1) {
                LOG(ERROR) << "Invalid Thermal Property size, empty value :" << value->getInt32Values().size();
                goto exit;
            } else {
                if (value->getInt32Values()[0] < MIN_THERMAL_VALUE || value->getInt32Values()[0] > MAX_THERMAL_VALUE) {
                    LOG(ERROR) << "Invalid Thermal Property value :" << value->getInt32Values()[0];
                }
                else{
                    LOG(DEBUG) << "Event Notify: New Thermal Property event received. Val:" << value->getInt32Values()[0];
                    handler_thermal(value->getInt32Values()[0]);
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

#ifdef ENABLE_QCOM_VHAL_NIGHTMODE
extern "C" __attribute__((visibility("default"))) void handler_vhal(){

    LOG(DEBUG) << __func__ << ": Enter " ;
    struct vhal_data* vhal_data = Vhal_Data::vhal_get_params();
    qti::audio::core::FocusSession focusSessionInfo;
    qti::audio::core::FocusInfo focusInfo;
    focusInfo.usage = "NIGHT_MODE";
    focusInfo.isExternalGain = true;
    float gainValue = DEFAULT_GAIN_VALUE;
    if(getAttenuationTarget() != -1.0f){
        gainValue = getAttenuationTarget();
    }
    focusInfo.gain = gainValue;

    if(vhal_data != nullptr){
        if(vhal_data->nightModeValue == 1){
            LOG(DEBUG) << "Night mode value is set" ;

            if(vhal_data->driverDoor == 1 || vhal_data->frontPassengerDoor==1 || vhal_data->rearLeftDoor==1 || vhal_data->rearRightDoor==1){
                LOG(DEBUG) << "Atleast 1 door is open, calling Focus Service to Duck to volume:" << gainValue ;
                if (!g_focusHandler.isValid()) {
                    LOG(ERROR) << "Failed to initialize g_focusHandler";
                    return;
                }
                g_focusHandler.requestFocus(focusInfo, &focusSessionInfo.FocusId);
                LOG(INFO) << "Focus Id: " << focusSessionInfo.FocusId;
                const auto& it = FocusHandler::focusIdMap.find("NIGHT_MODE");
                if(it != FocusHandler::focusIdMap.end())
                    it->second.push_back(focusSessionInfo.FocusId);
                else
                    FocusHandler::focusIdMap.insert({"NIGHT_MODE", {focusSessionInfo.FocusId}});
            }
            else if(vhal_data->driverDoor == 0 && vhal_data->frontPassengerDoor==0 && vhal_data->rearLeftDoor==0 && vhal_data->rearRightDoor==0){
                LOG(DEBUG) << "All doors are closed, calling Focus Service to Unduck" ;
                const auto& it = FocusHandler::focusIdMap.find("NIGHT_MODE");
                if(it != FocusHandler::focusIdMap.end() && !it->second.empty()) {
                    while(!it->second.empty()){
                        focusSessionInfo.FocusId = it->second.back();
                        LOG(DEBUG) << __func__ << ": Releasing NIGHT_MODE audio focus: " << focusSessionInfo.FocusId;
                        if (!g_focusHandler.isValid()) {
                            LOG(ERROR) << "Failed to initialize g_focusHandler";
                            return;
                        }
                        g_focusHandler.abandonFocus(focusSessionInfo.FocusId);
                        focusSessionInfo.FocusId = 0;
                        it->second.pop_back();
                    }
                }
                else{
                    LOG(DEBUG) << "No NIGHT_MODE Focus Session to unduck";
                    goto exit;
                }
            }
        }
        else{
            LOG(DEBUG) << "NIGHT_MODE value is not set" ;
        }
    }
exit:
    LOG(DEBUG) << __func__ << ": Exit ";
}
#endif

extern "C" __attribute__((visibility("default"))) void handler_thermal(int32_t temperature){
    LOG(DEBUG) << __func__ << ": Enter " ;
    qti::audio::core::FocusSession focusSessionInfo;
    float gainValue;
    qti::audio::core::FocusInfo focusInfo;
    focusInfo.usage = "DEVICE_TEMPERATURE_STATUS";
    focusInfo.isExternalGain = true;

    ThermalParser parser; // to parse thermal conditions XML
    FILE* file = NULL;
    file = fopen(TEMPERATURE_XML_PATH, "r");
    if(!file){
        LOG(ERROR) << __func__ <<  " File not present: " << TEMPERATURE_XML_PATH;
        return;
    }
    else{
        LOG(ERROR) << __func__ <<  " File present" << TEMPERATURE_XML_PATH;
    }
    fclose(file);

    if (parser.parseConfig(TEMPERATURE_XML_PATH)) {
        const auto& conditions = parser.getConditions();
        for(const auto& condn : conditions){

            // Extract the attributes
            int tempLower = condn.tempLower;
            int tempUpper = condn.tempUpper;
            gainValue = condn.gain;
            focusInfo.gain = gainValue;

            if (temperature >= tempLower && temperature <= tempUpper) {
                if(gainValue == 0) {
                    LOG(DEBUG) << "No vol limit, unducking"; //unduck
                    const auto& it = FocusHandler::focusIdMap.find("DEVICE_TEMPERATURE_STATUS");
                    if(it != FocusHandler::focusIdMap.end() && !it->second.empty()) {
                        while( !it->second.empty()){
                            focusSessionInfo.FocusId = it->second.back(); // Retrieving the last Id value
                            LOG(DEBUG) << __func__ << ": Releasing DEVICE_TEMPERATURE_STATUS audio focus: " << focusSessionInfo.FocusId;
                            if (!g_focusHandler.isValid()) {
                                LOG(ERROR) << "Failed to initialize g_focusHandler";
                                return;
                            }
                            g_focusHandler.abandonFocus(focusSessionInfo.FocusId);
                            focusSessionInfo.FocusId = 0;
                            it->second.pop_back();
                        }
                    }
                    else{
                        LOG(ERROR) << "No DEVICE_TEMPERATURE_STATUS Focus Session to unduck";
                        goto exit;
                    }
                }
                else {
                    LOG(DEBUG) << "Ducking to Vol limit =" << gainValue;//duck
                    if (!g_focusHandler.isValid()) {
                        LOG(ERROR) << "Failed to initialize g_focusHandler";
                        return;
                    }
                    g_focusHandler.requestFocus(focusInfo, &focusSessionInfo.FocusId);
                    LOG(INFO) << "Focus Id for DEVICE_TEMPERATURE_STATUS " << focusSessionInfo.FocusId;
                    const auto& it = FocusHandler::focusIdMap.find("DEVICE_TEMPERATURE_STATUS");
                    if(it != FocusHandler::focusIdMap.end())
                        it->second.push_back(focusSessionInfo.FocusId);
                    else
                        FocusHandler::focusIdMap.insert({"DEVICE_TEMPERATURE_STATUS", {focusSessionInfo.FocusId}});
                }
            }
        }
    }
exit:
    LOG(DEBUG) << __func__ << ": Exit ";
    return;
}

extern "C" __attribute__((visibility("default")))int priority_init(void)
{
    LOG(DEBUG) << __func__ << ": Enter " ;
    // Construct our async helper object
    std::shared_ptr<AudioVHALListener> pAudioListener = std::make_shared<AudioVHALListener>();
#ifdef ENABLE_QCOM_VHAL_NIGHTMODE
    //Create NIGHT_MODE VHAL DATA struct
    Vhal_Data::init();
#endif
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
        }
        else
        {
            LOG(DEBUG) << "Register for RADIO_AAM_MUTE_ORDER done.";
        }
#ifdef ENABLE_QCOM_VHAL_NIGHTMODE
        if (!subscribeToVHal(subscriptionClient.get(), NightModePropertyId)) {
            LOG(ERROR) << "Didn't register for NIGHT_MODE , Exiting.";
        }
        else
        {
            LOG(DEBUG) << "regiter for NIGHT_MODE done.";
        }

        if (!subscribeToVHal(subscriptionClient.get(), DriverDoorPropertyId)) {
            LOG(ERROR) << "Didn't register for Driver Door notification, Exiting.";
        }
        else
        {
            LOG(DEBUG) << "Register for Driver Door done.";
        }

        if (!subscribeToVHal(subscriptionClient.get(), FrontPassengerDoorPropertyId)) {
            LOG(ERROR) << "Didn't register for Front Passenger Door notification, Exiting.";
        }
        else
        {
            LOG(DEBUG) << "Register for Front Passenger door done.";
        }

        if (!subscribeToVHal(subscriptionClient.get(), RearLeftDoorPropertyId)) {
            LOG(ERROR) << "Didn't register for Rear Left Door notification, Exiting.";
        }
        else
        {
            LOG(DEBUG) << "Register for Rear Left Door done.";
        }

        if (!subscribeToVHal(subscriptionClient.get(), RearRightDoorPropertyId)) {
            LOG(ERROR) << "Didn't register for Rear Right Door notification, Exiting.";
        }
        else
        {
            LOG(DEBUG) << "Register for Rear Right Door done.";
        }
#endif
        if (!subscribeToVHal(subscriptionClient.get(), ThermalPropertyId)) {
            LOG(ERROR) << "Didn't register for Thermal Property, Exiting.";
        }
        else
        {
            LOG(DEBUG) << "Register for Thermal Property done.";
        }
    }
    return EXIT_SUCCESS;
}
