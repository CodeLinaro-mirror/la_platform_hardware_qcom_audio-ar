/*
 * Copyright (C) 2023 The Android Open Source Project
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
 * Changes from Qualcomm Technologies, Inc. are provided under the following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <vector>

#define LOG_TAG "AHAL_ModulePrimary_QTI"

#include <Utils.h>
#include <android-base/logging.h>
#include <cutils/str_parms.h>
#include <android/binder_auto_utils.h>
#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <error/Result.h>
#include <fmt/ranges.h>
#include <cmath>
#ifdef ENABLE_QCOM_AMPERE_AUDIO
#include <extensions/AudioHalFocusManager.h>
#include <aidl/alliance/hardware/automotive/audiocontrol/internal/IAudioControlInternal.h>
#endif
#include <qti-audio-core/PowerPolicyManager.h>

#include <aidl/qti/audio/core/VString.h>
#include <qti-audio-core/Bluetooth.h>
#include <qti-audio-core/ModulePrimary.h>
#include <qti-audio-core/Parameters.h>
#include <qti-audio-core/PlatformUtils.h>
#include <qti-audio-core/StreamInPrimary.h>
#include <qti-audio-core/StreamOutPrimary.h>
#ifdef ECNR_HAL_ENABLE
#include <qti-audio-core/StreamOutPrimaryOEM.h>
#include <qti-audio-core/StreamInPrimaryOEM.h>
#endif
#include <qti-audio-core/StreamStub.h>
#include <qti-audio-core/Telephony.h>
#include <qti-audio-core/Utils.h>
#include <qti-audio-core/Stream.h>
#include <memory>

#define AUDIO_PARAMETER_KEY_BALANCE "Balance"
#define AUDIO_PARAMETER_KEY_FADER "Fader"
#define AUDIO_PARAMETER_KEY_ISFADERAVAILABLE "isFaderAvailable"
#ifdef ENABLE_QCOM_AMPERE_AUDIO
#define MIN_VOLUME_VALUE_MB -9000
#define MAX_VOLUME_VALUE_MB 0
#define BALANCE_FADER_SCALE 5.0
#endif

#include <android/binder_manager.h>
#include <android/binder_process.h>
#include "PalParamDelegator.h"

using aidl::android::hardware::audio::common::SinkMetadata;
using aidl::android::hardware::audio::common::SourceMetadata;
using aidl::android::media::audio::common::AudioOffloadInfo;
using aidl::android::media::audio::common::AudioPort;
using aidl::android::media::audio::common::AudioPortExt;
using aidl::android::media::audio::common::AudioDevice;
using aidl::android::media::audio::common::AudioDeviceType;
using aidl::android::media::audio::common::AudioPortConfig;
using aidl::android::media::audio::common::MicrophoneInfo;
using aidl::android::media::audio::common::Boolean;
using aidl::android::media::audio::common::Float;

using ::aidl::android::hardware::audio::common::getFrameSizeInBytes;
using ::aidl::android::hardware::audio::common::isBitPositionFlagSet;
using ::aidl::android::hardware::audio::common::isValidAudioMode;
using ::aidl::android::hardware::audio::common::SinkMetadata;
using ::aidl::android::hardware::audio::common::SourceMetadata;
using ::aidl::android::hardware::audio::common::getChannelCount;
using ::aidl::android::hardware::audio::core::AudioPatch;
using ::aidl::android::hardware::audio::core::AudioRoute;
using ::aidl::android::hardware::audio::core::IStreamIn;
using ::aidl::android::hardware::audio::core::IStreamOut;
using ::aidl::android::hardware::audio::core::ITelephony;
using ::aidl::android::hardware::audio::core::VendorParameter;
using ::aidl::qti::audio::core::VString;
using ::aidl::android::hardware::audio::core::IBluetooth;
using ::aidl::android::hardware::audio::core::IBluetoothA2dp;
using ::aidl::android::hardware::audio::core::IBluetoothLe;
using ::aidl::android::hardware::audio::core::VendorParameter;
#ifdef ENABLE_QCOM_AMPERE_AUDIO
using aidl::android::media::audio::common::Float;
using aidl::ampere::hardware::interfaces::automotive::audioparameterparser::CarPlayVendorParameterExt;
using aidl::ampere::hardware::interfaces::automotive::audioparameterparser::RadioVendorParameterExt;
using aidl::ampere::hardware::interfaces::automotive::audioparameterparser::AudioControlVendorParameterExt;
#endif


namespace qti::audio::core {

std::vector<std::weak_ptr<::qti::audio::core::StreamOut>> ModulePrimary::mStreamsOut;
std::vector<std::weak_ptr<::qti::audio::core::StreamIn>> ModulePrimary::mStreamsIn;
std::string qti::audio::core::ModulePrimary::globalAudioSource = "DEFAULT";

#ifdef ENABLE_QCOM_AMPERE_AUDIO
std::unordered_map<std::string, FocusSession> ModulePrimary::mActiveFocusDevices;
std::shared_ptr<::aidl::alliance::hardware::automotive::audiocontrol::internal::IAudioControlInternal> ModulePrimary::mAudioControlInternalProxy = nullptr;
#define ALL_BUS_VOLUMES 0x7F
#define BUS_COUNT 7
#define Vol_to_mdB(X) ((X == 0.0) ? (MIN_VOLUME_VALUE_MB) : lrint(2000 * log10f(X)))
#endif

std::vector<float> qti::audio::core::MuteConfig::getVol = {-3600.0f, -3600.0f};

std::mutex ModulePrimary::outListMutex;
std::mutex ModulePrimary::inListMutex;
#define MEDIA_BUS "BUS00_MEDIA"
ndk::ScopedAStatus qti::audio::core::ModulePrimary::setAudioPortConfig(const ::aidl::android::media::audio::common::AudioPortConfig& in_requested,::aidl::android::media::audio::common::AudioPortConfig* out_suggested,bool* _aidl_return)
{
    int list_id,Requsted_id;
    LOG(DEBUG) << "setAudioPortConfig ModulePrimary";
    ndk::ScopedAStatus status = Module::setAudioPortConfig(in_requested,out_suggested,_aidl_return);
    if (!status.isOk()) {
            return status;
        }
    float volume;
    if (in_requested.gain.has_value()) {
        if (in_requested.gain->values.empty()) {
            return ndk::ScopedAStatus::ok();
        }
    volume = (static_cast<float>(in_requested.gain->values[0]));
#ifdef ENABLE_QCOM_HAL_AUDIO_FOCUS
        LOG(DEBUG) << __func__ << ": requested " << in_requested.toString();
        if (in_requested.ext.getTag() == AudioPortExt::device) {
            if (auto devicePort = in_requested.ext.get<AudioPortExt::device>();
                    (devicePort.device.type.type == AudioDeviceType::OUT_BUS &&
                     devicePort.device.type.connection.empty())) {
                if (auto address = devicePort.device.address.get<AudioDeviceAddress::Tag::id>();
                        !address.empty()) {
                    //update thermal derating algo of latest volume
                    //TODO: Elimnate hard dependency on MEDIA_BUS
                    if (address == MEDIA_BUS) {
                        mAudExt.mAAutoVhalPriorityExtension->setMediaGain(volume);
                    }
                    setUpPriorityFocus();
                    if (mActiveFocusDevices.find(address) != mActiveFocusDevices.end()) {
                        mAudExt.mAutoAudioHalPriorityExtension->updateVolume(mActiveFocusDevices[address].FocusId, volume, false /*internal volume change*/);
                    } else {
                        LOG(ERROR) << "Volume update failed for BUS: " << address;
                    }
                }
            }
        }
#endif
    } else {
        return ndk::ScopedAStatus::ok();
    }
    auto list = getOutStreams();
    if (list.empty()) {
        LOG(DEBUG) << "the module list is empty";
        return ndk::ScopedAStatus::ok();
    }
    auto& routes = getConfig().routes;
    auto route = routes.begin();
    for (route;route != routes.end(); route++) {
        if (route->sinkPortId == in_requested.portId) {
            LOG(DEBUG) << "route port " << route->toString();
            break;
        }
    }
    LOG(DEBUG) << "The module list is not empty";

    // Get the ports to handle
    auto portsToHandle = route->sourcePortIds;

    // Iterate over the list
    for (auto iter = list.begin(); iter != list.end() && !portsToHandle.empty(); ++iter) {
        // Try to lock the iterator
        auto outIter = iter->lock();
        if (outIter) {
            // Get the stream context and mix port config
            auto& mcontext = outIter->getStreamContext();
            auto& listAudioPortConfig = mcontext.getMixPortConfig();

            // Get the port ID and channel count
            int listId = listAudioPortConfig.portId;
            int numChannels = static_cast<int>(getChannelCount(listAudioPortConfig.channelMask.value()));

            // Check if the port ID is in the ports to handle
            if (std::find(portsToHandle.begin(), portsToHandle.end(), listId) != portsToHandle.end()) {
                // Create a vector to store the volume values
                std::vector<float> volumes(numChannels, volume);

                // Log the stream ID and volume
                LOG(DEBUG) << "Found the stream at ID: " << listId << " Gain is: " << volume << " Volume is: " << volumes[0];
                // Set the hardware volume
                auto streamObj = std::static_pointer_cast<::qti::audio::core::StreamOutPrimary>(outIter);
                streamObj->setPALVolume(volumes);
                LOG(DEBUG) << "Volume set: " << volumes[0];
                // Remove the port ID from the ports to handle
                std::erase(portsToHandle, listId);
            }
        } else {
            LOG(DEBUG) << "Error in generation of shared pointer";
        }
        outIter.reset();
    }

// Log any unhandled ports
if (!portsToHandle.empty()) {
    LOG(DEBUG) << "Unhandled ports for volume change: " << fmt::format("{}", portsToHandle);
}
    return ndk::ScopedAStatus::ok();
}

std::string ModulePrimary::toStringInternal() {
    std::ostringstream os;
    os << "--- ModulePrimary start ---" << std::endl;
    os << getConfig().toString() << std::endl;

    os << std::endl << " --- mPatches ---" << std::endl;
    std::for_each(mPatches.cbegin(), mPatches.cend(), [&](const auto& pair) {
        os << "PortConfigId/PortId:" << pair.first << " Patch Id:" << pair.second << std::endl;
    });
    os << std::endl << " --- mPatches end ---" << std::endl << std::endl;

    os << mStreams.toString();

    os << mPlatform.toString() << std::endl;
    os << "--- ModulePrimary end ---" << std::endl;
    return os.str();
}

void ModulePrimary::dumpInternal(const std::string& identifier) {
    const auto realTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::system_clock::now().time_since_epoch())
                                    .count();
    const std::string kDumpPath{std::string("/data/vendor/audio/audio_hal_service_")
                                        .append(identifier)
                                        .append("_")
                                        .append(std::to_string(realTimeMs))
                                        .append(".dump")};

    const auto fd = ::open(kDumpPath.c_str(), O_CREAT | O_WRONLY | O_TRUNC,
                           S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd <= 0) {
        LOG(ERROR) << __func__ << ": dump internal failed; fd:" << fd
                   << " unable to open file:" << kDumpPath;
        return;
    }
    const auto dumpData = toStringInternal();
    auto b = ::write(fd, dumpData.c_str(), dumpData.size());
    if (b != static_cast<decltype(b)>(dumpData.size())) {
        LOG(ERROR) << __func__ << ": dump internal failed to write in " << kDumpPath;
    }
    LOG(DEBUG) << __func__ << ": at: " << kDumpPath;
    ::close(fd);
    return;
}

binder_status_t ModulePrimary::dump(int fd, const char** args, uint32_t numArgs) {
    if (fd <= 0) {
        LOG(ERROR) << ": fd:" << fd << " dump error";
        return -EINVAL;
    }
    auto dumpData = toStringInternal();
    auto b = ::write(fd, dumpData.c_str(), dumpData.size());
    if (b != static_cast<decltype(b)>(dumpData.size())) {
        LOG(ERROR) << __func__ << " write error in dump";
        return -EIO;
    }
    LOG(INFO) << __func__ << " :success";
    return 0;
}


ModulePrimary::ModulePrimary() : Module(Type::DEFAULT) {
    mOffloadSpeedSupported = mPlatform.platformSupportsOffloadSpeed();
#ifdef ENABLE_QCOM_AMPERE_AUDIO
    (void) getAudioControlInternalService();
#endif
}

template<class Intf>
std::shared_ptr<Intf> getServiceInstance(const std::string& instanceName) {
    const std::string serviceName =
            std::string(Intf::descriptor).append("/").append(instanceName);
    std::shared_ptr<Intf> service;
    while (!service) {
        AIBinder* serviceBinder = nullptr;
        while (!serviceBinder) {
            // 'waitForService' may return a nullptr, hopefully a transient error.
            serviceBinder = AServiceManager_waitForService(serviceName.c_str());
        }
        // `fromBinder` may fail and return a nullptr if the service has died in the meantime.
        service = Intf::fromBinder(ndk::SpAIBinder(serviceBinder));
    }
    return service;
}

#ifdef ENABLE_QCOM_AMPERE_AUDIO
std::shared_ptr<aidl::alliance::hardware::automotive::audiocontrol::internal::IAudioControlInternal> ModulePrimary::getAudioControlInternalService() {
    LOG(ERROR) << __func__ << ": enter getAudioControlInternalService";
    //std::lock_guard l(mLock);
    if (mAudioControlInternalProxy == nullptr) {
        auto aidlServiceName = std::string() + aidl::alliance::hardware::automotive::audiocontrol::internal::IAudioControlInternal::descriptor + "/default";
        if (!AServiceManager_isDeclared(aidlServiceName.c_str())) {
            LOG(ERROR) << __func__ << ": No IAudioControlInternal declared, skipping";
            return nullptr;
        }
        std::shared_ptr<aidl::alliance::hardware::automotive::audiocontrol::internal::IAudioControlInternal> proxy =
            getServiceInstance<aidl::alliance::hardware::automotive::audiocontrol::internal::IAudioControlInternal>("default");
        if (proxy == nullptr) {
            LOG(ERROR) << __func__ << ": Failed to connect IAudioControlInternal";
            return nullptr;
        } else {
            LOG(DEBUG) << __func__ << ": Connected to IAudioControlInternal: SUCCESS";
            mAudioControlInternalProxy = proxy;
        }
    }
    return mAudioControlInternalProxy;
}
#endif

ndk::ScopedAStatus ModulePrimary::getMicrophones(std::vector<MicrophoneInfo>* _aidl_return) {
    *_aidl_return = mPlatform.getMicrophoneInfo();
    LOG(VERBOSE) << __func__ << ": returning " << ::android::internal::ToString(*_aidl_return);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModulePrimary::getMicMute(bool* _aidl_return) {
    if (!mTelephony) {
        LOG(ERROR) << __func__ << ": Telephony not created ";
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    *_aidl_return = mMicMute;
    LOG(VERBOSE) << __func__ << ": returning " << *_aidl_return;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModulePrimary::setMicMute(bool in_mute) {
    if (!mTelephony) {
        LOG(ERROR) << __func__ << ": Telephony not created ";
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    LOG(DEBUG) << __func__ << ": " << in_mute;

    mMicMute = in_mute;
    mPlatform.setMicMuteStatus(mMicMute);

    mTelephony->setMicMute(mMicMute);

    int ret = mAudExt.mHfpExtension->audio_extn_hfp_set_mic_mute(mMicMute);

    for (const auto& inputMixPortConfigId :
         getActiveInputMixPortConfigIds(getConfig().portConfigs)) {
        if(!mPlatform.getTranslationRecordState()){
            mStreams.setStreamMicMute(inputMixPortConfigId, mMicMute);
        } else {
            // Need to keep the Audio FFECNS Record stream unmuted when Translate Record Usecase Enabled
            LOG(DEBUG) << __func__ << ": SetStreamMicMute skipped for Voice Translate Record";
        }
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModulePrimary::updateScreenState(bool in_isTurnedOn) {
    LOG(VERBOSE) << __func__ << ": " << in_isTurnedOn;
    mPlatform.updateScreenState(in_isTurnedOn);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModulePrimary::updateScreenRotation(ScreenRotation in_rotation) {
    LOG(VERBOSE) << __func__ << ": " << toString(in_rotation);
    mPlatform.updateScreenRotation(in_rotation);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModulePrimary::getBluetooth(std::shared_ptr<IBluetooth>* _aidl_return) {
    if (!mBluetooth) {
        mBluetooth = ndk::SharedRefBase::make<::qti::audio::core::Bluetooth>();
    }
    *_aidl_return = mBluetooth.getInstance();
    LOG(DEBUG) << __func__
               << ": returning instance of IBluetooth: " << _aidl_return->get()->asBinder().get();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModulePrimary::getBluetoothA2dp(std::shared_ptr<IBluetoothA2dp>* _aidl_return) {
    if (!mBluetoothA2dp) {
        mBluetoothA2dp = ndk::SharedRefBase::make<::qti::audio::core::BluetoothA2dp>();
    }
    *_aidl_return = mBluetoothA2dp.getInstance();
    LOG(DEBUG) << __func__ << ": returning instance of IBluetoothA2dp: "
               << _aidl_return->get()->asBinder().get();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModulePrimary::getBluetoothLe(std::shared_ptr<IBluetoothLe>* _aidl_return) {
    if (!mBluetoothLe) {
        mBluetoothLe = ndk::SharedRefBase::make<::qti::audio::core::BluetoothLe>();
    }
    *_aidl_return = mBluetoothLe.getInstance();
    LOG(DEBUG) << __func__
               << ": returning instance of IBluetoothLe: " << _aidl_return->get()->asBinder().get();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModulePrimary::getTelephony(std::shared_ptr<ITelephony>* _aidl_return) {
    if (!mTelephony) {
        mTelephony = ndk::SharedRefBase::make<Telephony>();
        mPlatform.setTelephony(mTelephony.getInstance());
    }
    *_aidl_return = mTelephony.getInstance();
    LOG(DEBUG) << __func__
               << ": returning instance of ITelephony: " << _aidl_return->get()->asBinder().get();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModulePrimary::createInputStream(StreamContext&& context,
                                                    const SinkMetadata& sinkMetadata,
                                                    const std::vector<MicrophoneInfo>& microphones,
                                                    std::shared_ptr<StreamIn>* result) {
#ifdef ECNR_HAL_ENABLE
    createStreamInstance<StreamInPrimaryOEM>(result, std::move(context), sinkMetadata, microphones);
#else
    createStreamInstance<StreamInPrimary>(result, std::move(context), sinkMetadata, microphones);
#endif
    PowerPolicyManager::getInstance().updateStreamInPrimaryList(
        (std::static_pointer_cast<::qti::audio::core::StreamInPrimary>(*result)));
    ModulePrimary::inListMutex.lock();
    ModulePrimary::updateStreamInList(*result);
    if (mTelephony) {
        mTelephony->mStreamInPrimary = *result;
    }
    ModulePrimary::inListMutex.unlock();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModulePrimary::createOutputStream(
        StreamContext&& context, const SourceMetadata& sourceMetadata,
        const std::optional<AudioOffloadInfo>& offloadInfo, std::shared_ptr<StreamOut>* result) {
    if (mPlatform.isSoundCardDown() &&
        (hasOutputDirectFlag(context.getMixPortConfig().flags.value()) ||
         hasOutputCompressOffloadFlag(context.getMixPortConfig().flags.value()))) {
        LOG(ERROR) << __func__ << ": avoid direct or compress streams as sound card is down";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
#ifdef ECNR_HAL_ENABLE
    createStreamInstance<StreamOutPrimaryOEM>(result, std::move(context), sourceMetadata, offloadInfo);
#else
    createStreamInstance<StreamOutPrimary>(result, std::move(context), sourceMetadata, offloadInfo);
#endif
    PowerPolicyManager::getInstance().updateStreamOutPrimaryList(
        (std::static_pointer_cast<::qti::audio::core::StreamOutPrimary>(*result)));
    ModulePrimary::outListMutex.lock();
    ModulePrimary::updateStreamOutList(*result);
    // save primary out stream weak ptr, as some other modules need it.
    if (mTelephony) {
        mTelephony->mStreamOutPrimary = *result;
    }

    ModulePrimary::outListMutex.unlock();
    return ndk::ScopedAStatus::ok();
}

std::vector<::aidl::android::media::audio::common::AudioProfile> ModulePrimary::getDynamicProfiles(
        const ::aidl::android::media::audio::common::AudioPort& audioPort) {
    if (isUsbDevice(audioPort.ext.get<AudioPortExt::Tag::device>().device)) {
        /* as of now, we do dynamic fetching for usb devices*/
        auto dynamicProfiles = mPlatform.getDynamicProfiles(audioPort);
        return dynamicProfiles;
    }
    return {};
}

void ModulePrimary::onNewPatchCreation(const std::vector<AudioPortConfig*>& sources,
                                       const std::vector<AudioPortConfig*>& sinks,
                                       AudioPatch& newPatch) {
    if (!isMixPortConfig(*(sources.at(0))) && !isMixPortConfig(*(sinks.at(0)))) {
        LOG(VERBOSE) << __func__ << ": no mix ports detected";
        return;
    }
    auto numFrames = mPlatform.getMinimumStreamSizeFrames(sources, sinks);
    if (numFrames < kMinimumStreamBufferSizeFrames) {
        LOG(DEBUG) << __func__ << ": got invalid stream size frames " << numFrames
                   << " adjusting to " << kMinimumStreamBufferSizeFrames;
        numFrames = kMinimumStreamBufferSizeFrames;
    }
    newPatch.minimumStreamBufferSizeFrames = numFrames;
}

void ModulePrimary::setAudioPatchTelephony(const std::vector<AudioPortConfig*>& sources,
                                           const std::vector<AudioPortConfig*>& sinks,
                                           const AudioPatch& patch) {
    std::string patchDetails = getPatchDetails(patch);

    if (!mTelephony) {
        LOG(ERROR) << __func__ << ": Telephony not created " << patchDetails << patch.toString();
        return;
    }

    if (!isDevicePortConfig(*(sources.at(0))) || !isDevicePortConfig(*(sinks.at(0)))) {
        return;
    }

    bool updateRx = isTelephonyRXDevice(sources.at(0)->ext.get<AudioPortExt::Tag::device>().device);
    bool updateTx = isTelephonyTXDevice(sinks.at(0)->ext.get<AudioPortExt::Tag::device>().device);

    if (!updateRx && !updateTx) {
        LOG(ERROR) << __func__ << ": neither RX nor TX update " << patchDetails << patch.toString();
        return;
    }

    const auto& portConfigsForDeviceChange = updateRx ? (sinks) : (sources);

    std::vector<AudioDevice> devices;
    for (const auto portConfig : portConfigsForDeviceChange) {
        devices.push_back(portConfig->ext.get<AudioPortExt::Tag::device>().device);
    }

    mTelephony->setDevices(devices, updateRx);
    mAudExt.mHfpExtension->audio_extn_hfp_set_device(devices, updateRx);
    LOG(INFO) << __func__ << ": set telephony " << (updateRx ? "RX" : "TX") << " devices";
}

void ModulePrimary::resetAudioPatchTelephony(const AudioPatch& patch) {
    const std::string patchDetails = getPatchDetails(patch);
    if (!mTelephony) {
        LOG(ERROR) << __func__ << ": Telephony not created " << patchDetails << patch.toString();
        return;
    }

    auto& configs = getConfig().portConfigs;
    std::vector<int32_t> missingIds;
    auto sources = selectByIds<AudioPortConfig>(configs, patch.sourcePortConfigIds, &missingIds);
    if (!missingIds.empty()) {
        LOG(ERROR) << __func__ << ": following source port config ids not found: "
                   << ::android::internal::ToString(missingIds);
    }
    auto sinks = selectByIds<AudioPortConfig>(configs, patch.sinkPortConfigIds, &missingIds);
    if (!missingIds.empty()) {
        LOG(ERROR) << __func__ << ": following sink port config ids not found: "
                   << ::android::internal::ToString(missingIds);
    }

    if (!isDevicePortConfig(*(sources.at(0))) || !isDevicePortConfig(*(sinks.at(0)))) {
        // atleast one of the port config is a mix port config.
        return;
    }

    bool updateRx = isTelephonyRXDevice(sources.at(0)->ext.get<AudioPortExt::Tag::device>().device);
    bool updateTx = isTelephonyTXDevice(sinks.at(0)->ext.get<AudioPortExt::Tag::device>().device);

    if (!updateRx && !updateTx) {
        LOG(ERROR) << __func__ << ": neither RX nor TX update " << patchDetails << patch.toString();
        return;
    }

    mTelephony->resetDevices(updateRx);

    LOG(INFO) << __func__ << ": reset telephony " << (updateRx ? "RX" : "TX") << " devices";
}

int ModulePrimary::onExternalDeviceConnectionChanged(
        const ::aidl::android::media::audio::common::AudioPort& audioPort, bool connected) {
    if (mDebug.simulateDeviceConnections) {
        LOG(DEBUG) << __func__ << ": connection is in simulation mode";
        return 0;
    }

    if (int ret = mPlatform.handleDeviceConnectionChange(audioPort, connected); ret) {
        LOG(WARNING) << __func__ << " failed to handle device connection change:"
                     << (connected ? " connect" : "disconnect") << " for " << audioPort.toString();
        return ret;
    }

    if (!mTelephony) {
        LOG(ERROR) << __func__ << ": Telephony not created ";
        return 0;
    }

    // At this point it is safe to assume this audio port if of type audio device
    const auto& extDevice = audioPort.ext.get<AudioPortExt::Tag::device>().device;
    mTelephony->onExternalDeviceConnectionChanged(extDevice, connected);

    return 0;
}

int32_t ModulePrimary::getNominalLatencyMs(const AudioPortConfig& mixPortConfig) {
    return mPlatform.getLatencyMs(mixPortConfig);
}

ndk::ScopedAStatus ModulePrimary::getSupportedPlaybackRateFactors(
        SupportedPlaybackRateFactors* _aidl_return) {
    LOG(DEBUG) << __func__ << " speed supported " << mOffloadSpeedSupported;
    if (mOffloadSpeedSupported) {
        _aidl_return->minSpeed = 0.1f;
        _aidl_return->maxSpeed = 2.0f;
        _aidl_return->minPitch = 1.0f;
        _aidl_return->maxPitch = 1.0f;
        return ndk::ScopedAStatus::ok();
    }
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
// start of module parameters handling
#ifdef ENABLE_QCOM_AMPERE_AUDIO

namespace {

template <typename T>
using ConversionResult = ::android::error::Result<T>;

#define GENERATE_EXTRACT_PARAMETER_LIST_DEF(V)      \
    V(CarPlay)                                      \
    V(AudioControl)                                 \
    V(Radio)

#define GENERATE_EXTRACT_PARAMETER_TEMPLATES(symbol)                                            \
    template <typename W, symbol##VendorParameterExt::Parameter::Tag _tag, typename V>          \
    ConversionResult<V> extractParameter(const VendorParameter& p)  {                           \
        std::optional<W> value;                                                                 \
        binder_status_t result = p.ext.getParcelable(&value);                                   \
        if (result == STATUS_OK && value.has_value()) {                                         \
            if (value.value().value.getTag() != _tag) {                                         \
                return ::android::base::unexpected(::android::BAD_VALUE);                       \
            }                                                                                   \
            return value.value().value.template get<_tag>();                                    \
        }                                                                                       \
        LOG(ERROR) << __func__ << ": failed to read the value of the parameter \"" << p.id      \
                   << "\": " << result;                                                         \
        return ::android::base::unexpected(::android::BAD_VALUE);                               \
    }

GENERATE_EXTRACT_PARAMETER_LIST_DEF(GENERATE_EXTRACT_PARAMETER_TEMPLATES)

}  // namespace

void ModulePrimary::onSetCarplayParameters(const std::vector<VendorParameter>& params) {
    for (const auto& param : params) {
        if (setCarPlayParameter(param) != ::android::OK) {
            LOG(ERROR) << __func__ << ": FAILED to extract value from " << param.id.c_str();
        }
    }
}

std::string ModulePrimary::carplayParamConverter(CarPlayVendorParameterExt::Rate carplayparams) {
    std::string carplay_param;
    switch (carplayparams)
        {
            case CarPlayVendorParameterExt::Rate::KHZ_8:
                carplay_param = "8000" ;
                break;
            case CarPlayVendorParameterExt::Rate::KHZ_16:
                carplay_param = "16000" ;
                break;
            case CarPlayVendorParameterExt::Rate::KHZ_24:
                carplay_param = "24000" ;
                break;
            case CarPlayVendorParameterExt::Rate::KHZ_32:
                carplay_param = "32000" ;
                break;
            case CarPlayVendorParameterExt::Rate::KHZ_48:
                carplay_param = "48000" ;
                break;
            default:
                carplay_param = "Invalid" ;
        }
        LOG(DEBUG) << __func__ <<" Updated carplay_param: "<<carplay_param;
        return carplay_param;
}
int ModulePrimary::getMedia_volume() {
    int volume = 0;
    const auto& configs = getConfig().portConfigs;
    for (const auto& portConfig : configs) {
        if (portConfig.ext.getTag() == AudioPortExt::device) {
            if (auto devicePort = portConfig.ext.get<AudioPortExt::device>();
                    (devicePort.device.type.type == AudioDeviceType::OUT_BUS &&
                     devicePort.device.type.connection.empty())) {
                if (auto address = devicePort.device.address.get<AudioDeviceAddress::Tag::id>();
                        !address.empty()) {
                    if (address == MEDIA_BUS) {
                        volume = portConfig.gain->values[0];
                        LOG(DEBUG) << __func__ << " volume: " << volume;
                    }
                }
            }
        }
    }
    return volume;
}

::android::status_t ModulePrimary::setCarPlayParameter(const VendorParameter& param) {
    struct str_parms* parms = NULL;
    std::string kvpairs = "";
    std::string keyvalue;
    static FocusSession focusSessionInfo = {};
    using Tag = CarPlayVendorParameterExt::Parameter::Tag;
    if (param.id == CarPlayVendorParameterExt::CARPLAY_SAMPLERATE) {
        std::string CarplaySampleRate;
        auto p = extractParameter<CarPlayVendorParameterExt, Tag::sampleRate,
                CarPlayVendorParameterExt::Rate>(param);
        CarPlayVendorParameterExt::Rate aidlCpRate = VALUE_OR_RETURN_STATUS(p);
        LOG(DEBUG) << __func__ << " CP Rate "<< toString(aidlCpRate);

        CarplaySampleRate = carplayParamConverter(aidlCpRate);
        LOG(DEBUG) << __func__ << " CP SampleRate "<<CarplaySampleRate;

        keyvalue = param.id + "=" + CarplaySampleRate + ";";
        kvpairs.append(keyvalue);
        if (kvpairs.length() != keyvalue.length()) {
            LOG(ERROR) << __func__ << ": invalid kvpairs length";
            return ::android::BAD_VALUE;
        }
        LOG(DEBUG) << __func__ << " Key Value pairs: " << kvpairs;
        if (!kvpairs.empty()) {
            parms = str_parms_create_str(kvpairs.c_str());
            if (!parms) {
                return ::android::BAD_VALUE;
            }
            mAudExt.audio_extn_set_parameters(parms);
        }
    } else if (param.id == CarPlayVendorParameterExt::CARPLAY_VOCODER_SAMPLERATE) {
        std::string VocoderSampleRate;
        auto p = extractParameter<CarPlayVendorParameterExt, Tag::vocoderRate,
                CarPlayVendorParameterExt::Rate>(param);
        CarPlayVendorParameterExt::Rate aidlVocoderRate = VALUE_OR_RETURN_STATUS(p);
        LOG(DEBUG) << __func__ << " CP Vocoder Rate "<< toString(aidlVocoderRate) ;

        VocoderSampleRate = carplayParamConverter(aidlVocoderRate);
        LOG(DEBUG) << __func__ << " CP Vocoder Sample Rate "<<VocoderSampleRate;

        std::string keyvalue = param.id + "=" + VocoderSampleRate + ";";
        kvpairs.append(keyvalue);
        if (kvpairs.length() != keyvalue.length()) {
            LOG(ERROR) << __func__ << ": invalid kvpairs length";
            return ::android::BAD_VALUE;
        }
        LOG(DEBUG) << __func__ << " Key Value pairs: " << kvpairs;
        if (!kvpairs.empty()) {
            parms = str_parms_create_str(kvpairs.c_str());
            if (!parms) {
                return ::android::BAD_VALUE;
            }
            mAudExt.audio_extn_set_parameters(parms);
        }
    } else if (param.id == CarPlayVendorParameterExt::CARPLAY_TYPE) {
        auto p = extractParameter<CarPlayVendorParameterExt, Tag::type,
                CarPlayVendorParameterExt::Type>(param);
        CarPlayVendorParameterExt::Type aidlType = VALUE_OR_RETURN_STATUS(p);
        LOG(DEBUG) << __func__ << " CP Type "<< toString(aidlType);
        keyvalue = param.id + "=" + toString(aidlType) + ";";
        kvpairs.append(keyvalue);
        if (kvpairs.length() != keyvalue.length()) {
            LOG(ERROR) << __func__ << ": invalid kvpairs length";
            return ::android::BAD_VALUE;
        }
        LOG(DEBUG) << __func__ << " Key Value pairs: " << kvpairs;
        if (!kvpairs.empty()) {
            parms = str_parms_create_str(kvpairs.c_str());
            if (!parms) {
                return ::android::BAD_VALUE;
            }
            mAudExt.audio_extn_set_parameters(parms);
        }
    } else if (param.id == CarPlayVendorParameterExt::CARPLAY_TRANSPORT) {
        std:: string connection_type;
        auto p = extractParameter<CarPlayVendorParameterExt, Tag::transport,
                CarPlayVendorParameterExt::Transport>(param);
        CarPlayVendorParameterExt::Transport aidlTransport = VALUE_OR_RETURN_STATUS(p);
        LOG(DEBUG) << __func__ << " CP Transport "<< toString(aidlTransport);
        switch(aidlTransport) {
            case CarPlayVendorParameterExt::Transport::USB:
                connection_type = "0";
                break;
            case CarPlayVendorParameterExt::Transport::WIFI:
                connection_type = "1";
                break;
            default: connection_type = "Invalid";
                break;
        }
        keyvalue = param.id + "=" + connection_type + ";";
        kvpairs.append(keyvalue);
        if (kvpairs.length() != keyvalue.length()) {
            LOG(ERROR) << __func__ << ": invalid kvpairs length";
            return ::android::BAD_VALUE;
        }
        LOG(DEBUG) << __func__ << " Key Value pairs: " << kvpairs;
        if (!kvpairs.empty()) {
            parms = str_parms_create_str(kvpairs.c_str());
            if (!parms) {
                return ::android::BAD_VALUE;
            }
            mAudExt.audio_extn_set_parameters(parms);
        }
    } else if (param.id == CarPlayVendorParameterExt::CARPLAY_DUCK) {
        FocusInfo focusInfo = {};
        auto p = extractParameter<CarPlayVendorParameterExt, Tag::duckAudio,
                CarPlayVendorParameterExt::DuckAudio>(param);
        CarPlayVendorParameterExt::DuckAudio aidlDuckAudio = VALUE_OR_RETURN_STATUS(p);
        if (aidlDuckAudio.command == CarPlayVendorParameterExt::DuckAudio::DuckCommand::NONE) {
            return ::android::BAD_VALUE;
        }
        LOG(DEBUG) << __func__ << " CP Duck command "<< toString(aidlDuckAudio.command);

        if (aidlDuckAudio.command == CarPlayVendorParameterExt::DuckAudio::DuckCommand::DUCK) {
            if ((aidlDuckAudio.targetVolume < 0 && aidlDuckAudio.targetVolume > 1) || (aidlDuckAudio.rampDurationSec < 0 && aidlDuckAudio.rampDurationSec > 1)) {
                return ::android::BAD_VALUE;
            }
            focusInfo.usage = "CP_DUCK";
            focusInfo.gain = Vol_to_mdB(aidlDuckAudio.targetVolume) + static_cast<float>(getMedia_volume());
            LOG(DEBUG) << __func__ << " focusInfo.gain: " << focusInfo.gain;
            focusInfo.isExternalGain = true;
            focusInfo.rampDuration = aidlDuckAudio.rampDurationSec * 1000;
            mAudExt.mAutoAudioHalPriorityExtension->requestFocus(focusInfo, &focusSessionInfo.FocusId);
            LOG(DEBUG) << __func__ << " FocusId = " <<focusSessionInfo.FocusId << " rampduration: " << focusInfo.rampDuration;
        } else if (aidlDuckAudio.command == CarPlayVendorParameterExt::DuckAudio::DuckCommand::UNDUCK) {
            if (aidlDuckAudio.rampDurationSec < 0 && aidlDuckAudio.rampDurationSec > 1) {
                return ::android::BAD_VALUE;
            }
            focusInfo.rampDuration = aidlDuckAudio.rampDurationSec * 1000;
            LOG(DEBUG) << __func__ <<" rampduration: " << focusInfo.rampDuration;
            mAudExt.mAutoAudioHalPriorityExtension->abandonFocus(focusSessionInfo.FocusId);
        }
    } else {
        LOG(ERROR) << __func__ << ": unhandled parameter id " << param.id.c_str();
        return ::android::BAD_VALUE;
    }
    return ::android::OK;
}

void ModulePrimary::onSetAudioControlParameters(const std::vector<::aidl::android::hardware::audio::core::VendorParameter>& params) {
    for (const auto& param : params) {
        if (setAudioControlParameter(param) != ::android::OK) {
            LOG(ERROR) << __func__ << ": FAILED to extract value from " << param.id.c_str();
        }
    }
}

const std::map<int,int> volumeMap = {
    {-9000, 0}, {-8775, 1}, {-8550, 2}, {-8325, 3},
    {-8100, 4}, {-7875, 5}, {-7650, 6}, {-7425, 7},
    {-7200, 8}, {-6975, 9}, {-6750, 10}, {-6525, 11},
    {-6300, 12}, {-6075, 13}, {-5850, 14}, {-5625, 15},
    {-5400, 16}, {-5175, 17}, {-4950, 18}, {-4725, 19},
    {-4500, 20}, {-4275, 21}, {-4050, 22}, {-3825, 23},
    {-3600, 24}, {-3375, 25}, {-3150, 26}, {-2925, 27},
    {-2700, 28}, {-2475, 29}, {-2250, 30}, {-2025, 31},
    {-1800, 32}, {-1575, 33}, {-1350, 34}, {-1125, 35},
    {-900, 36}, {-675, 37}, {-450, 38}, {-225, 39}, {0, 40}};

int getNearestIndex(int gain) {
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

void ModulePrimary::setUpPriorityFocus() {
    auto &mActiveFocusDevices = ModulePrimary::mActiveFocusDevices;
    const auto& configs = getConfig().portConfigs;
    for (const auto& portConfig : configs) {
        if (portConfig.ext.getTag() == AudioPortExt::device) {
            if (auto devicePort = portConfig.ext.get<AudioPortExt::device>();
                    (devicePort.device.type.type == AudioDeviceType::OUT_BUS &&
                     devicePort.device.type.connection.empty())) {

                if (auto address = devicePort.device.address.get<AudioDeviceAddress::Tag::id>();
                        !address.empty()) {
                    if (mActiveFocusDevices.find(address) == mActiveFocusDevices.end()) {
                        FocusSession focusSessionInfo;
                        FocusInfo focusInfo;
                        focusInfo.usage = address;
                        focusInfo.gain = 0.0;
                        focusInfo.device = devicePort.device;
                        mAudExt.mAutoAudioHalPriorityExtension->requestFocus(focusInfo, &focusSessionInfo.FocusId);
                        mActiveFocusDevices.insert(make_pair(address, focusSessionInfo));
                    }
                }
            }
        }
    }

}

ndk::ScopedAStatus ModulePrimary::handleMasterMute(
        const AudioControlVendorParameterExt::MasterMuteRequest& request) {
    LOG(ERROR) << __func__ << ": request " << request.toString();
    ModulePrimary::setUpPriorityFocus();
    std::vector<AudioGainConfigInfo> agcis = getAudioGainConfigsForSinks();

    std::vector<Reasons> reasons{};
    static FocusSession focusSessionInfo;

    if (request.state == AudioControlVendorParameterExt::MasterMuteRequest::State::ACTIVATED) {
        {
            FocusInfo focusInfo;
            focusInfo.usage = request.type;
            focusInfo.gain = 0.0;
            mAudExt.mAutoAudioHalPriorityExtension->requestFocus(focusInfo, &focusSessionInfo.FocusId);
            LOG(INFO) << "Focus Id: " << focusSessionInfo.FocusId;
        }

    } else {
        LOG(INFO) << __func__ << "Abandoning focus for mastermute, focus id: " << focusSessionInfo.FocusId;
        mAudExt.mAutoAudioHalPriorityExtension->abandonFocus(focusSessionInfo.FocusId);
    }


    return ndk::ScopedAStatus::ok();
}

std::vector<AudioGainConfigInfo> ModulePrimary::getAudioGainConfigsForSinks() {
    const auto& configs = getConfig().portConfigs;
    std::vector<AudioGainConfigInfo> agcis{};
    // Find sinks device ports
    for (const auto& portConfig : configs) {
        if (portConfig.ext.getTag() == AudioPortExt::device) {
            if (auto devicePort = portConfig.ext.get<AudioPortExt::device>();
                    devicePort.device.type.type == AudioDeviceType::OUT_DEVICE ||
                    (devicePort.device.type.type == AudioDeviceType::OUT_BUS &&
                     devicePort.device.type.connection.empty())) {
                if (auto address = devicePort.device.address.get<AudioDeviceAddress::Tag::id>();
                        !address.empty()) {
                    AudioGainConfigInfo agci{
                            .zoneId = 0,
                            .devicePortAddress = address,
                            .volumeIndex = 0,
                    };
                    agcis.push_back(agci);
                }
            }
        }
    }
    return agcis;
}

::android::status_t ModulePrimary::setAudioControlParameter(const ::aidl::android::hardware::audio::core::VendorParameter& param) {
    LOG(DEBUG) << __func__  ;
    static FocusSession focusSessionInfo;
    using Tag = AudioControlVendorParameterExt::Parameter::Tag;
    if (param.id ==  AudioControlVendorParameterExt::MASTER_MUTE_REQUEST) {
        auto p = extractParameter<AudioControlVendorParameterExt, Tag::masterMuteRequest,
                AudioControlVendorParameterExt::MasterMuteRequest>(param);
        AudioControlVendorParameterExt::MasterMuteRequest muteRequest =
                VALUE_OR_RETURN_STATUS(p);
        handleMasterMute(muteRequest);
        LOG(DEBUG) << __func__ << " handleMasterMute";

    } else if (param.id == AudioControlVendorParameterExt::REQUEST_AUDIO_FOCUS) {
        auto p = extractParameter<AudioControlVendorParameterExt, Tag::requestAudioFocus,
                AudioControlVendorParameterExt::AudioFocusRequest>(param);
        AudioControlVendorParameterExt::AudioFocusRequest focusRequest =
                VALUE_OR_RETURN_STATUS(p);

        LOG(DEBUG) << __func__ << " Focus request "<< focusRequest.toString();
        FocusInfo focusInfo;
        FocusSession focusSessioninfo((int64_t)(focusRequest.soundId));
        focusInfo.usage = focusRequest.useCase;
        focusInfo.gain = -1000.0;
        focusInfo.muteOrderType = focusRequest.muteOrderType;
        mAudExt.mAutoAudioHalPriorityExtension->requestFocus(focusInfo, &focusSessioninfo.FocusId);
        LOG(INFO) << "Focus Id: " << focusRequest.soundId;

    } else if (param.id == AudioControlVendorParameterExt::ABANDON_AUDIO_FOCUS) {
        auto p = extractParameter<AudioControlVendorParameterExt, Tag::abandonAudioFocus,
                AudioControlVendorParameterExt::AudioFocusAbandon>(param);
        AudioControlVendorParameterExt::AudioFocusAbandon focusAbandon =
                VALUE_OR_RETURN_STATUS(p);
        LOG(DEBUG) << __func__ << " Focus Abandon request "<< focusAbandon.toString();
        mAudExt.mAutoAudioHalPriorityExtension->abandonFocus((int64_t)focusAbandon.soundId);

    } else if (param.id == AudioControlVendorParameterExt::BALANCE) {
        float value = 0.0;
        if (!extractParameter<Float>(param, &value)) {
            LOG(ERROR) << __func__ << ": FAILED extract value from key " << param.id.c_str();
            return ::android::BAD_VALUE;
        }
        else
        {
            struct str_parms* parms = NULL;
            std::string kvpairs;
            std::string keyvalue = param.id + "=" + std::to_string(value) + ";";
            kvpairs.append(keyvalue);
            if (kvpairs.length() != keyvalue.length()) {
                LOG(ERROR) << __func__ << ": invalid kvpairs length";
                return ::android::BAD_VALUE;
            }
            LOG(DEBUG) << __func__ << " Key Value pairs: " << kvpairs;
            if (!kvpairs.empty()) {
                parms = str_parms_create_str(kvpairs.c_str());
                if (!parms) {
                    return ::android::BAD_VALUE;
                }
                mAudExt.audio_extn_set_parameters(parms);
            }
        }
        LOG(DEBUG) << __func__ << " Balance "<< value;
    } else if (param.id == AudioControlVendorParameterExt::FADER) {
        float value = 0.0;
        if (!extractParameter<Float>(param, &value)) {
            LOG(ERROR) << __func__ << ": FAILED extract value from key " << param.id.c_str();
            return ::android::BAD_VALUE;
        }
        else
        {
            struct str_parms* parms = NULL;
            std::string kvpairs ;
            std::string keyvalue = param.id + "=" + std::to_string(value) + ";";
            kvpairs.append(keyvalue);
            if (kvpairs.length() != keyvalue.length()) {
                LOG(ERROR) << __func__ << ": invalid kvpairs length";
                return ::android::BAD_VALUE;
            }
            LOG(DEBUG) << __func__ << " Key Value pairs: " << kvpairs;
            if (!kvpairs.empty()) {
                parms = str_parms_create_str(kvpairs.c_str());
                if (!parms) {
                    return ::android::BAD_VALUE;
                }
                mAudExt.audio_extn_set_parameters(parms);
            }
        }
        LOG(DEBUG) << __func__ << " Fader "<< value;
    } else {
        LOG(ERROR) << __func__ << ": unhandled parameter id " << param.id.c_str();
        return ::android::BAD_VALUE;
    }

    return ::android::OK;
}
void ModulePrimary::onsetRadioVendorParameter(const std::vector<::aidl::android::hardware::audio::core::VendorParameter>& params) {
    for (const auto& param : params) {
        if (setRadioVendorParameter(param) != ::android::OK) {
            LOG(ERROR) << __func__ << ": FAILED to extract value from " << param.id.c_str();
        }
    }
}


::android::status_t ModulePrimary::setRadioVendorParameter(const ::aidl::android::hardware::audio::core::VendorParameter& param) {
    using Tag = RadioVendorParameterExt::Parameter::Tag;
    if (param.id == RadioVendorParameterExt::AUDIO_SOURCE) {
        auto p = extractParameter<RadioVendorParameterExt, Tag::audioSource,
                RadioVendorParameterExt::AudioSource>(param);
        if (!p.has_value()) {
            LOG(ERROR) << __func__ << ": Failed to extract parameter";
            return p.error();
        }
        RadioVendorParameterExt::AudioSource aidlAudioSource = p.value();
        globalAudioSource = toString(aidlAudioSource);
        LOG(DEBUG) << __func__ << " AUDIO_SOURCE: " << globalAudioSource;
        struct str_parms* parms = NULL;
        std::string kvpairs = "";
        std::string keyvalue = param.id + "=" + globalAudioSource + ";";
        kvpairs.append(keyvalue);
        if (kvpairs.length() != keyvalue.length()) {
            LOG(ERROR) << __func__ << ": invalid kvpairs length";
            return ::android::BAD_VALUE;
        }
        LOG(DEBUG) << __func__ << " Key Value pairs: " << kvpairs;
        if (!kvpairs.empty()) {
            parms = str_parms_create_str(kvpairs.c_str());
            if (!parms) {
                return ::android::BAD_VALUE;
            }
            mAudExt.audio_extn_set_parameters(parms);
        }
    } else {
        LOG(ERROR) << __func__ << ": Unknown parameter id " << param.id.c_str();
        return ::android::BAD_VALUE;
    }
    return ::android::OK;
}
#endif

ndk::ScopedAStatus ModulePrimary::setVendorParameters(
        const std::vector<::aidl::android::hardware::audio::core::VendorParameter>& in_parameters,
        bool in_async) {
    LOG(VERBOSE) << __func__ << ": parameter count " << in_parameters.size()
               << ", async: " << in_async;
    for (const auto& p : in_parameters) {
        if (p.id == VendorDebug::kForceTransientBurstName) {
            if (!extractParameter<Boolean>(p, &mVendorDebug.forceTransientBurst)) {
                return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
            }
        } else if (p.id == VendorDebug::kForceSynchronousDrainName) {
            if (!extractParameter<Boolean>(p, &mVendorDebug.forceSynchronousDrain)) {
                return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
            }
        } else {
            struct str_parms* parms = NULL;
            std::string kvpairs = getkvPairsForVendorParameter(in_parameters);
            if (!kvpairs.empty()) {
                parms = str_parms_create_str(kvpairs.c_str());
                if (!parms) {
                    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
                }
                mAudExt.audio_extn_set_parameters(parms);
            }

            mPlatform.setVendorParameters(in_parameters, in_async);
        }
    }
    processSetVendorParameters(in_parameters);
    return ndk::ScopedAStatus::ok();
}

bool ModulePrimary::processSetVendorParameters(const std::vector<VendorParameter>& parameters) {
    FeatureToVendorParametersMap pendingActions{};
    for (const auto& p : parameters) {
        const auto searchId = mSetParameterToFeatureMap.find(p.id);
        if (searchId == mSetParameterToFeatureMap.cend()) {
            LOG(VERBOSE) << __func__ << ": not configured " << p.id;
            continue;
        }

        auto itr = pendingActions.find(searchId->second);
        if (itr == pendingActions.cend()) {
            pendingActions[searchId->second] = std::vector<VendorParameter>({p});
            continue;
        }
        itr->second.push_back(p);
    }

    for (const auto & [ key, value ] : pendingActions) {
        const auto search = mFeatureToSetHandlerMap.find(key);
        if (search == mFeatureToSetHandlerMap.cend()) {
            LOG(VERBOSE) << __func__
                         << ": no handler set on Feature:" << static_cast<int>(search->first);
            continue;
        }
        auto handler = std::bind(search->second, this, value);
        handler(); // a dynamic dispatch to a SetHandler
    }
    return true;
}

void ModulePrimary::onSetGenericParameters(const std::vector<VendorParameter>& params) {
    for (const auto& param : params) {
        std::string paramValue{};
        if (!extractParameter<VString>(param, &paramValue)) {
            LOG(ERROR) << ": extraction failed for " << param.id;
            continue;
        }
        if (Parameters::kInCallMusic == param.id) {
            const auto isOn = getBoolFromString(paramValue);
            mPlatform.setInCallMusicState(isOn);
            LOG(INFO) << __func__ << ": ICMD playback:" << isOn;
        } else if (Parameters::kUHQA == param.id) {
            const bool enable = paramValue == "on" ? true : false;
            mPlatform.updateUHQA(enable);
        } else if (Parameters::kTranslateRecord == param.id) {
            // Add Translate_Record param check and update using the Set Function
            const auto isOn = getBoolFromString(paramValue);
            mPlatform.setTranslationRecordState(isOn);
            LOG(INFO) << __func__ << ": PCM Record FFECNS for Translation:" << isOn;
        }
    }
}


void MuteConfig::set_mute_config_for_address(char* address, bool muted, float volume) {
    LOG(DEBUG) << __func__ << ": Enter, muted: " << muted << ", address: " << address;
    bool is_muted = false;

    ModulePrimary::outListMutex.lock();

    for (auto weakStream : ModulePrimary::getOutStreams()) {
        if (weakStream.expired()) {
            LOG(DEBUG) << "stream empty: ";
        }
        auto stream = weakStream.lock();
        if (stream) {
            auto streamOutPrimary = std::static_pointer_cast<::qti::audio::core::StreamOutPrimary>(stream);
            if (std::strcmp(streamOutPrimary->getAddress().c_str(), address) == 0) {
                LOG(DEBUG) << "Mute applied to stream with address: " << address;
                if (muted) {
                   if(!is_muted) {
                        (std::static_pointer_cast<::qti::audio::core::StreamOutPrimary>(stream))->getHwVolume(&getVol);
                        is_muted  = true;
                    }
                    std::vector<float> vol;
                    LOG(DEBUG)<<"gain is:"<<volume;
                    int channel=getVol.size(),i;
                    for (i=0;i<channel;i++)
                        vol.push_back(volume);
                    LOG(DEBUG)<<"gain is:"<<::android::internal::ToString(vol);

                    (std::static_pointer_cast<::qti::audio::core::StreamOutPrimary>(stream))->setPALVolume(vol);
                    LOG(DEBUG)<<"volume set :"<<vol[0];
               } else {
                    (std::static_pointer_cast<::qti::audio::core::StreamOutPrimary>(stream))->setPALVolume(getVol);
                    is_muted  = false;
                }
            }
            stream.reset();
       } else {
            LOG(DEBUG) << "failed to generate shared pointer";
        }
    }
    ModulePrimary::outListMutex.unlock();
}

extern "C" __attribute__((visibility("default"))) void extn_set_mute_config_for_address(char* address, bool muted, float volume)
{
    LOG(DEBUG)<< __func__ << " mute:" << muted  << "address:" << address;
    auto& muteConfigInst = MuteConfig::GetInstance();
    return muteConfigInst.set_mute_config_for_address(address, muted, volume);
}

void ModulePrimary::onSetHDRParameters(const std::vector<VendorParameter>& params) {
    for (const auto& param : params) {
        std::string paramValue{};
        if (!extractParameter<VString>(param, &paramValue)) {
            LOG(ERROR) << __func__ << ": extraction failed for " << param.id;
            continue;
        }
        if (param.id == Parameters::kHdrRecord) {
            mPlatform.setHDREnabled(paramValue == "true");
        } else if (param.id == Parameters::kHdrSamplingRate) {
            mPlatform.setHDRSampleRate(static_cast<int32_t>(getInt64FromString(paramValue)));
        } else if (param.id == Parameters::kHdrChannelCount) {
            mPlatform.setHDRChannelCount(static_cast<int32_t>(getInt64FromString(paramValue)));
        } else if (param.id == Parameters::kWnr) {
            mPlatform.setWNREnabled(paramValue == "true");
        } else if (param.id == Parameters::kAns) {
            mPlatform.setANREnabled(paramValue == "true");
        } else if (param.id == Parameters::kOrientation) {
            mPlatform.setOrientation(paramValue);
        } else if (param.id == Parameters::kInverted) {
            mPlatform.setInverted(paramValue == "true");
        } else if (param.id == Parameters::kFacing) {
            mPlatform.setFacing(paramValue);
        }
    }
    LOG(VERBOSE) << __func__ << ": processed";
}

void ModulePrimary::onSetTelephonyParameters(const std::vector<VendorParameter>& parameters) {
    if (!mTelephony) {
        LOG(ERROR) << __func__ << ": Telephony not created";
        return;
    }

    Telephony::SetUpdates setUpdates{};
    bool isSetUpdate = false;

    bool isDeviceMuted = false;
    std::string muteDirection{""};
    bool isDeviceMuteUpdate = false;

    for (const auto& p : parameters) {
        std::string paramValue{};
        if (!extractParameter<VString>(p, &paramValue)) {
            LOG(ERROR) << ": extraction failed for " << p.id;
            continue;
        }
        if (Parameters::kVoiceCallState == p.id) {
            setUpdates.mCallState =
                    static_cast<Telephony::CallState>(getInt64FromString(paramValue));
            isSetUpdate = true;
        } else if (Parameters::kVoiceVSID == p.id) {
            setUpdates.mVSID = static_cast<Telephony::VSID>(getInt64FromString(paramValue));
            isSetUpdate = true;
        } else if (Parameters::kVoiceCallType == p.id) {
            setUpdates.mCallType = std::move(paramValue);
            isSetUpdate = true;
        } else if (Parameters::kVoiceCRSCall == p.id) {
            setUpdates.mIsCrsCall = paramValue == "true" ? true : false;
            isSetUpdate = true;
        } else if (Parameters::kVoiceCRSVolume == p.id) {
            mTelephony->setCRSVolumeFromIndex(getInt64FromString(paramValue));
        } else if (Parameters::kVolumeBoost == p.id) {
            const bool enable = paramValue == "on" ? true : false;
            mTelephony->updateVolumeBoost(enable);
        } else if (Parameters::kVoiceSlowTalk == p.id) {
            const bool enable = paramValue == "true" ? true : false;
            mTelephony->updateSlowTalk(enable);
        } else if (Parameters::kVoiceHDVoice == p.id) {
            const bool enable = paramValue == "true" ? true : false;
            mTelephony->updateHDVoice(enable);
        } else if (Parameters::kVoiceDeviceMute == p.id) {
            isDeviceMuted = paramValue == "true" ? true : false;
            isDeviceMuteUpdate = true;
        } else if (Parameters::kVoiceDirection == p.id) {
            muteDirection = paramValue;
        } else if (Parameters::kVoiceTranslationRxMute == p.id) {
            const auto isOn = getBoolFromString(paramValue);
            mPlatform.setTranslationRxMuteState(isOn);
            LOG(DEBUG) << __func__ << " : translation Rx mute set as true" ;
            mTelephony->updateVoiceVolume();
        }
    }

    if (isSetUpdate) {
        mTelephony->reconfigure(setUpdates);
    }
    if (isDeviceMuteUpdate) {
        mTelephony->updateDeviceMute(isDeviceMuted, muteDirection);
    }

    return;
}

void ModulePrimary::onSetWFDParameters(const std::vector<VendorParameter>& parameters) {
    for (const auto& p : parameters) {
        std::string paramValue{};
        if (!extractParameter<VString>(p, &paramValue)) {
            LOG(ERROR) << ": extraction failed for " << p.id;
            continue;
        }
        if (Parameters::kWfdChannelMap == p.id) {
            auto numProxyChannels = static_cast<uint32_t>(getInt64FromString(paramValue));
            mPlatform.setWFDProxyChannels(numProxyChannels);
        } else if (Parameters::kWfdIPAsProxyDevConnected == p.id) {
            auto isIPAsProxy = getBoolFromString(paramValue);
            mPlatform.setIPAsProxyDeviceConnected(isIPAsProxy);
        } else if (Parameters::kProxyRecordFMQSize == p.id) {
            const size_t& proxyRecordFMQSize = static_cast<int32_t>(getInt64FromString(paramValue));
            mPlatform.setProxyRecordFMQSize(proxyRecordFMQSize);
        }
    }
    return;
}

void ModulePrimary::onSetFTMParameters(const std::vector<VendorParameter>& parameters) {
    auto itrForCfgWaitTime =
            std::find_if(parameters.cbegin(), parameters.cend(),
                         [](const auto& p) { return p.id == Parameters::kFbspCfgWaitTime; });
    auto itrForFTMWaitTime =
            std::find_if(parameters.cbegin(), parameters.cend(),
                         [](const auto& p) { return p.id == Parameters::kFbspFTMWaitTime; });
    auto itrForValiWaitTime =
            std::find_if(parameters.cbegin(), parameters.cend(),
                         [](const auto& p) { return p.id == Parameters::kFbspValiWaitTime; });
    auto itrForValiValiTime =
            std::find_if(parameters.cbegin(), parameters.cend(),
                         [](const auto& p) { return p.id == Parameters::kFbspValiValiTime; });
    auto itrForTriggerSpeakerCall =
            std::find_if(parameters.cbegin(), parameters.cend(),
                         [](const auto& p) { return p.id == Parameters::kTriggerSpeakerCall; });

    if (itrForCfgWaitTime != parameters.cend() && itrForFTMWaitTime != parameters.cend()) {
        std::string heatTime{}, runTime{};
        if ((!extractParameter<VString>(*itrForCfgWaitTime, &heatTime)) ||
            (!extractParameter<VString>(*itrForFTMWaitTime, &runTime))) {
            LOG(ERROR) << __func__ << ": extraction failed!!!";
            return;
        }
        mPlatform.setFTMSpeakerProtectionMode(static_cast<uint32_t>(getInt64FromString(heatTime)),
                                              static_cast<uint32_t>(getInt64FromString(runTime)),
                                              true, false, false);
    } else if (itrForValiWaitTime != parameters.cend() && itrForValiValiTime != parameters.cend()) {
        std::string heatTime{}, runTime{};
        if ((!extractParameter<VString>(*itrForValiWaitTime, &heatTime)) ||
            (!extractParameter<VString>(*itrForValiValiTime, &runTime))) {
            LOG(ERROR) << __func__ << ": extraction failed!!!";
            return;
        }
        mPlatform.setFTMSpeakerProtectionMode(static_cast<uint32_t>(getInt64FromString(heatTime)),
                                              static_cast<uint32_t>(getInt64FromString(runTime)),
                                              false, true, false);
    } else if (itrForTriggerSpeakerCall != parameters.cend()) {
        mPlatform.setFTMSpeakerProtectionMode(0, 0, false, false, true);
    }

    return;
}

void ModulePrimary::onSetHapticsParameters(const std::vector<VendorParameter>& parameters) {
    for (const auto& param : parameters) {
        std::string paramValue{};
        if (!extractParameter<VString>(param, &paramValue)) {
            LOG(ERROR) << ": extraction failed for " << param.id;
            continue;
        }
        if (Parameters::kHapticsVolume == param.id) {
            const float hapticsVolume = getFloatFromString(paramValue);
            mPlatform.setHapticsVolume(hapticsVolume);
        } else if (Parameters::kHapticsIntensity == param.id) {
            const int hapticsIntensity = getInt64FromString(paramValue);
            mPlatform.setHapticsIntensity(hapticsIntensity);
        }
    }
    return;
}

// static
ModulePrimary::SetParameterToFeatureMap ModulePrimary::fillSetParameterToFeatureMap() {
    SetParameterToFeatureMap map{{Parameters::kHdrRecord, Feature::HDR},
                                 {Parameters::kWnr, Feature::HDR},
                                 {Parameters::kAns, Feature::HDR},
                                 {Parameters::kOrientation, Feature::HDR},
                                 {Parameters::kInverted, Feature::HDR},
                                 {Parameters::kHdrChannelCount, Feature::HDR},
                                 {Parameters::kHdrSamplingRate, Feature::HDR},
                                 {Parameters::kFacing, Feature::HDR},
                                 {Parameters::kVoiceCallState, Feature::TELEPHONY},
                                 {Parameters::kVoiceCallType, Feature::TELEPHONY},
                                 {Parameters::kVoiceVSID, Feature::TELEPHONY},
                                 {Parameters::kVoiceCRSCall, Feature::TELEPHONY},
                                 {Parameters::kVoiceCRSVolume, Feature::TELEPHONY},
                                 {Parameters::kVolumeBoost, Feature::TELEPHONY},
                                 {Parameters::kVoiceSlowTalk, Feature::TELEPHONY},
                                 {Parameters::kVoiceHDVoice, Feature::TELEPHONY},
                                 {Parameters::kVoiceDeviceMute, Feature::TELEPHONY},
                                 {Parameters::kVoiceDirection, Feature::TELEPHONY},
                                 {Parameters::kVoiceTranslationRxMute, Feature::TELEPHONY},
                                 {Parameters::kInCallMusic, Feature::GENERIC},
                                 {Parameters::kTranslateRecord, Feature::GENERIC},
                                 {Parameters::kUHQA, Feature::GENERIC},
                                 {Parameters::kFbspCfgWaitTime, Feature::FTM},
                                 {Parameters::kFbspFTMWaitTime, Feature::FTM},
                                 {Parameters::kFbspValiWaitTime, Feature::FTM},
                                 {Parameters::kFbspValiValiTime, Feature::FTM},
                                 {Parameters::kTriggerSpeakerCall, Feature::FTM},
                                 {Parameters::kWfdChannelMap, Feature::WFD},
                                 {Parameters::kWfdIPAsProxyDevConnected, Feature::WFD},
                                 {Parameters::kProxyRecordFMQSize, Feature::WFD},
                                 {Parameters::kHapticsVolume, Feature::HAPTICS},
                                 {Parameters::kHapticsIntensity, Feature::HAPTICS},
#ifdef ENABLE_QCOM_AMPERE_AUDIO
                                 {AudioControlVendorParameterExt::MASTER_MUTE_REQUEST, Feature::AUDIOCONTROL},
                                 {AudioControlVendorParameterExt::REQUEST_AUDIO_FOCUS, Feature::AUDIOCONTROL},
                                 {AudioControlVendorParameterExt::ABANDON_AUDIO_FOCUS, Feature::AUDIOCONTROL},
                                 {RadioVendorParameterExt::AUDIO_SOURCE, Feature::AUDIOSOURCE},
                                 {CarPlayVendorParameterExt::CARPLAY_TRANSPORT, Feature::CARPLAY},
                                 {CarPlayVendorParameterExt::CARPLAY_TYPE, Feature::CARPLAY},
                                 {CarPlayVendorParameterExt::CARPLAY_SAMPLERATE, Feature::CARPLAY},
                                 {CarPlayVendorParameterExt::CARPLAY_VOCODER_SAMPLERATE, Feature::CARPLAY},
                                 {CarPlayVendorParameterExt::CARPLAY_DUCK, Feature::CARPLAY},
                                 {AudioControlVendorParameterExt::BALANCE, Feature::AUDIOCONTROL},
                                 {AudioControlVendorParameterExt::FADER, Feature::AUDIOCONTROL},
#endif
    };
    return map;
}

// static
ModulePrimary::FeatureToSetHandlerMap ModulePrimary::fillFeatureToSetHandlerMap() {
    FeatureToSetHandlerMap map{
            {Feature::GENERIC, &ModulePrimary::onSetGenericParameters},
            {Feature::HDR, &ModulePrimary::onSetHDRParameters},
            {Feature::TELEPHONY, &ModulePrimary::onSetTelephonyParameters},
            {Feature::WFD, &ModulePrimary::onSetWFDParameters},
            {Feature::FTM, &ModulePrimary::onSetFTMParameters},
            {Feature::HAPTICS, &ModulePrimary::onSetHapticsParameters},
#ifdef ENABLE_QCOM_AMPERE_AUDIO
            {Feature::AUDIOSOURCE, &ModulePrimary::onsetRadioVendorParameter},
            {Feature::CARPLAY, &ModulePrimary::onSetCarplayParameters},
            {Feature::AUDIOCONTROL, &ModulePrimary::onSetAudioControlParameters},


#endif
    };
    return map;
}

ndk::ScopedAStatus ModulePrimary::getVendorParameters(
        const std::vector<std::string>& in_ids,
        std::vector<::aidl::android::hardware::audio::core::VendorParameter>* _aidl_return) {
    LOG(DEBUG) << __func__ << ": id count: " << in_ids.size();
    std::vector<VendorParameter> result{};
    for (const auto& id : in_ids) {
        if (id == VendorDebug::kForceTransientBurstName) {
            VendorParameter forceTransientBurst{.id = id};
            forceTransientBurst.ext.setParcelable(Boolean{mVendorDebug.forceTransientBurst});
            _aidl_return->push_back(std::move(forceTransientBurst));
        } else if (id == VendorDebug::kForceSynchronousDrainName) {
            VendorParameter forceSynchronousDrain{.id = id};
            forceSynchronousDrain.ext.setParcelable(Boolean{mVendorDebug.forceSynchronousDrain});
            _aidl_return->push_back(std::move(forceSynchronousDrain));
        }
        #ifndef ENABLE_QCOM_AMPERE_AUDIO
         else if ((id == AUDIO_PARAMETER_KEY_BALANCE) || (id == AUDIO_PARAMETER_KEY_FADER)) {
            LOG(DEBUG) << __func__ << ": " << id;
            auto value = mAudExt.audio_extn_get_parameters(id);
            result.emplace_back(makeVendorParameter(id, std::to_string(value)));
            *_aidl_return = result;
            return ndk::ScopedAStatus::ok();
        } else if (id == AUDIO_PARAMETER_KEY_ISFADERAVAILABLE) {
            LOG(DEBUG) << __func__ << ": " << id;
            std::string enablevalue{};
            auto istrue = mAudExt.audio_extn_get_parameters(id);
            if (istrue) {
                enablevalue = "true";
            } else {
                enablevalue = "false";
            }
            result.emplace_back(makeVendorParameter(id, enablevalue));
            *_aidl_return = result;
            return ndk::ScopedAStatus::ok();
        }
        #endif
    }

    auto results = processGetVendorParameters(in_ids);
    std::move(results.begin(), results.end(), std::back_inserter(*_aidl_return));

    if (_aidl_return->size() != in_ids.size()) {
        LOG(ERROR) << __func__ << ": handled parameters " << _aidl_return->size()
                   << " requested " << in_ids.size();
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    return ndk::ScopedAStatus::ok();
}

std::vector<VendorParameter> ModulePrimary::processGetVendorParameters(
        const std::vector<std::string>& ids) {
    FeatureToStringMap pendingActions{};
    // only group of features are mapped to Feature, rest are kept as generic.
    // If the key is found in feature map, use the feature otherwise call GENERIC feature.
    for (const auto& id : ids) {
        auto search = mGetParameterToFeatureMap.find(id);
        Feature mappedFeature = Feature::GENERIC;
        if (search != mGetParameterToFeatureMap.cend()) {
            mappedFeature = search->second;
        }
        auto itr = pendingActions.find(mappedFeature);
        if (itr == pendingActions.cend()) {
            pendingActions[mappedFeature] = std::vector<std::string>({id});
            continue;
        }
        itr->second.push_back(id);
    }

    std::vector<VendorParameter> result{};
    for (const auto & [ key, value ] : pendingActions) {
        const auto search = mFeatureToGetHandlerMap.find(key);
        if (search == mFeatureToGetHandlerMap.cend()) {
            LOG(ERROR) << __func__
                       << ": no handler set on Feature:" << static_cast<int>(search->first);
            continue;
        }
        auto handler = std::bind(search->second, this, value);
        auto keyResult = handler(); // a dynamic dispatch to GetHandler
        result.insert(result.end(), keyResult.begin(), keyResult.end());
    }
    return result;
}

#ifdef ENABLE_QCOM_AMPERE_AUDIO

std::vector<VendorParameter> ModulePrimary::onGetCarplayParams(
        const std::vector<std::string>& ids) {
    bool allParametersKnown = true;
    std::vector<VendorParameter> results{};
    for (const auto& id : ids) {
        if (id == CarPlayVendorParameterExt::CARPLAY_SAMPLERATE) {
            VendorParameter returnParameter{.id = id};
            auto ptr = std::make_shared<CarPlayVendorParameterExt>();
            using Tag = CarPlayVendorParameterExt::Parameter::Tag;
            ptr->value = CarPlayVendorParameterExt::Parameter::make<Tag::sampleRate>(
                    CarPlayVendorParameterExt::Rate::KHZ_32);
            returnParameter.ext.setParcelable(*ptr.get());
            results.push_back(returnParameter);
        } else if (id == CarPlayVendorParameterExt::CARPLAY_DUCK) {
            VendorParameter returnParameter{.id = id};
            auto ptr = std::make_shared<CarPlayVendorParameterExt>();
            using Tag = CarPlayVendorParameterExt::Parameter::Tag;
            ptr->value = CarPlayVendorParameterExt::Parameter::make<Tag::duckAudio>(
                    CarPlayVendorParameterExt::DuckAudio::DuckCommand::DUCK, 150, 15);
            returnParameter.ext.setParcelable(*ptr.get());
            results.push_back(returnParameter);
        } else {
            allParametersKnown = false;
            LOG(VERBOSE) << __func__ << ": unhandled parameter \"" << id << "\"";
        }
    }
    if (!allParametersKnown) {
        LOG(ERROR) << __func__ << ": unhandled parameter dispatched to CarPlay";
    }
    return results;
}

std::vector<::aidl::android::hardware::audio::core::VendorParameter> ModulePrimary::onGetAudioControlParams(
    const std::vector<std::string>& ids) {
    LOG(DEBUG) << __func__ << "Entry";
    bool allParametersKnown = true;
    std::vector<VendorParameter> results{};
    for (const auto& id : ids) {

    if (id == AudioControlVendorParameterExt::FADER_AVAILABILITY) {
        auto faderAvailability  = mAudExt.audio_extn_get_parameters(id) ;
        bool faderValue = static_cast<bool>(faderAvailability);
        LOG(DEBUG) << __func__ << "faderAvailability Value " << faderValue ;
        VendorParameter parameter{.id = AudioControlVendorParameterExt::FADER_AVAILABILITY};
        AudioControlVendorParameterExt myExtension;
        myExtension.value = AudioControlVendorParameterExt::Parameter::make<AudioControlVendorParameterExt::Parameter::Tag::faderAvailable>(faderValue);
        parameter.ext.setParcelable(myExtension);
        results.push_back(parameter);
        return results;
    } else if (id == AudioControlVendorParameterExt::FADER) {
        auto fader  = mAudExt.audio_extn_get_parameters(id) ;
        float faderLevel = fader/BALANCE_FADER_SCALE;
        LOG(DEBUG) << __func__ << "faderLevel Value " << faderLevel ;
        VendorParameter parameter{.id = AudioControlVendorParameterExt::FADER};
        AudioControlVendorParameterExt myExtension;
        myExtension.value = AudioControlVendorParameterExt::Parameter::make<AudioControlVendorParameterExt::Parameter::Tag::fader>(faderLevel);
        parameter.ext.setParcelable(myExtension);
        results.push_back(parameter);
        return results;

    }
    else if (id == AudioControlVendorParameterExt::BALANCE) {
        auto balance  = mAudExt.audio_extn_get_parameters(id) ;
        float balanceLevel = balance/BALANCE_FADER_SCALE;
        LOG(DEBUG) << __func__ << "balanceLevel Value " << balanceLevel ;
        VendorParameter parameter{.id = AudioControlVendorParameterExt::BALANCE};
        AudioControlVendorParameterExt myExtension;
        myExtension.value = AudioControlVendorParameterExt::Parameter::make<AudioControlVendorParameterExt::Parameter::Tag::balance>(balanceLevel);
        parameter.ext.setParcelable(myExtension);
        results.push_back(parameter);
    }
    else
    {
        LOG(ERROR) << __func__ << ": unhandled parameter dispatched to AudioControl";
    }
}

LOG(DEBUG) << __func__ << "Exit";
return results;
}
#endif

std::vector<VendorParameter> ModulePrimary::onGetAudioExtnParams(
        const std::vector<std::string>& ids) {
    std::vector<VendorParameter> results{};
    for (const auto& id : ids) {
        if (id == Parameters::kFMStatus) {
            bool fm_status = mAudExt.mFmExtension->audio_extn_fm_get_status();
            VendorParameter param;
            param.id = id;
            VString parcel;
            parcel.value = fm_status ? "true" : "false";
            setParameter(parcel, param);
            results.push_back(param);
        } else if (id == Parameters::kCanOpenProxy) {
            VendorParameter param;
            param.id = id;
            VString parcel;
            parcel.value = "1";
            setParameter(parcel, param);
            results.push_back(param);
        }
    }
    return results;
}

std::vector<VendorParameter> ModulePrimary::onGetGenericParams(
        const std::vector<std::string>& ids) {
    std::vector<VendorParameter> results{};
    for (const auto& id : ids) {
        if (id == Parameters::kOffloadPlaySpeedSupported) {
            LOG(DEBUG) << __func__ << " " << id << " supported " << mOffloadSpeedSupported;
            std::string value = (mOffloadSpeedSupported ? "true" : "false");
            auto param = makeVendorParameter(id, value);
            results.push_back(param);
        }
    }
    return results;
}

std::vector<VendorParameter> ModulePrimary::onGetBluetoothParams(
        const std::vector<std::string>& ids) {
    if (!mBluetoothA2dp) {
        LOG(ERROR) << __func__ << ": Bluetooth not created";
        return {};
    }
    std::vector<VendorParameter> results{};
    for (const auto& id : ids) {
        if (id == Parameters::kA2dpSuspended) {
            VendorParameter param;
            bool a2dpEnabled = false;
            param.id = id;
            VString parcel;
            mBluetoothA2dp->isEnabled(&a2dpEnabled);
            //if a2dp enabled is true then suspend is 0, else suspend is 1
            parcel.value = a2dpEnabled ? "0" : "1";
            setParameter(parcel, param);
            results.push_back(param);
        }
    }
    return results;
}

std::vector<VendorParameter> ModulePrimary::onGetHDRParameters(
        const std::vector<std::string>& ids) {
    std::vector<VendorParameter> result;
    for (const auto& id : ids) {
        std::string value{};
        if (id == Parameters::kHdrRecord) {
            value = makeParamValue(mPlatform.isHDREnabled());
            result.push_back(makeVendorParameter(id, value));
        } else if (id == Parameters::kHdrSamplingRate) {
            value = std::to_string(mPlatform.getHDRSampleRate());
            result.push_back(makeVendorParameter(id, value));
        } else if (id == Parameters::kHdrChannelCount) {
            value = std::to_string(mPlatform.getHDRChannelCount());
            result.push_back(makeVendorParameter(id, value));
        } else if (id == Parameters::kWnr) {
            value = makeParamValue(mPlatform.isWNREnabled());
            result.push_back(makeVendorParameter(id, value));
        } else if (id == Parameters::kAns) {
            value = makeParamValue(mPlatform.isANREnabled());
            result.push_back(makeVendorParameter(id, value));
        } else if (id == Parameters::kOrientation) {
            value = mPlatform.getOrientation();
            result.push_back(makeVendorParameter(id, value));
        } else if (id == Parameters::kInverted) {
            value = makeParamValue(mPlatform.isInverted());
            result.push_back(makeVendorParameter(id, value));
        } else if (id == Parameters::kFacing) {
            value = mPlatform.getFacing();
            result.push_back(makeVendorParameter(id, value));
        }
    }
    LOG(VERBOSE) << __func__ << ": processed";
    return result;
}

std::vector<VendorParameter> ModulePrimary::onGetTelephonyParameters(
        const std::vector<std::string>& ids) {
    if (!mTelephony) {
        LOG(ERROR) << __func__ << ": Telephony not created";
        return {};
    }
    std::vector<VendorParameter> results{};
    for (const auto& id : ids) {
        if (id == Parameters::kVoiceIsCRsSupported) {
            VendorParameter param;
            param.id = id;
            VString parcel;
            parcel.value = mTelephony->isCrsCallSupported() ? "1" : "0";
            setParameter(parcel, param);
            results.push_back(param);
        }
    }
    return results;
}

std::vector<VendorParameter> ModulePrimary::onGetWFDParameters(
        const std::vector<std::string>& ids) {
    std::vector<VendorParameter> results{};
    for (const auto& id : ids) {
        if (id == Parameters::kCanOpenProxy) {
            VendorParameter param;
            param.id = id;
            VString parcel;
            parcel.value = "1"; // This "1" indicates WFD client can try AHAL Capture.
            setParameter(parcel, param);
            results.push_back(param);
        } else if (id == Parameters::kWfdProxyRecordActive) {
            VendorParameter param;
            param.id = id;
            VString parcel;
            parcel.value = mPlatform.IsProxyRecordActive();
            setParameter(parcel, param);
            results.push_back(param);
        } else if (id == Parameters::kWfdIPAsProxyDevConnected) {
            VendorParameter param;
            param.id = id;
            VString parcel;
            parcel.value = mPlatform.isIPAsProxyDeviceConnected();
            setParameter(parcel, param);
            results.push_back(param);
        } else {
            LOG(ERROR) << __func__ << ": unknown parameter in WFD feature. id:" << id;
        }
    }
    return results;
}

std::vector<VendorParameter> ModulePrimary::onGetFTMParameters(
        const std::vector<std::string>& ids) {
    std::vector<VendorParameter> results{};
    for (const auto& id : ids) {
        VendorParameter param;
        VString parcel;
        if (id == Parameters::kFTMParam) {
            param.id = id;
            const auto& ftmResult = mPlatform.getFTMResult();
            if (ftmResult) {
                parcel.value = ftmResult.value();
            } else {
                parcel.value = "";
            }
            setParameter(parcel, param);
            results.push_back(param);
        } else if (id == Parameters::kFTMSPKRParam) {
            param.id = id;
            const auto& calResult = mPlatform.getSpeakerCalibrationResult();
            if (calResult) {
                parcel.value = calResult.value();
            } else {
                parcel.value = "false";
            }
            setParameter(parcel, param);
            results.push_back(param);
        } else {
            LOG(ERROR) << __func__ << ": unknown parameter in FTM feature. id:" << id;
        }
    }
    return results;
}


// static
ModulePrimary::GetParameterToFeatureMap ModulePrimary::fillGetParameterToFeatureMap() {
    GetParameterToFeatureMap map{{Parameters::kHdrRecord, Feature::HDR},
                                 {Parameters::kWnr, Feature::HDR},
                                 {Parameters::kAns, Feature::HDR},
                                 {Parameters::kOrientation, Feature::HDR},
                                 {Parameters::kInverted, Feature::HDR},
                                 {Parameters::kHdrChannelCount, Feature::HDR},
                                 {Parameters::kHdrSamplingRate, Feature::HDR},
                                 {Parameters::kFacing, Feature::HDR},
                                 {Parameters::kVoiceIsCRsSupported, Feature::TELEPHONY},
                                 {Parameters::kA2dpSuspended, Feature::BLUETOOTH},
                                 {Parameters::kCanOpenProxy, Feature::WFD},
                                 {Parameters::kWfdProxyRecordActive, Feature::WFD},
                                 {Parameters::kWfdIPAsProxyDevConnected, Feature::WFD},
                                 {Parameters::kFTMParam, Feature::FTM},
                                 {Parameters::kFTMSPKRParam, Feature::FTM},
                                 {Parameters::kFMStatus, Feature::AUDIOEXTENSION},
#ifdef ENABLE_QCOM_AMPERE_AUDIO
                                 {AudioControlVendorParameterExt::MASTER_MUTE_REQUEST, Feature::AUDIOCONTROL},
                                 {AudioControlVendorParameterExt::REQUEST_AUDIO_FOCUS, Feature::AUDIOCONTROL},
                                 {AudioControlVendorParameterExt::ABANDON_AUDIO_FOCUS, Feature::AUDIOCONTROL},
                                 {AudioControlVendorParameterExt::BALANCE, Feature::AUDIOCONTROL},
                                 {AudioControlVendorParameterExt::FADER, Feature::AUDIOCONTROL},
                                 {RadioVendorParameterExt::AUDIO_SOURCE, Feature::AUDIOSOURCE},
                                 {CarPlayVendorParameterExt::CARPLAY_TRANSPORT, Feature::CARPLAY},
                                 {CarPlayVendorParameterExt::CARPLAY_TYPE, Feature::CARPLAY},
                                 {CarPlayVendorParameterExt::CARPLAY_SAMPLERATE, Feature::CARPLAY},
                                 {CarPlayVendorParameterExt::CARPLAY_VOCODER_SAMPLERATE, Feature::CARPLAY},
                                 {CarPlayVendorParameterExt::CARPLAY_STATUS, Feature::CARPLAY},
                                 {CarPlayVendorParameterExt::CARPLAY_DUCK, Feature::CARPLAY},
                                 {AudioControlVendorParameterExt::BALANCE, Feature::AUDIOCONTROL},
                                 {AudioControlVendorParameterExt::FADER, Feature::AUDIOCONTROL},
                                 {AudioControlVendorParameterExt::FADER_AVAILABILITY, Feature::AUDIOCONTROL},
#endif
    };
    return map;
}

// static
ModulePrimary::FeatureToGetHandlerMap ModulePrimary::fillFeatureToGetHandlerMap() {
    FeatureToGetHandlerMap map{{Feature::HDR, &ModulePrimary::onGetHDRParameters},
                               {Feature::TELEPHONY, &ModulePrimary::onGetTelephonyParameters},
                               {Feature::BLUETOOTH, &ModulePrimary::onGetBluetoothParams},
                               {Feature::WFD, &ModulePrimary::onGetWFDParameters},
                               {Feature::FTM, &ModulePrimary::onGetFTMParameters},
                               {Feature::AUDIOEXTENSION, &ModulePrimary::onGetAudioExtnParams},
                               {Feature::GENERIC, &ModulePrimary::onGetGenericParams},
#ifdef ENABLE_QCOM_AMPERE_AUDIO
                               {Feature::AUDIOCONTROL, &ModulePrimary::onGetAudioControlParams},
                               {Feature::CARPLAY, &ModulePrimary::onGetCarplayParams},
#endif
    };
    return map;
}

// end of module parameters handling

} // namespace qti::audio::core

