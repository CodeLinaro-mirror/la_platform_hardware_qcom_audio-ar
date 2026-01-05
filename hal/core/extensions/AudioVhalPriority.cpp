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
#include <aidl/android/hardware/automotive/vehicle/BnVehicleCallback.h>
#include <aidl/android/hardware/automotive/vehicle/IVehicleCallback.h>

#include <aidl/android/hardware/automotive/vehicle/IVehicle.h>
#include <aidl/android/hardware/automotive/vehicle/SetValueRequest.h>
#include <aidl/android/hardware/automotive/vehicle/SetValueRequests.h>

#include <aidl/android/hardware/automotive/vehicle/GetValueRequest.h>
#include <aidl/android/hardware/automotive/vehicle/SetValueRequests.h>

#include <aidl/android/hardware/automotive/vehicle/SetValueResults.h>
#include <aidl/android/hardware/automotive/vehicle/GetValueResults.h>
#include <aidl/android/hardware/automotive/vehicle/StatusCode.h>
#include <LargeParcelableBase.h>
#include <chrono>
#include <thread>

#define MIN_NIGHT_MODE 0
#define MAX_NIGHT_MODE 1
#define MIN_DOOR_VALUE 0
#define MAX_DOOR_VALUE 1
#define MIN_RADIO_MUTE_VALUE 0
#define MAX_RADIO_MUTE_VALUE 1
#define DEFAULT_GAIN_VALUE -4000
#define MIN_THERMAL_VALUE 0
#define MAX_THERMAL_VALUE 120
#define DP_TO_MDB 100
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

using GetValueCallbackFunc = std::function<void(VhalClientResult<std::unique_ptr<android::frameworks::automotive::vhal::IHalPropValue>>)>;
std::shared_ptr<aidl::android::hardware::automotive::vehicle::IVehicle> mVhal;
using ::aidl::android::hardware::automotive::vehicle::IVehicle;
using ::android::automotive::car_binder_lib::LargeParcelableBase;

#define SOMEIP_CONNECTION_STATE  0x0A3D + 0x20000000 + 0x01000000+ 0x00200000  // VehiclePropertyGroup:VENDOR, VehicleArea:GLOBAL, VehiclePropertyType:BOOLEAN
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
FocusHandler g_focusHandler(PRIORITY_LIB);

std::mutex mtx, vhalCallbackMutex;
std::condition_variable cv, cvTemp;
bool isDeratingEnabled = false;
bool isTemperatureInfoAvailable = false;
static qti::audio::core::FocusSession focusSessionInfo;
static float mediaGain = 0.0;

//to be set by HAL as part of any volume update
extern "C" __attribute__((visibility("default"))) void setMediaGain(float gain) {
    mediaGain = gain;
    return;
}

float getCurrentMediaGain() {
    return mediaGain;
}
static int uuid = 0;
std::shared_ptr<IVehicle> getVhalService() {
    if (mVhal == nullptr) {
        std::string serviceName =
                std::string().append(IVehicle::descriptor).append("/default");

        if (!AServiceManager_isDeclared(serviceName.c_str())) {
            LOG(ERROR) <<"IVehicle not declared, exiting";
            return nullptr;
        }

        AIBinder* binder = AServiceManager_waitForService(serviceName.c_str());
        if (binder != nullptr) {
            ndk::SpAIBinder spBinder(binder);
            std::shared_ptr<IVehicle> service =
                                IVehicle::fromBinder(spBinder);
            if (service != nullptr) {
                mVhal = service;
                LOG(INFO) << "Connected to IVehicle service";
            } else {
                LOG(ERROR) << "Can't connect to IVehicle service";
            }
        } else {
            LOG(ERROR) << "Failed to get service handle for " << serviceName;
        }
    }
    return mVhal;
}

int getUUID() {
    return (++uuid) % INT_MAX;
}

void triggerFocusRequest(float volLimit) {

    if (volLimit < MIN_VOLUME) {
        volLimit = MIN_VOLUME;
    } else if (volLimit > MAX_VOLUME) {
        //shouldn't hit this case
        volLimit = MAX_VOLUME;
    }
    LOG(INFO) << "Limiting Volume to due to THERMAL LIMITATION to : " << volLimit;
    if (focusSessionInfo.FocusId != -1) {
        g_focusHandler.updateFocusRequest(focusSessionInfo.FocusId, volLimit);
    } else {
        qti::audio::core::FocusInfo focusInfo;
        focusInfo.usage = "DEVICE_TEMPERATURE_STATUS";
        focusInfo.isExternalGain = true;
        focusInfo.gain = volLimit;
        g_focusHandler.requestFocus(focusInfo, &focusSessionInfo.FocusId);
    }
}

void triggerFocusAbandon() {
    LOG(INFO) << "Disabling Thermal Volume Restriction";
    g_focusHandler.abandonFocus(focusSessionInfo.FocusId);
    focusSessionInfo.FocusId = -1;
}

static int32_t OTWTemperature; //to be updated as part of OTW intimation 
                        //from VHAL
static bool isOTWStatusSet = false;

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
        updateFocusRequestFunc = (UpdateFocusRequestType)dlsym(mHandle, "updateFocusRequest");
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

int FocusHandler::updateFocusRequest(const int64_t focusId, float gain) {
    if (updateFocusRequestFunc) {
        return updateFocusRequestFunc(focusId, gain);
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
    LOG(DEBUG) << __func__ << ": Enter " ;
    ::qti::audio::oem::config::AudioConfigType req = AUDIO_CONFIG_ATTENUATION_TARGET ;
    ::qti::audio::oem::config::AudioConfigData configData;
    ::qti::audio::oem::config::AudioConfigManager::getInstance().getAudioConfigValue(req,&configData);
    LOG(DEBUG) << "Attenuation value returned from getAudioConfigValue is: " << configData.defaultValue;
    return configData.defaultValue;
}

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

bool parseOTWStatus(std::vector<int32_t> &values) {
    if (values.size() < DeviceTemperatureIndex::MAX_ENTRIES) {
        LOG(ERROR) << "Invalid size for DEVICE_TEMPERATURE_STATUS payload";
        return false;
    }
    return values[DeviceTemperatureIndex::AMP1_GLOBAL_OTW] == 0x1 ||
            values[DeviceTemperatureIndex::AMP2_GLOBAL_OTW] == 0x1 ||
            values[DeviceTemperatureIndex::EXT_AMP1_GLOBAL_OTW] == 0x1 ||
            values[DeviceTemperatureIndex::EXT_AMP2_GLOBAL_OTW] == 0x1;
}

int32_t parseOTWTemperature(std::vector<int32_t> &values) {
    if (values.size() < DeviceTemperatureIndex::MAX_ENTRIES) {
        LOG(ERROR) << "Invalid size for DEVICE_TEMPERATURE_STATUS payload";
        return -1;
    }
    int32_t temperature = 0xFF;
    if (values[DeviceTemperatureIndex::AMP1_GLOBAL_OTW]) {
        temperature = values[DeviceTemperatureIndex::AMP1_GLOBAL_TEMPERATURE];
    } else if (values[DeviceTemperatureIndex::AMP2_GLOBAL_OTW]) {
        temperature = values[DeviceTemperatureIndex::AMP2_GLOBAL_TEMPERATURE];
    } else if (values[DeviceTemperatureIndex::EXT_AMP1_GLOBAL_OTW]) {
        temperature = values[DeviceTemperatureIndex::EXT_AMP1_GLOBAL_TEMPERATURE];
    } else if (values[DeviceTemperatureIndex::EXT_AMP2_GLOBAL_OTW]) {
        temperature = values[DeviceTemperatureIndex::EXT_AMP2_GLOBAL_TEMPERATURE];
    }
    return temperature;
}


std::shared_ptr<IVhalClient> getVhalClient() {
    static std::shared_ptr<IVhalClient> vhalClient;
    if (vhalClient == nullptr) {
        vhalClient = IVhalClient::create();
    }
    return vhalClient;
}

ndk::ScopedAStatus subscribeVhalProp();

void VhalPropCallback(VhalClientResult<std::unique_ptr<android::frameworks::automotive::vhal::IHalPropValue>> result) {
    if (!result.ok()) {
        LOG(ERROR) << "VHAL callback error: " << result.error().message();
        return;
    }
    if(result.value() == nullptr) {
        return;
    }
    if (result.value()->getPropId() == SOMEIP_CONNECTION_STATE) {
        if (result.value()->getInt32Values()[0] == true)
        {
            subscribeVhalProp();
        }
    }
    if (result.value()->getPropId() == MuteRadioOrderByAAMId) {
        if (result.value()->getInt32Values().size() < 1) {
            LOG(ERROR) << "Invalid Radio Mute getInt32Values size, empty value :" << result.value()->getInt32Values().size();
            return;
        } else {
            if (result.value()->getInt32Values()[0] < MIN_RADIO_MUTE_VALUE || result.value()->getInt32Values()[0] > MAX_RADIO_MUTE_VALUE) {
                LOG(ERROR) << "Invalid Radio Mute value :" << result.value()->getInt32Values()[0];
            }
            else{
                LOG(DEBUG) << "Event Notify: New Radio Mute event received. Val:" << result.value()->getInt32Values()[0];
                handler_radioMute(result.value()->getInt32Values()[0]);
            }
        }
    }
    if (result.value()->getPropId() == ThermalPropertyId) {
        auto tempInfo = result.value()->getInt32Values();
        if (tempInfo.empty()) {
            LOG(ERROR) << "Thermal Event Received, Payload Empty";
            return;
        } else if (!isDeratingEnabled) {
            //change to verbose
            LOG(INFO) << "Thermal Event Received, Device Temperature:" << tempInfo[0];
            for (auto it: tempInfo) {
                LOG(INFO) << it << " ";
            }
            isOTWStatusSet = parseOTWStatus(tempInfo);
            if (isOTWStatusSet) {
                isDeratingEnabled = true;
                cv.notify_one();
            }
        }
    }
}

ndk::ScopedAStatus subscribeVhalProp() {
    std::vector<aidl::android::hardware::automotive::vehicle::SubscribeOptions> options;
    std::shared_ptr<AudioVHALListener> pAudioListener = std::make_shared<AudioVHALListener>();
    auto vhalClient = getVhalClient();
    if (vhalClient == nullptr) {
        LOG(ERROR) << "Vehicle HAL getService returned NULL.  Exiting.";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    } else {
        auto ThermalPropertyClient = vhalClient->getSubscriptionClient(pAudioListener);
        options = {
            {
                .propId = ThermalPropertyId,
                .areaIds = {},
            }
        };
        if (auto status = ThermalPropertyClient->subscribe(options); !status.ok()) {
            LOG(ERROR) << "Subscription to ThermalPropertyId property faied!!";
        } else {
            LOG(INFO) << "Subscribed to ThermalPropertyId property";
            auto requestThermalPropValue = vhalClient->createHalPropValue(ThermalPropertyId);

            auto Thermalcallback = std::make_shared<IVhalClient::GetValueCallbackFunc>(VhalPropCallback);
            vhalClient->getValue(*requestThermalPropValue,Thermalcallback);
        }

        auto MuteRadioOrderClient = vhalClient->getSubscriptionClient(pAudioListener);
        options = {
            {
                .propId = MuteRadioOrderByAAMId,
                .areaIds = {},
            }
        };
        if (auto status = MuteRadioOrderClient->subscribe(options); !status.ok()) {
            LOG(ERROR) << "Subscription to MuteRadioOrderByAAMId property failed!!";
        } else {
            LOG(INFO) << "Subscribed to MuteRadioOrderByAAMId property";
            auto requestMutePropValue = vhalClient->createHalPropValue(MuteRadioOrderByAAMId);

            auto Mutecallback = std::make_shared<IVhalClient::GetValueCallbackFunc>(VhalPropCallback);
            vhalClient->getValue(*requestMutePropValue,Mutecallback);
        }
    }
    return ndk::ScopedAStatus::ok();
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
        if (value->getPropId() == ThermalPropertyId) {
            auto tempInfo = value->getInt32Values();
            if (tempInfo.empty()) {
                LOG(ERROR) << "Thermal Event Received, Payload Empty";
                goto exit;
            } else if (!isDeratingEnabled) {
                //change to verbose
                LOG(INFO) << "Thermal Event Received, Device Temperature:" << tempInfo[0];
                for (auto it: tempInfo) {
                    LOG(INFO) << it << " ";
                }
                isOTWStatusSet = parseOTWStatus(tempInfo);
                if (isOTWStatusSet) {
                    isDeratingEnabled = true;
                    cv.notify_one();
                }
            }
        }
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
        if (value->getPropId() == SOMEIP_CONNECTION_STATE) {
            LOG(INFO) << __func__ << "PropId matching with SOMEIP_CONNECTION_STATE";
             if (value->getInt32Values().size() < 1) {
                LOG(ERROR) << "Invalid SOMEIP_CONNECTION_STATE  size, empty value :" << value->getInt32Values().size();
                goto exit;
            } else {
                LOG(DEBUG) << "Event Notify: New SOMEIP_CONNECTION_STATE event received. Val:" << value->getInt32Values()[0];
                if (value->getInt32Values()[0]) {
                    subscribeVhalProp();
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
        if (it != FocusHandler::focusIdMap.end())
            it->second.push_back(focusSessionInfo.FocusId);
        else
            FocusHandler::focusIdMap.insert({"RADIO_AAM_MUTE_ORDER", {focusSessionInfo.FocusId}});
        }
    else {
        LOG(DEBUG) << "RADIO_AAM_MUTE_ORDER property is deactivated, calling Focus Service to unduck" ;

        const auto& it = FocusHandler::focusIdMap.find("RADIO_AAM_MUTE_ORDER");
        if (it != FocusHandler::focusIdMap.end() && !it->second.empty()) {
            focusSessionInfo.FocusId = it->second.back(); // Retrieving the last Id value
            it->second.pop_back();
        } else {
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
    if (getAttenuationTarget() != -1.0f) {
        gainValue = getAttenuationTarget();
    }
    focusInfo.gain = gainValue;

    if(vhal_data != nullptr){
        if(vhal_data->nightModeValue == 1) {
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
            else if (vhal_data->driverDoor == 0 && vhal_data->frontPassengerDoor==0 && vhal_data->rearLeftDoor==0 && vhal_data->rearRightDoor==0) {
                LOG(DEBUG) << "All doors are closed, calling Focus Service to Unduck" ;
                const auto& it = FocusHandler::focusIdMap.find("NIGHT_MODE");
                if (it != FocusHandler::focusIdMap.end() && !it->second.empty()) {
                    while (!it->second.empty()) {
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
                } else {
                    LOG(DEBUG) << "No NIGHT_MODE Focus Session to unduck";
                    goto exit;
                }
            }
        }
        else {
            LOG(DEBUG) << "NIGHT_MODE value is not set" ;
        }
    }
exit:
    LOG(DEBUG) << __func__ << ": Exit ";
}
#endif

std::pair<int32_t, bool> processVehiclePropValue(std::unique_ptr<IHalPropValue> &propValueResult) {
    int32_t propId = -1, areadId = -1;
    propId = propValueResult->getPropId();
    areadId = propValueResult->getAreaId();
    LOG(DEBUG) << __func__ << ": PropId : " << propId << ": areaId : " << areadId;

    bool isOTWStatusSet = false;
    int32_t OTWTemperature = 0xFF;
    if (propId != ThermalPropertyId) {
        LOG(ERROR) << __func__ << "Callback PropId not matching with THERMAL PROP ID";
    } else {
        std::vector<int32_t> tempInfo = propValueResult->getInt32Values();
        for (auto it: tempInfo) {
            LOG(INFO) << it << " ";
        }
        isOTWStatusSet = parseOTWStatus(tempInfo);
        if (isOTWStatusSet) {
            if (auto temperature = parseOTWTemperature(tempInfo); temperature != 0xFF) {
                OTWTemperature = temperature;
            } else {
                LOG(ERROR) << "OTW set, but failed to get OTW temperature";
                isOTWStatusSet = false;
            }
        }
    }
    return {OTWTemperature, isOTWStatusSet};
}

std::pair<int32_t, bool> getOTWInfo() {
    auto vhalClient = getVhalClient();
    auto propValue = vhalClient->createHalPropValue(ThermalPropertyId);
    auto vhalClientResult = vhalClient->getValueSync(*propValue);
    if (vhalClientResult.ok()) {
        auto propValueResult = std::move(vhalClientResult.value());
        return processVehiclePropValue(propValueResult);
    } else {
        LOG(ERROR) << "Error fetching temperature info from VHAL";
        return {0xFF, false};
    }
}

int32_t getCnfAttachTimeMs() {
    ::qti::audio::oem::config::AudioConfigType req = AUDIO_CONFIG_THERMAL_ATTACK_TIME;
    ::qti::audio::oem::config::AudioConfigData configData;
    ::qti::audio::oem::config::AudioConfigManager::getInstance().getAudioConfigValue(req, &configData);
    LOG(VERBOSE) << "CNF_ATTACK_TIME_MS: " << configData.defaultValue;
    return configData.defaultValue;
}

int32_t getCnfReleaseTimeMs() {
    ::qti::audio::oem::config::AudioConfigType req = AUDIO_CONFIG_THERMAL_RELEASE_TIME;
    ::qti::audio::oem::config::AudioConfigData configData;
    ::qti::audio::oem::config::AudioConfigManager::getInstance().getAudioConfigValue(req, &configData);
    LOG(VERBOSE) << "CNF_RELEASE_TIME_MS: " << configData.defaultValue;
    return configData.defaultValue;
}

int32_t getCnfDeltaStepMdb() {
    ::qti::audio::oem::config::AudioConfigType req = AUDIO_CONFIG_THERMAL_DELTA_STEP;
    ::qti::audio::oem::config::AudioConfigData configData;
    ::qti::audio::oem::config::AudioConfigManager::getInstance().getAudioConfigValue(req, &configData);
    LOG(VERBOSE) << "CNF_DELTA_STEP_MDB: " << configData.defaultValue*DP_TO_MDB;
    return configData.defaultValue*DP_TO_MDB;
}

void thermalDeratingLoop() {
    LOG(INFO) << "threadLoop started for Thermal Derating";
    std::unique_lock _lock(mtx);
    while (true) {
        LOG(INFO) << "Waiting for OTW";
        cv.wait(_lock, []{ return isDeratingEnabled;});
        int32_t curTemp, prevTemp;
        bool isOTWStatusSet;
        int32_t volumeLimit = getCurrentMediaGain();
        std::pair<int32_t, bool> OTWInfo = getOTWInfo();
        curTemp = OTWInfo.first;
        isOTWStatusSet = OTWInfo.second;
        if (isOTWStatusSet) {
            triggerFocusRequest(volumeLimit);
            prevTemp = curTemp;
        } else {
            triggerFocusAbandon();
            LOG(ERROR) << "OTW set, but failed to get OTW temperature, "
                "derating not triggered";
            isDeratingEnabled = false;
            break;
        }
        //there's already a request to block volume increase
        //sleep for sometime to assess volume changes
        std::this_thread::sleep_for(std::chrono::milliseconds(getCnfAttachTimeMs()));
        while (true) {
            OTWInfo = getOTWInfo(); //trigger a call to VHAL, blocked till get completes
                                    //update OTW status and OTW temperature
            curTemp = OTWInfo.first;
            isOTWStatusSet = OTWInfo.second;
            if (!isOTWStatusSet) {
                triggerFocusAbandon();
                isDeratingEnabled = false;
                break;
            }
            if (curTemp - prevTemp >= 0) {
                volumeLimit -= getCnfDeltaStepMdb();
                triggerFocusRequest(volumeLimit);
                std::this_thread::sleep_for(std::chrono::milliseconds(getCnfAttachTimeMs()));
            } else {
                volumeLimit += getCnfDeltaStepMdb();
                if (volumeLimit >= 0) {
                    triggerFocusAbandon();
                    isDeratingEnabled = false;
                    break;
                }
                triggerFocusRequest(volumeLimit);
                std::this_thread::sleep_for(std::chrono::milliseconds(getCnfReleaseTimeMs()));
            }
            prevTemp = curTemp;
        }
    }
    LOG(DEBUG) << __func__ << ": Exit ";
    return;
}
std::unique_ptr<std::thread> mThread = std::make_unique<std::thread>(&thermalDeratingLoop);

extern "C" __attribute__((visibility("default")))int priority_deinit(void) {
    //join the derating threadLoop
    return 0;
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
    pVnet = getVhalClient();
    if (pVnet == nullptr) {
        LOG(ERROR) << "Vehicle HAL getService returned NULL.  Exiting.";
        return EXIT_FAILURE;
    } else {
        auto subscriptionClient = pVnet->getSubscriptionClient(pAudioListener);

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

        std::vector<aidl::android::hardware::automotive::vehicle::SubscribeOptions> options;
        options = {
            {
                .propId = SOMEIP_CONNECTION_STATE,
                .areaIds = {},
            }
        };
        if (auto status = subscriptionClient->subscribe(options); !status.ok()) {
            LOG(ERROR) << "Subscription to SOMEIP_CONNECTION_STATE property faied!!";
        } else {
            LOG(INFO) << "Subscribed to SOMEIP_CONNECTION_STATE property";
            auto propValue = pVnet->createHalPropValue(SOMEIP_CONNECTION_STATE);
            auto callbackPtr = std::make_shared<GetValueCallbackFunc>(VhalPropCallback);
            pVnet->getValue(*propValue, callbackPtr);
            subscribeVhalProp();
        }
    }
    return EXIT_SUCCESS;
}
