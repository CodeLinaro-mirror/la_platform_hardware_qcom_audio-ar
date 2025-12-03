/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#define LOG_TAG "AudioDiagnostics"
#include <IVhalClient.h>
#include <aidl/android/hardware/automotive/vehicle/IVehicle.h>
#include <aidl/android/hardware/automotive/vehicle/SetValueRequest.h>
#include <aidl/android/hardware/automotive/vehicle/SetValueRequests.h>

#include <aidl/android/hardware/automotive/vehicle/SetValueResults.h>
#include <aidl/android/hardware/automotive/vehicle/GetValueResults.h>
#include <aidl/android/hardware/automotive/vehicle/StatusCode.h>

#include <aidl/android/hardware/automotive/vehicle/BnVehicleCallback.h>
#include <aidl/android/hardware/automotive/vehicle/IVehicleCallback.h>
#include <android-base/thread_annotations.h>

/** Sound card state */
typedef enum card_status_t {
    CARD_STATUS_OFFLINE = 0,
    CARD_STATUS_ONLINE,
    CARD_STATUS_STANDBY,
    CARD_STATUS_NONE,
} card_status_t;

typedef struct {
    int card;
    int fd;
    card_status_t status;
} sndcard_t;

struct SpeakerIndex {
    enum : int32_t {
        FRONT_LEFT = 0x0,
        FRONT_RIGHT = 0x1,
        REAR_LEFT = 0x2,
        REAR_RIGHT = 0x3,
    };
};

struct SpeakerState {
    enum : int32_t {
        NOT_CONNECTED = 0x0,
        OK = 0x1,
        NOK = 0x2,
    };
};

struct AudioCompIndex {
    enum : int32_t {
        RESERVED = 0x0,
        AMP1 = 0x1,
        AMP2 = 0x2,
        A2B_LINK1 = 0x3,
        A2B_LINK2 = 0x4,
        EXT_AMP1 = 0x5,
        EXT_AMP2 = 0x6,
        RESERVED2 = 0x7,
    };
};

struct AudioCompStatus {
    enum : int32_t {
        OK = 0x0,
        FAILURE = 0x1,
        DO_NOT_CARE = 0xFF,
    };
};


class AudioDiagnostics {
        std::thread mThread;
        void monitorThreadLoop();
        static std::shared_ptr<aidl::android::hardware::automotive::vehicle::IVehicle> mVhal;
        bool isDiagnosticMonitorEnabled = true;
    public:
        static std::shared_ptr<aidl::android::hardware::automotive::vehicle::IVehicle> getVhalService();
        static AudioDiagnostics& InitDiagnosticSevice() {
            static const auto kAudioDiagnosticService = []() {
                std::unique_ptr<AudioDiagnostics> audioExt{new AudioDiagnostics()};
                return std::move(audioExt);
            }();
            return *(kAudioDiagnosticService.get());
        }
        AudioDiagnostics();
        ~AudioDiagnostics();
};

class SpeakerGroupAvailCallback final :
      public android::frameworks::automotive::vhal::ISubscriptionCallback {
public:
    void onPropertyEvent(const std::vector<std::unique_ptr<android::frameworks::automotive::vhal::IHalPropValue>>& values) override;
    void onPropertySetError(
            [[maybe_unused]] const std::vector<android::frameworks::automotive::vhal::HalPropError>&
                    errors) override {
        LOG(ERROR) << "onPropertySetError: failed to set VHAL property";
        return;
    }
};


class AudioFailureDetectCallback final :
      public android::frameworks::automotive::vhal::ISubscriptionCallback {
public:
    void onPropertyEvent(const std::vector<std::unique_ptr<android::frameworks::automotive::vhal::IHalPropValue>>& values) override;
    void onPropertySetError(
            [[maybe_unused]] const std::vector<android::frameworks::automotive::vhal::HalPropError>&
                    errors) override {
        LOG(ERROR) << "onPropertySetError: failed to set VHAL property";
        return;
    }
};

class AudioVhalAvailibilityCallback final :
      public android::frameworks::automotive::vhal::ISubscriptionCallback {
public:
    void onPropertyEvent(const std::vector<std::unique_ptr<android::frameworks::automotive::vhal::IHalPropValue>>& values) override;
    void onPropertySetError(
            [[maybe_unused]] const std::vector<android::frameworks::automotive::vhal::HalPropError>&
                    errors) override {
        LOG(ERROR) << "onPropertySetError: failed to set VHAL property";
        return;
    }

};
