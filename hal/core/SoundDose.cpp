/*
 * Copyright (C) 2022 The Android Open Source Project
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

/*
 * ​​​​​Changes from Qualcomm Innovation Center, Inc. are provided under the following license:
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL_SoundDose_QTI"

#include <android-base/logging.h>
#include <qti-audio-core/SoundDose.h>
#include "PalDefs.h"
#include <qti-audio/PlatformConverter.h>

using aidl::android::media::audio::common::AudioDevice;
using aidl::android::media::audio::common::AudioDeviceType;
namespace qti::audio::core {

/*RS1 threshold value above which only Sound dose is to be reported.*/
constexpr float kRs1OutputdBFS = 80.f;  // dBA

using ISoundDoseCallback = aidl::android::hardware::audio::core::sounddose::ISoundDose::IHalSoundDoseCallback;
using MelRecord = ISoundDoseCallback::MelRecord;

ndk::ScopedAStatus SoundDose::setOutputRs2UpperBound(float in_rs2ValueDbA) {
    if (in_rs2ValueDbA < static_cast<float>(MIN_RS2) ||
        in_rs2ValueDbA > static_cast<float>(DEFAULT_MAX_RS2)) {
        LOG(ERROR) << __func__ << ": RS2 value is invalid: " << in_rs2ValueDbA;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    mRs2Value = in_rs2ValueDbA;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SoundDose::getOutputRs2UpperBound(float* _aidl_return) {
    *_aidl_return = mRs2Value;
    LOG(DEBUG) << __func__ << ": returning " << *_aidl_return;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SoundDose::registerSoundDoseCallback(
        const std::shared_ptr<ISoundDose::IHalSoundDoseCallback>& in_callback) {
    if (in_callback.get() == nullptr) {
        LOG(ERROR) << __func__ << ": Callback is nullptr";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (mCallback != nullptr) {
        LOG(ERROR) << __func__ << ": Sound dose callback was already registered";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    mCallback = in_callback;
    LOG(DEBUG) << __func__ << ": Registered sound dose callback ";
    return ndk::ScopedAStatus::ok();
}

void SoundDose::updateActiveDevicesMap(const AudioDeviceAddress& deviceAddress, pal_device_id_t palDeviceId) {
    if (deviceAddress.getTag() != AudioDeviceAddress::Tag::mac) {
        LOG(ERROR) << __func__ << " failed to find MAC address for given Bluetooth device";
        return;
    }
    const auto& deviceAddressMac = deviceAddress.get<AudioDeviceAddress::Tag::mac>();
    if (deviceAddressMac.size() != 6) {
        LOG(ERROR) << __func__ << " invalid MAC address size for Bluetooth device";
        return;
    }

    // Update the map
    mActiveDeviceAddressMap[palDeviceId] = deviceAddress;

}
void SoundDose::handleSoundDoseInfoEvent(void* const data) {
    // Convert eventData to pal_sound_dose_info_t*
    pal_sound_dose_info_t* palSoundDoseInfo = reinterpret_cast<pal_sound_dose_info_t*>(data);

    if (!palSoundDoseInfo) {
        LOG(ERROR) << "Invalid event data pointer";
        return;
    }

    if (!mCallback) {
        LOG(ERROR) << "mCallback is null";
        return;
    }

    AudioDeviceDescription deviceDescription = PlatformConverter::convertPalIdToAudioDeviceDescription(palSoundDoseInfo->id);
    if (mActiveDeviceAddressMap.find(palSoundDoseInfo->id) == mActiveDeviceAddressMap.end()) {
        //entry not found in map
        return;
    }

    AudioDeviceAddress deviceAddress = mActiveDeviceAddressMap[palSoundDoseInfo->id];
    AudioDevice audioDevice;
    audioDevice.type = deviceDescription;
    audioDevice.address = deviceAddress;

    if (palSoundDoseInfo->is_momentary_exposure_warning == 1) {
        // Call onMomentaryExposureWarning
        float currentDbA = palSoundDoseInfo->mel_values[0];
        LOG(DEBUG) << "Momentary exposure warning with value: " << currentDbA;
        mCallback->onMomentaryExposureWarning(currentDbA, audioDevice);
    } else {
        // Process mel_values to find continuous values above 80
        std::vector<float> melValues;
        uint64_t firstTimestamp = 0;
        bool inSegment = false;

        for (uint32_t i = 0; i < palSoundDoseInfo->num_mel_values; ++i) {
            if (palSoundDoseInfo->mel_values[i] >= kRs1OutputdBFS) {
                if (!inSegment) {
                    // Start a new segment
                    inSegment = true;
                    firstTimestamp = palSoundDoseInfo->timestamp[i];
                    melValues.clear();
                }
                melValues.push_back(palSoundDoseInfo->mel_values[i]);
            } else {
                if (inSegment) {
                    // End the current segment and send it
                    MelRecord melRecord;
                    melRecord.melValues = melValues;
                    melRecord.timestamp = firstTimestamp;
                    LOG(DEBUG) << "Sending new MEL values segment with timestamp: " << firstTimestamp;
                    mCallback->onNewMelValues(melRecord, audioDevice);
                    inSegment = false;
                }
            }
        }

        // Send the last segment if it exists
        if (inSegment) {
            MelRecord melRecord;
            melRecord.melValues = melValues;
            melRecord.timestamp = firstTimestamp;
            LOG(DEBUG) << "Sending final MEL values segment with timestamp: " << firstTimestamp;
            mCallback->onNewMelValues(melRecord, audioDevice);
        }
    }
}

} // namespace qti::audio::core
