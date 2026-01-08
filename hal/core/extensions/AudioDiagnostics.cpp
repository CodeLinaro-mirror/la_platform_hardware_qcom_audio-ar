/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

/*
* Copyright (C) 2021 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <cstdint>
#define LOG_TAG "AudioDiagnostics"
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <list>
#include <sys/eventfd.h>
#include <thread>
#include <android-base/logging.h>
#include <include/extensions/AudioDiagnostics.h>
#include <LargeParcelableBase.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

using ::aidl::android::hardware::automotive::vehicle::IVehicle;
using ::android::automotive::car_binder_lib::LargeParcelableBase;
using android::frameworks::automotive::vhal::IVhalClient;
using android::frameworks::automotive::vhal::VhalClientResult;

#define SNDCARD_PATH "/sys/kernel/snd_card/card_state"
#define MAX_SLEEP_RETRY 100

#define AUDIO_AVAILABILITY_ON 0x049D + 0x20000000 + 0x01000000 + 0x00200000 // VehiclePropertyGroup:VENDOR, VehicleArea:GLOBAL, VehiclePropertyType:BOOLEAN
#define AUDIO_NOTI_DETECT_FAILURE_V2 0x09F3 + 0x20000000 + 0x01000000 + 0x00410000 // VehiclePropertyGroup:VENDOR, VehicleArea:GLOBAL, VehiclePropertyType:INT32_VEC
#define SPEAKER_GROUP_AVAILABILITY 0x09F5 + 0x20000000 + 0x01000000 + 0x00410000 // VehiclePropertyGroup:VENDOR, VehicleArea:GLOBAL, VehiclePropertyType:INT32_VEC
#define VUC_SPEAKER_GROUP_AVAILABILITY 0x0A21 + 0x20000000 + 0x01000000 + 0x00410000 // VehiclePropertyGroup:VENDOR, VehicleArea:GLOBAL, VehiclePropertyType:INT
#define VUC_SPEAKER_GROUP_AVAILABILITY_SIZE 4
#define AUDIO_NOTI_DETECT_FAILURE_V2_SIZE 8
#define SOMEIP_CONNECTION_STATE  0x0A3D + 0x20000000 + 0x01000000+ 0x00200000  // VehiclePropertyGroup:VENDOR, VehicleArea:GLOBAL, VehiclePropertyType:BOOLEAN

static int fd = -1, efd = -1;
static int exit_thread = 0; //by default exit thread is made false
static bool card_status = true, speaker_status = true, audio_comp_status = true;
static int uuid = 0;
using GetValueCallbackFunc = std::function<void(VhalClientResult<std::unique_ptr<android::frameworks::automotive::vhal::IHalPropValue>>)>;

std::shared_ptr<aidl::android::hardware::automotive::vehicle::IVehicle> AudioDiagnostics::mVhal;

ndk::ScopedAStatus subscribeDiagProp() ;
ndk::ScopedAStatus updateVHAL() ;
ndk::ScopedAStatus updateSpeakerGroupAvailability(std::vector<int32_t> speakerInfo);
int getUUID() {
    return (++uuid) % INT_MAX;
}

std::shared_ptr<IVhalClient> getVhalClient() {
    static std::shared_ptr<IVhalClient> vhalClient;
    if (vhalClient == nullptr) {
        vhalClient = IVhalClient::create();
    }
    return vhalClient;
}
void SomeipCallback(VhalClientResult<std::unique_ptr<android::frameworks::automotive::vhal::IHalPropValue>> result) {
    if (!result.ok()) {
        LOG(ERROR) << "VHAL callback error: " << result.error().message();
        return;
    }
    if(result.value() == nullptr) {
        return;
    }
    if (result.value()->getPropId() == SOMEIP_CONNECTION_STATE) {
        if (result.value()->getInt32Values()[0]== true) {
            subscribeDiagProp();
        }
    }
    if (result.value()->getPropId() == AUDIO_NOTI_DETECT_FAILURE_V2) {
        std::vector<int32_t> audioCompInfo = result.value()->getInt32Values();
        for (auto it: audioCompInfo) {
            LOG(INFO) << __func__ << it;
        }
        if (audioCompInfo.size() != AUDIO_NOTI_DETECT_FAILURE_V2_SIZE) {
            LOG(ERROR) << "Invalid AUDIO_NOTI_DETECT_FAILURE_V2_SIZE getInt32Values size: "
                << audioCompInfo.size();
        } else {
            if (audioCompInfo[AudioCompIndex::AMP1] != AudioCompStatus::OK &&
                audioCompInfo[AudioCompIndex::AMP2] != AudioCompStatus::OK &&
                audioCompInfo[AudioCompIndex::A2B_LINK1] != AudioCompStatus::OK &&
                audioCompInfo[AudioCompIndex::A2B_LINK2] != AudioCompStatus::OK &&
                audioCompInfo[AudioCompIndex::EXT_AMP1] != AudioCompStatus::OK &&
                audioCompInfo[AudioCompIndex::EXT_AMP2] != AudioCompStatus::OK) {
                LOG(ERROR) << "AUDIO UNAVAILABLE, reporting to VHAL";
                speaker_status = false;
            } else {
                LOG(INFO) << "Audio Components are in connected state";
                speaker_status = true;
            }
            updateVHAL();

        }
    }
    if (result.value()->getPropId() == VUC_SPEAKER_GROUP_AVAILABILITY) {
        std::vector<int32_t> speakerInfo = result.value()->getInt32Values();
        for (auto it: speakerInfo) {
            LOG(INFO) << __func__ << it;
        }
        {
            LOG(INFO) << "Updating SPEAKER_GROUP_AVAILABILITY";
            updateSpeakerGroupAvailability(speakerInfo);
        }
        if (speakerInfo.size() != VUC_SPEAKER_GROUP_AVAILABILITY_SIZE) {
            LOG(ERROR) << "Invalid VUC_SPEAKER_GROUP_AVAILABILITY_SIZE getInt32Values size: "
                << speakerInfo.size();
        } else {
            if (speakerInfo[SpeakerIndex::FRONT_LEFT] != SpeakerState::OK &&
                speakerInfo[SpeakerIndex::FRONT_RIGHT] != SpeakerState::OK &&
                speakerInfo[SpeakerIndex::REAR_LEFT] != SpeakerState::OK &&
                speakerInfo[SpeakerIndex::REAR_RIGHT] != SpeakerState::OK) {
                LOG(ERROR) << "AUDIO UNAVAILABLE, reporting to VHAL";
                audio_comp_status = false;
            } else {
                LOG(INFO) << "SPEAKERs are in connected state";
                audio_comp_status = true;
            }
            updateVHAL();
        }

    }

}

ndk::ScopedAStatus updateSpeakerGroupAvailability(std::vector<int32_t> speakerInfo) {
    auto vhalClient = getVhalClient();
    auto requestPropValue = vhalClient->createHalPropValue(SPEAKER_GROUP_AVAILABILITY);
    requestPropValue->setInt32Values(speakerInfo);
    auto callback = [](VhalClientResult<void> result){
        if (result.ok()) {
            LOG(INFO) << "setValue successful for SPEAKER_GROUP_AVAILABILITY";
        } else {
            LOG(ERROR) << "setValue failed for SPEAKER_GROUP_AVAILABILITY";
        }
        return;
    };
    std::shared_ptr<IVhalClient::SetValueCallbackFunc>
        setValueCallbackFunc = std::make_shared<IVhalClient::SetValueCallbackFunc>(callback);
    vhalClient->setValue(*requestPropValue, setValueCallbackFunc);
    return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus subscribeDiagProp() {
    std::vector<aidl::android::hardware::automotive::vehicle::SubscribeOptions> options;

    // Track per-property subscription success across calls
    static bool audioFailureSubscribed   = false;
    static bool speakerGroupSubscribed   = false;

    auto vhalClient = getVhalClient();
    if (vhalClient == nullptr) {
        LOG(ERROR) << "Vehicle HAL getService returned NULL.  Exiting.";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    if (!audioFailureSubscribed) {
        auto audioFailureDetectClient =
            vhalClient->getSubscriptionClient(std::make_shared<AudioFailureDetectCallback>());

        options = {
            {
                .propId = AUDIO_NOTI_DETECT_FAILURE_V2,
                .areaIds = {},
            }
        };

        if (auto status = audioFailureDetectClient->subscribe(options); !status.ok()) {
            LOG(ERROR) << "Subscription to AUDIO_NOTI_DETECT_FAILURE_V2 property failed!!";
            audioFailureSubscribed = false; // keep false, so we retry next time
        } else {
            LOG(INFO) << "Subscribed to AUDIO_NOTI_DETECT_FAILURE_V2 property";
            audioFailureSubscribed = true;

            auto requestAudioFailurePropValue =
                vhalClient->createHalPropValue(AUDIO_NOTI_DETECT_FAILURE_V2);
            auto AudioFailurecallback =
                std::make_shared<IVhalClient::GetValueCallbackFunc>(SomeipCallback);
            vhalClient->getValue(*requestAudioFailurePropValue, AudioFailurecallback);
        }
    } else {
        LOG(INFO) << "AUDIO_NOTI_DETECT_FAILURE_V2 already subscribed; skipping.";
    }

    if (!speakerGroupSubscribed) {
        auto speakerGroupAvailClient =
            vhalClient->getSubscriptionClient(std::make_shared<SpeakerGroupAvailCallback>());

        options = {
            {
                .propId = VUC_SPEAKER_GROUP_AVAILABILITY,
                .areaIds = {},
            }
        };

        if (auto status = speakerGroupAvailClient->subscribe(options); !status.ok()) {
            LOG(ERROR) << "Subscription to VUC_SPEAKER_GROUP_AVAILABILITY property failed!!";
            speakerGroupSubscribed = false; // keep false, so we retry next time
        } else {
            LOG(INFO) << "Subscribed to VUC_SPEAKER_GROUP_AVAILABILITY property";
            speakerGroupSubscribed = true;

            auto requestSpeakerPropValue =
                vhalClient->createHalPropValue(VUC_SPEAKER_GROUP_AVAILABILITY);
            auto SpeakerGroupCallback =
                std::make_shared<IVhalClient::GetValueCallbackFunc>(SomeipCallback);
            vhalClient->getValue(*requestSpeakerPropValue, SpeakerGroupCallback);
        }
    } else {
        LOG(INFO) << "VUC_SPEAKER_GROUP_AVAILABILITY already subscribed; skipping.";
    }

    if (audioFailureSubscribed && speakerGroupSubscribed) {
        LOG(DEBUG) << "All diag subscriptions completed; future calls will skip.";
    } else {
        LOG(WARNING) << "Some diag subscriptions failed; only failed ones will retry next time.";
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus updateVHAL() {
    bool audio_status = card_status && speaker_status && audio_comp_status;
    auto vhalClient = getVhalClient();
    auto requestPropValue = vhalClient->createHalPropValue(AUDIO_AVAILABILITY_ON);
    requestPropValue->setInt32Values(std::vector<int32_t>{
                                static_cast<int32_t>(audio_status)});
    auto callback = [audio_status](VhalClientResult<void> result){
        if (result.ok()) {
            LOG(INFO) << "setValue with audio_status: " << audio_status <<
                " successful for AUDIO_AVAILABILITY_ON";
        } else {
            LOG(ERROR) << "setValue failed for AUDIO_AVAILABILITY_ON, status: " << audio_status;
        }
        return;
    };
    std::shared_ptr<IVhalClient::SetValueCallbackFunc>
        setValueCallbackFunc = std::make_shared<IVhalClient::SetValueCallbackFunc>(callback);
    vhalClient->setValue(*requestPropValue, setValueCallbackFunc);
    return ndk::ScopedAStatus::ok();
}

void AudioDiagnostics::monitorThreadLoop()
{
    struct pollfd *poll_fds;
    int rv = 0;
    char buf[12];
    int sound_card_status = 0;
    int tries = MAX_SLEEP_RETRY;

    card_status_t status = CARD_STATUS_NONE;
    while(--tries) {
        if (exit_thread == 1)
            break;
        if ((fd = open(SNDCARD_PATH, O_RDWR)) < 0) {
            LOG(ERROR) << "Open failed snd sysfs node";
        }
        else {
            LOG(INFO) << "snd sysfs node open successful";
            break;
        }
        usleep(500000);
    }
    efd = eventfd(0, EFD_CLOEXEC);
    if (fd == -1 || efd == -1)
        goto Done;
    poll_fds = (struct pollfd*) calloc(2, sizeof(struct pollfd));
    if(NULL == poll_fds) {
        LOG(INFO) << "Memory allocation failed";
        goto Done;
    }

    while(isDiagnosticMonitorEnabled) {
        memset(buf , 0 ,sizeof(buf));
        read(fd, buf, 1);
        sscanf(buf , "%d", &sound_card_status);
        card_status = (sound_card_status == 1);
        lseek(fd,0L,SEEK_SET);

        poll_fds[0].fd = fd;
        poll_fds[0].events = POLLERR | POLLPRI;
        poll_fds[0].revents = 0;
        poll_fds[1].fd = efd;
        poll_fds[1].events = POLLERR | POLLIN;
        poll_fds[1].revents = 0;

        LOG(INFO) << "waiting sys_notify event\n";
        if (( rv = poll(poll_fds, 2, -1)) < 0 ) {
            LOG(ERROR) << "snd sysfs node poll error\n";
        } else if ((poll_fds[0].revents & POLLPRI)) {
            lseek(poll_fds[0].fd,0L,SEEK_SET);
            read(poll_fds[0].fd, buf, 1);
            sscanf(buf , "%d", &sound_card_status);
            LOG(INFO) << "sound card status: " << sound_card_status;
            if (sound_card_status == 0) {
               status = CARD_STATUS_OFFLINE;
               //inform vhal for AUDIO DOWN
               card_status = false;
               updateVHAL();
            }
            else if (sound_card_status == 1) {
                status = CARD_STATUS_ONLINE;
               //inform vhal for AUDIO UP
               card_status = true;
               updateVHAL();
            }
            else if (sound_card_status == 2)
                status = CARD_STATUS_STANDBY;
            else if (sound_card_status == 3)
                break;
       } else if((poll_fds[1].revents & POLLIN)) {
            uint64_t eval;
            read(poll_fds[1].fd, &eval, 8);
            if (eval == 1) {
                free(poll_fds);
                poll_fds = NULL;
                close(efd);
                close(fd);
                break;
            }
       }
    }
Done:
    return;
}


std::shared_ptr<IVehicle> AudioDiagnostics::getVhalService() {
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

AudioDiagnostics::AudioDiagnostics() {
    mThread = std::thread(&AudioDiagnostics::monitorThreadLoop, this);
    LOG(INFO) << "AudioDiagnostics init done.";

    //init VHAL service
    auto mVhalClient = getVhalClient();
    if (mVhalClient == nullptr) {
        LOG(ERROR) << "VHAL client creation failed!!";
        return;
    }

    std::vector<aidl::android::hardware::automotive::vehicle::SubscribeOptions> options;
    auto audioSomeipStatusClient = mVhalClient->getSubscriptionClient(
                            std::make_shared<AudioVhalAvailibilityCallback>());
    options = {
        {
            .propId = SOMEIP_CONNECTION_STATE,
            .areaIds = {},
        }
    };
    if (auto status = audioSomeipStatusClient->subscribe(options); !status.ok()) {
        LOG(ERROR) << "Subscription to SOMEIP_CONNECTION_STATE property faied!!";
    } else {
        LOG(INFO) << "Subscribed to SOMEIP_CONNECTION_STATE property";
        auto propValue = mVhalClient->createHalPropValue(SOMEIP_CONNECTION_STATE);
        auto callbackPtr = std::make_shared<GetValueCallbackFunc>(SomeipCallback);
        mVhalClient->getValue(*propValue, callbackPtr);
        subscribeDiagProp();
    }
    updateVHAL();
    return;
}

AudioDiagnostics::~AudioDiagnostics() {
   uint64_t eval = 1;
   exit_thread = 1;
   if(efd != -1)
      write(efd, &eval, 8);
   mThread.join();
}

extern "C" __attribute__((visibility("default")))
void AudioDiagnosticsInit()  {
    AudioDiagnostics::InitDiagnosticSevice();
}

void AudioFailureDetectCallback::onPropertyEvent(
    const std::vector<std::unique_ptr<
            android::frameworks::automotive::vhal::IHalPropValue>>& values) {
    LOG(DEBUG) << __func__ << ": Enter " ;
    for(const auto& value : values) {
        int32_t propId = value->getPropId();
        int32_t areadId = value->getAreaId();
        LOG(DEBUG) << __func__ << ": PropId : " << propId << ": areaId : " << areadId;

        if (propId != AUDIO_NOTI_DETECT_FAILURE_V2) {
            LOG(ERROR) << __func__ << "PropId not matching with AUDIO_NOTI_DETECT_FAILURE_V2";
        } else {
            std::vector<int32_t> audioCompInfo = value->getInt32Values();
            for (auto it: audioCompInfo) {
                LOG(INFO) << __func__ << it;
            }
            if (audioCompInfo.size() != AUDIO_NOTI_DETECT_FAILURE_V2_SIZE) {
                LOG(ERROR) << "Invalid AUDIO_NOTI_DETECT_FAILURE_V2_SIZE getInt32Values size: "
                    << audioCompInfo.size();
            } else {
                if (audioCompInfo[AudioCompIndex::AMP1] != AudioCompStatus::OK &&
                    audioCompInfo[AudioCompIndex::AMP2] != AudioCompStatus::OK &&
                    audioCompInfo[AudioCompIndex::A2B_LINK1] != AudioCompStatus::OK &&
                    audioCompInfo[AudioCompIndex::A2B_LINK2] != AudioCompStatus::OK &&
                    audioCompInfo[AudioCompIndex::EXT_AMP1] != AudioCompStatus::OK &&
                    audioCompInfo[AudioCompIndex::EXT_AMP2] != AudioCompStatus::OK) {
                    LOG(ERROR) << "AUDIO UNAVAILABLE, reporting to VHAL";
                    speaker_status = false;
                } else {
                    LOG(INFO) << "Audio Components are in connected state";
                    speaker_status = true;
                }
                updateVHAL();
            }
        }
    }
    return;
}

/*------------------------------------------------------------------------------------------------------------*/

void SpeakerGroupAvailCallback::onPropertyEvent(
    const std::vector<std::unique_ptr<
            android::frameworks::automotive::vhal::IHalPropValue>>& values) {
    LOG(DEBUG) << __func__ << ": Enter " ;
    for(const auto& value : values) {
        int32_t propId = value->getPropId();
        int32_t areadId = value->getAreaId();
        LOG(DEBUG) << __func__ << ": PropId : " << propId << ": areaId : " << areadId;
        if (propId != VUC_SPEAKER_GROUP_AVAILABILITY) {
            LOG(ERROR) << __func__ << "PropId not matching with VUC_SPEAKER_GROUP_AVAILABILITY";
        } else {
            std::vector<int32_t> speakerInfo = value->getInt32Values();
            for (auto it: speakerInfo) {
                LOG(INFO) << __func__ << it;
            }
            {
                LOG(INFO) << "Updating SPEAKER_GROUP_AVAILABILITY";
                updateSpeakerGroupAvailability(speakerInfo);
            }
            if (speakerInfo.size() != VUC_SPEAKER_GROUP_AVAILABILITY_SIZE) {
                LOG(ERROR) << "Invalid VUC_SPEAKER_GROUP_AVAILABILITY_SIZE getInt32Values size: "
                    << speakerInfo.size();
            } else {
                if (speakerInfo[SpeakerIndex::FRONT_LEFT] != SpeakerState::OK &&
                    speakerInfo[SpeakerIndex::FRONT_RIGHT] != SpeakerState::OK &&
                    speakerInfo[SpeakerIndex::REAR_LEFT] != SpeakerState::OK &&
                    speakerInfo[SpeakerIndex::REAR_RIGHT] != SpeakerState::OK) {
                    LOG(ERROR) << "AUDIO UNAVAILABLE, reporting to VHAL";
                    audio_comp_status = false;
                } else {
                    LOG(INFO) << "SPEAKERs are in connected state";
                    audio_comp_status = true;
                }
                updateVHAL();
            }
        }
    }
    return;
}


/*------------------------------------------------------------------------------------------------------------*/

void AudioVhalAvailibilityCallback::onPropertyEvent(
    const std::vector<std::unique_ptr<
            android::frameworks::automotive::vhal::IHalPropValue>>& values) {
    LOG(DEBUG) << __func__ << ": Enter " ;
    for(const auto& value : values) {
        int32_t propId = value->getPropId();
        int32_t areadId = value->getAreaId();
        LOG(DEBUG) << __func__ << ": PropId : " << propId << ": areaId : " << areadId;
        if (propId != SOMEIP_CONNECTION_STATE) {
            LOG(ERROR) << __func__ << "PropId not matching with SOMEIP_CONNECTION_STATE";
        } else {
            LOG(INFO) << __func__ << "PropId matching with SOMEIP_CONNECTION_STATE";
             if (value->getInt32Values().size() < 1) {
                LOG(ERROR) << "Invalid SOMEIP_CONNECTION_STATE  size, empty value :" << value->getInt32Values().size();
                return;
            } else {
                LOG(DEBUG) << "Event Notify: New SOMEIP_CONNECTION_STATE event received. Val:" << value->getInt32Values()[0];
                if (value->getInt32Values()[0]) {
                    subscribeDiagProp();
                }
            }
        }
    }
    return;
}
