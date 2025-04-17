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

#define SNDCARD_PATH "/sys/kernel/snd_card/card_state"
#define MAX_SLEEP_RETRY 100

#define AUDIO_AVAILABILITY_ON 0x049D + 0x20000000 + 0x01000000 + 0x00200000 // VehiclePropertyGroup:VENDOR, VehicleArea:GLOBAL, VehiclePropertyType:BOOLEAN
#define AUDIO_NOTI_DETECT_FAILURE_V2 0x09F3 + 0x20000000 + 0x01000000 + 0x00410000 // VehiclePropertyGroup:VENDOR, VehicleArea:GLOBAL, VehiclePropertyType:INT32_VEC
#define SPEAKER_GROUP_AVAILABILITY 0x09F5 + 0x20000000 + 0x01000000 + 0x00410000 // VehiclePropertyGroup:VENDOR, VehicleArea:GLOBAL, VehiclePropertyType:INT32_VEC
#define SPEAKER_GROUP_AVAILABILITY_SIZE 4
#define AUDIO_NOTI_DETECT_FAILURE_V2_SIZE 7

static int fd = -1, efd = -1;
static int exit_thread = 0; //by default exit thread is made false
static bool card_status = false, speaker_status = false, audio_comp_status = false;
static int uuid = 0;
std::shared_ptr<aidl::android::hardware::automotive::vehicle::IVehicle> AudioDiagnostics::mVhal;

int getUUID() {
    return (++uuid) % INT_MAX;
}

ndk::ScopedAStatus updateVHAL() {
    bool audio_status = card_status && speaker_status && audio_comp_status;
    ::aidl::android::hardware::automotive::vehicle::SetValueRequest
             request =  ::aidl::android::hardware::automotive::vehicle::SetValueRequest{
                                            .requestId = (long)(getUUID()),
                                            .value = aidl::android::hardware::automotive::vehicle::VehiclePropValue{
                                                        .prop = AUDIO_AVAILABILITY_ON,
                                                        .value.int32Values{(int)audio_status},
                                                    },
                                        };
    ::aidl::android::hardware::automotive::vehicle::SetValueRequests requests;
    requests.payloads = {request};
    auto result = LargeParcelableBase::parcelableToStableLargeParcelable(requests);
    if (!result.ok()) {
        LOG(INFO) << "conversion to parcelable failed!!";
        return ndk::ScopedAStatus::ok();
    }
    if (result.value() != nullptr) {
        requests.sharedMemoryFd = std::move(*result.value());
        requests.payloads.clear();
    }

    auto vehicleCallback = ndk::SharedRefBase::make<AudioDiagVehicleCallback>();
    auto callbackClient =
    ::aidl::android::hardware::automotive::vehicle::IVehicleCallback::fromBinder(vehicleCallback->asBinder());
    auto mVhal = AudioDiagnostics::getVhalService();
    auto status = mVhal->setValues(callbackClient, requests);
    if (!status.isOk() ) {
       LOG(INFO) << "setValues failed: " << status.getMessage();
    }
    return ndk::ScopedAStatus::ok();
}

void AudioDiagnostics::monitorThreadLoop()
{
    struct pollfd *poll_fds;
    int rv = 0;
    char buf[12];
    int card_status = 0;
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
        read(fd, buf, 10);
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
            sscanf(buf , "%d", &card_status);
            LOG(INFO) << "card status: " << card_status;
            if (card_status == 0) {
               status = CARD_STATUS_OFFLINE;
               //inform vhal for AUDIO DOWN
               card_status = false;
               updateVHAL();
            }
            else if (card_status == 1) {
                status = CARD_STATUS_ONLINE;
               //inform vhal for AUDIO UP
                card_status = true;
               updateVHAL();
            }
            else if (card_status == 2)
                status = CARD_STATUS_STANDBY;
            else if (card_status == 3)
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
    auto mVhal = AudioDiagnostics::getVhalService();
    if (mVhal == nullptr) {
        LOG(ERROR) << "VHAL client creation failed!!";
    }
    std::shared_ptr<AudioFailureDetectCallback>
        failureDetectCallback = ndk::SharedRefBase::make<AudioFailureDetectCallback>();
    std::vector<aidl::android::hardware::automotive::vehicle::SubscribeOptions> options;
    options = {
        {
            .propId = AUDIO_NOTI_DETECT_FAILURE_V2,
            .areaIds = {},
        }
    };
    if (auto status = mVhal->subscribe(failureDetectCallback, options,
                                       /*maxSharedMemoryFileCount=*/0); !status.isOk()) {
        LOG(ERROR) << "Subscription to AUDIO_NOTI_DETECT_FAILURE_V2 property faied!!";
    } else {
        LOG(INFO) << "Subscribed to AUDIO_NOTI_DETECT_FAILURE_V2 property";
    }

    std::shared_ptr<SpeakerGroupAvailCallback>
        speakerGroupAvailCallback = ndk::SharedRefBase::make<SpeakerGroupAvailCallback>();
    options = {
        {
            .propId = SPEAKER_GROUP_AVAILABILITY,
            .areaIds = {},
        }
    };
    if (auto status = mVhal->subscribe(speakerGroupAvailCallback, options,
                                       /*maxSharedMemoryFileCount=*/0); !status.isOk()) {
        LOG(ERROR) << "Subscription to SPEAKER_GROUP_AVAILABILITY property failed!!";
    } else {
        LOG(INFO) << "Subscribed to SPEAKER_GROUP_AVAILABILITY property";
    }
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

ndk::ScopedAStatus AudioDiagVehicleCallback::onGetValues(
        const aidl::android::hardware::automotive::vehicle::GetValueResults& results) {
    LOG(INFO) << "onGetValues called";
    return ndk::ScopedAStatus::ok();
}


ndk::ScopedAStatus AudioDiagVehicleCallback::onSetValues(
        const aidl::android::hardware::automotive::vehicle::SetValueResults& results) {
    LOG(INFO) << "onSetValues called";
    {
        for (auto entry: results.payloads) {
            LOG(INFO) << "RequestId: " << (long)entry.requestId
                << " result: " << (int)entry.status;
        }
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus AudioDiagVehicleCallback::onPropertyEvent(
        const aidl::android::hardware::automotive::vehicle::VehiclePropValues&,
        int32_t) {

    LOG(INFO) << "onPropertyEvent called";
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus AudioDiagVehicleCallback::onPropertySetError(
        const aidl::android::hardware::automotive::vehicle::VehiclePropErrors&) {
    LOG(INFO) << "onPropertySetError called";
    return ndk::ScopedAStatus::ok();
}

/*------------------------------------------------------------------------------------------------------------*/



ndk::ScopedAStatus AudioFailureDetectCallback::onGetValues(
        const aidl::android::hardware::automotive::vehicle::GetValueResults& results) {
    LOG(INFO) << "onGetValues called";
    return ndk::ScopedAStatus::ok();
}


ndk::ScopedAStatus AudioFailureDetectCallback::onSetValues(
        const aidl::android::hardware::automotive::vehicle::SetValueResults& results) {
    LOG(INFO) << "onSetValues called";
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus AudioFailureDetectCallback::onPropertySetError(
        const aidl::android::hardware::automotive::vehicle::VehiclePropErrors&) {
    LOG(INFO) << "onPropertySetError called";
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus AudioFailureDetectCallback::onPropertyEvent(
        const aidl::android::hardware::automotive::vehicle::VehiclePropValues& vehiclePropValues,
        int32_t) {
    LOG(DEBUG) << __func__ << ": Enter " ;
    int32_t propId = -1, areadId = -1;
    for (auto entry: vehiclePropValues.payloads) {
        propId = entry.prop;
        areadId = entry.areaId;
        LOG(DEBUG) << __func__ << ": PropId : " << propId << ": areaId : " << areadId;

        if (propId != AUDIO_NOTI_DETECT_FAILURE_V2) {
            LOG(ERROR) << __func__ << "Callback PropId not matching with SPEAKER_GROUP_AVAILABILITY";
        } else {
            std::vector<int32_t> audioCompInfo = entry.value.int32Values;
            if (audioCompInfo.size() != AUDIO_NOTI_DETECT_FAILURE_V2_SIZE) {
                LOG(ERROR) << "Invalid AUDIO_NOTI_DETECT_FAILURE_V2_SIZE getInt32Values size: " << audioCompInfo.size();
                goto exit;
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
exit:
    LOG(DEBUG) << __func__ << ": Exit ";
    return ndk::ScopedAStatus::ok();
}

/*------------------------------------------------------------------------------------------------------------*/



ndk::ScopedAStatus SpeakerGroupAvailCallback::onGetValues(
        const aidl::android::hardware::automotive::vehicle::GetValueResults& results) {
    LOG(INFO) << "onGetValues called";
    return ndk::ScopedAStatus::ok();
}


ndk::ScopedAStatus SpeakerGroupAvailCallback::onSetValues(
        const aidl::android::hardware::automotive::vehicle::SetValueResults& results) {
    LOG(INFO) << "onSetValues called";
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SpeakerGroupAvailCallback::onPropertySetError(
        const aidl::android::hardware::automotive::vehicle::VehiclePropErrors&) {
    LOG(INFO) << "onPropertySetError called";
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SpeakerGroupAvailCallback::onPropertyEvent(
        const aidl::android::hardware::automotive::vehicle::VehiclePropValues& vehiclePropValues,
        int32_t) {
    LOG(DEBUG) << __func__ << ": Enter " ;
    int32_t propId = -1, areadId = -1;
    for (auto entry: vehiclePropValues.payloads) {
        propId = entry.prop;
        areadId = entry.areaId;
        LOG(DEBUG) << __func__ << ": PropId : " << propId << ": areaId : " << areadId;

        if (propId != SPEAKER_GROUP_AVAILABILITY) {
            LOG(ERROR) << __func__ << "Callback PropId not matching with SPEAKER_GROUP_AVAILABILITY";
        } else {
            std::vector<int32_t> speakerInfo = entry.value.int32Values;
            if (speakerInfo.size() != SPEAKER_GROUP_AVAILABILITY_SIZE) {
                LOG(ERROR) << "Invalid SPEAKER_GROUP_AVAILABILITY_SIZE getInt32Values size: " << speakerInfo.size();
                goto exit;
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
exit:
    LOG(DEBUG) << __func__ << ": Exit ";
    return ndk::ScopedAStatus::ok();
}