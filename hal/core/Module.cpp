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
 * Changes from Qualcomm Technologies, Inc. are provided under the following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL_Module_QTI"

#include <algorithm>
#include <set>
#include <vector>

#include <aidl/android/media/audio/common/AudioInputFlags.h>
#include <aidl/android/media/audio/common/AudioOutputFlags.h>
#include <aidl/qti/audio/core/VString.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/strings.h>
#include <android/binder_ibinder_platform.h>
#include <cutils/str_parms.h>
#include <error/expected_utils.h>
#include <mediautils/MemoryLeakTrackUtil.h>
#include <memunreachable/memunreachable.h>
#include <qti-audio-core/Bluetooth.h>
#include <qti-audio-core/Module.h>
#include <qti-audio-core/Parameters.h>
#include <qti-audio-core/PlatformUtils.h>
#include <qti-audio-core/StreamInPrimary.h>
#include <qti-audio-core/StreamMmapBase.h>
#include <qti-audio-core/StreamOutPrimary.h>
#include <qti-audio-core/Telephony.h>
#include <qti-audio-core/Utils.h>
#include <fstream>
#include <sstream>
#include <memory>

using aidl::android::hardware::audio::common::SinkMetadata;
using aidl::android::hardware::audio::common::SourceMetadata;
using aidl::android::hardware::audio::core::sounddose::ISoundDose;
using aidl::android::media::audio::common::AudioChannelLayout;
using aidl::android::media::audio::common::AudioDevice;
using aidl::android::media::audio::common::AudioFormatDescription;
using aidl::android::media::audio::common::AudioFormatType;
using aidl::android::media::audio::common::AudioInputFlags;
using aidl::android::media::audio::common::AudioIoFlags;
using aidl::android::media::audio::common::AudioMMapPolicy;
using aidl::android::media::audio::common::AudioMMapPolicyInfo;
using aidl::android::media::audio::common::AudioMMapPolicyType;
using aidl::android::media::audio::common::AudioMode;
using aidl::android::media::audio::common::AudioOffloadInfo;
using aidl::android::media::audio::common::AudioOutputFlags;
using aidl::android::media::audio::common::AudioPort;
using aidl::android::media::audio::common::AudioPortConfig;
using aidl::android::media::audio::common::AudioPortExt;
using aidl::android::media::audio::common::AudioProfile;
using aidl::android::media::audio::common::Boolean;

#if AUDIO_CORE_VERSION >= 4
using aidl::android::media::audio::common::FlushFromFrameSupport;
#endif

using aidl::android::media::audio::common::Int;
using aidl::android::media::audio::common::MicrophoneInfo;
using aidl::android::media::audio::common::PcmType;

using ::aidl::android::hardware::audio::common::SinkMetadata;
using ::aidl::android::hardware::audio::common::SourceMetadata;

using ::aidl::android::hardware::audio::core::AudioPatch;
using ::aidl::android::hardware::audio::core::AudioRoute;
using ::aidl::android::hardware::audio::core::IModule;
using ::aidl::android::hardware::audio::core::IBluetooth;
using ::aidl::android::hardware::audio::core::IBluetoothA2dp;
using ::aidl::android::hardware::audio::core::IBluetoothLe;
using ::aidl::android::hardware::audio::core::IStreamIn;
using ::aidl::android::hardware::audio::core::IStreamOut;
using ::aidl::android::hardware::audio::core::ITelephony;
using aidl::android::hardware::audio::core::MmapBufferDescriptor;
using ::aidl::android::hardware::audio::core::StreamDescriptor;
using ::aidl::android::hardware::audio::core::VendorParameter;
using ::aidl::android::hardware::audio::core::sounddose::ISoundDose;
using ::aidl::qti::audio::core::VString;

using ::android::base::EqualsIgnoreCase;

namespace qti::audio::core {

namespace {

bool generateDefaultPortConfig(const AudioPort& port, AudioPortConfig* config) {
    *config = {};
    config->portId = port.id;
    if (port.profiles.empty()) {
        LOG(ERROR) << __func__ << ": port " << port.id << " has no profiles";
        return false;
    }
    const auto& profile = port.profiles.begin();
    config->format = profile->format;
    if (profile->channelMasks.empty()) {
        LOG(ERROR) << __func__ << ": the first profile in port " << port.id
                   << " has no channel masks";
        return false;
    }
    config->channelMask = *profile->channelMasks.begin();
    if (profile->sampleRates.empty()) {
        LOG(ERROR) << __func__ << ": the first profile in port " << port.id
                   << " has no sample rates";
        return false;
    }
    Int sampleRate;
    sampleRate.value = *profile->sampleRates.begin();
    config->sampleRate = sampleRate;
    config->flags = port.flags;
    config->ext = port.ext;
    return true;
}

bool findAudioProfile(const AudioPort& port, const AudioFormatDescription& format,
                      AudioProfile* profile) {
    if (auto profilesIt =
                find_if(port.profiles.begin(), port.profiles.end(),
                        [&format](const auto& profile) { return profile.format == format; });
        profilesIt != port.profiles.end()) {
        *profile = *profilesIt;
        return true;
    }
    return false;
}

std::vector<AudioProfile> getStandard16And24BitPcmAudioProfiles() {
    auto createStdPcmAudioProfile = [](const PcmType& pcmType) {
        return AudioProfile{
                .format = AudioFormatDescription{.type = AudioFormatType::PCM, .pcm = pcmType},
                .channelMasks = {AudioChannelLayout::make<AudioChannelLayout::layoutMask>(
                                         AudioChannelLayout::LAYOUT_MONO),
                                 AudioChannelLayout::make<AudioChannelLayout::layoutMask>(
                                         AudioChannelLayout::LAYOUT_STEREO)},
                .sampleRates = {8000, 11025, 16000, 32000, 44100, 48000}};
    };
    return {
            createStdPcmAudioProfile(PcmType::INT_16_BIT),
            createStdPcmAudioProfile(PcmType::INT_24_BIT),
    };
}

}  // namespace

Module::Module() {
    static_assert(IModule::version == 3 || IModule::version == 4,
                  "only 3 and 4 versions are supported");
    populateConnectedProfiles();
    mPlatform.registerPlatformGlobalCallBack(static_cast<PlatformGlobalCallback*>(this));
    mOffloadSpeedSupported = mPlatform.platformSupportsOffloadSpeed();
}

// #################### start of overriding APIs from IModule ####################

ndk::ScopedAStatus Module::setModuleDebug(
        const ::aidl::android::hardware::audio::core::ModuleDebug& in_debug) {
    LOG(DEBUG) << __func__ << ": " << ": old flags:" << mDebug.toString()
               << ", new flags: " << in_debug.toString();
    if (mDebug.simulateDeviceConnections != in_debug.simulateDeviceConnections &&
        !mConnectedDevicePorts.empty()) {
        LOG(ERROR) << __func__ << ": "
                   << ": attempting to change device connections simulation while having external "
                   << "devices connected";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    if (in_debug.streamTransientStateDelayMs < 0) {
        LOG(ERROR) << __func__ << ": " << ": streamTransientStateDelayMs is negative: "
                   << in_debug.streamTransientStateDelayMs;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    mDebug = in_debug;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::getTelephony(std::shared_ptr<ITelephony>* _aidl_return) {
    if (!mTelephony) {
        mTelephony = ndk::SharedRefBase::make<Telephony>();
        mPlatform.setTelephony(mTelephony.getInstance());
    }
    *_aidl_return = mTelephony.getInstance();
    LOG(DEBUG) << __func__
               << ": returning instance of ITelephony: " << _aidl_return->get()->asBinder().get();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::getBluetooth(std::shared_ptr<IBluetooth>* _aidl_return) {
    if (!mBluetooth) {
        mBluetooth = ndk::SharedRefBase::make<::qti::audio::core::Bluetooth>();
    }
    *_aidl_return = mBluetooth.getInstance();
    LOG(DEBUG) << __func__
               << ": returning instance of IBluetooth: " << _aidl_return->get()->asBinder().get();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::getBluetoothA2dp(std::shared_ptr<IBluetoothA2dp>* _aidl_return) {
    if (!mBluetoothA2dp) {
        mBluetoothA2dp = ndk::SharedRefBase::make<::qti::audio::core::BluetoothA2dp>();
    }
    *_aidl_return = mBluetoothA2dp.getInstance();
    LOG(DEBUG) << __func__ << ": returning instance of IBluetoothA2dp: "
               << _aidl_return->get()->asBinder().get();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::getBluetoothLe(std::shared_ptr<IBluetoothLe>* _aidl_return) {
    if (!mBluetoothLe) {
        mBluetoothLe = ndk::SharedRefBase::make<::qti::audio::core::BluetoothLe>();
    }
    *_aidl_return = mBluetoothLe.getInstance();
    LOG(DEBUG) << __func__
               << ": returning instance of IBluetoothLe: " << _aidl_return->get()->asBinder().get();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::prepareToDisconnectExternalDevice(int32_t in_portId) {
    auto& ports = getConfig().ports;
    auto portIt = findById<AudioPort>(ports, in_portId);
    if (portIt == ports.end()) {
        LOG(ERROR) << __func__ << ": port id " << in_portId << " not found";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (portIt->ext.getTag() != AudioPortExt::Tag::device) {
        LOG(ERROR) << __func__ << ": port id " << in_portId << " is not a device port";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    auto connectedPortsIt = mConnectedDevicePorts.find(in_portId);
    if (connectedPortsIt == mConnectedDevicePorts.end()) {
        LOG(ERROR) << __func__ << ": port id " << in_portId << " is not a connected device port";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    onPrepareToDisconnectExternalDevice(*portIt);

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::connectExternalDevice(const AudioPort& in_templateIdAndAdditionalData,
                                                 AudioPort* _aidl_return) {
    LOG(DEBUG) << __func__
               << ": requested template port: " << in_templateIdAndAdditionalData.toString();
    const int32_t templateId = in_templateIdAndAdditionalData.id;
    auto& ports = getConfig().ports;
    AudioPort connectedPort;
    {  // Scope the template port so that we don't accidentally modify it.
        auto templateIt = findById<AudioPort>(ports, templateId);
        if (templateIt == ports.end()) {
            LOG(ERROR) << __func__ << ": port id " << templateId << " not found";
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
        if (templateIt->ext.getTag() != AudioPortExt::Tag::device) {
            LOG(ERROR) << __func__ << ": port id " << templateId << " is not a device port";
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
        if (!templateIt->profiles.empty()) {
            LOG(ERROR) << __func__ << ": port id " << templateId
                       << " does not have dynamic profiles";
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
        auto& templateDevicePort = templateIt->ext.get<AudioPortExt::Tag::device>();
        if (templateDevicePort.device.type.connection.empty()) {
            LOG(ERROR) << __func__ << ": port id " << templateId << " is permanently attached";
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
        // Postpone id allocation until we ensure that there are no client errors.
        connectedPort = *templateIt;
        connectedPort.extraAudioDescriptors = in_templateIdAndAdditionalData.extraAudioDescriptors;
        const auto& inputDevicePort =
                in_templateIdAndAdditionalData.ext.get<AudioPortExt::Tag::device>();
        auto& connectedDevicePort = connectedPort.ext.get<AudioPortExt::Tag::device>();
        connectedDevicePort.device.address = inputDevicePort.device.address;
        // Check if there is already a connected port with for the same external device.
        for (auto connectedPortPair : mConnectedDevicePorts) {
            auto connectedPortIt = findById<AudioPort>(ports, connectedPortPair.first);
            if (connectedPortIt->ext.get<AudioPortExt::Tag::device>().device ==
                connectedDevicePort.device) {
                LOG(ERROR) << __func__ << ": device " << connectedDevicePort.device.toString()
                           << " is already connected at the device port id "
                           << connectedPortPair.first;
                return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
            }
        }
    }

    RETURN_STATUS_IF_ERROR(populateConnectedDevicePort(&connectedPort, templateId));
    if (!mDebug.simulateDeviceConnections) {
        if (auto ret = onExternalDeviceConnectionChanged(connectedPort, true /*connected*/); ret) {
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
        }

        const auto& dynamicProfiles = getDynamicProfiles(in_templateIdAndAdditionalData);
        if (dynamicProfiles.size() != 0) {
            connectedPort.profiles = dynamicProfiles;
            LOG(VERBOSE) << __func__ << ": over writing with dynamic profiles "
                         << connectedPort.profiles;
        }
        // At this point it is safe to assume this audio port if of type audio device
        if (mTelephony) {
            const auto& extDevice = connectedPort.ext.get<AudioPortExt::Tag::device>().device;
            mTelephony->onExternalDeviceConnectionChanged(extDevice, true);
        }
    } else {
        auto& connectedProfiles = getConfig().connectedProfiles;
        if (auto connectedProfilesIt = connectedProfiles.find(templateId);
            connectedProfilesIt != connectedProfiles.end()) {
            connectedPort.profiles = connectedProfilesIt->second;
        }
    }
    auto tryRevertingConnection = [&]() {
        onExternalDeviceConnectionChanged(connectedPort, false /*connected*/);
        if (mTelephony) {
            const auto& extDevice = connectedPort.ext.get<AudioPortExt::Tag::device>().device;
            mTelephony->onExternalDeviceConnectionChanged(extDevice, false);
        }
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    };

    if (connectedPort.profiles.empty()) {
        LOG(ERROR) << "Profiles of a connected port still empty after connecting external device "
                   << connectedPort.toString();
        return tryRevertingConnection();
    }
    for (auto profile : connectedPort.profiles) {
        if (profile.channelMasks.empty()) {
            LOG(ERROR) << __func__ << ": the profile " << profile.name << " has no channel masks";
            return tryRevertingConnection();
        }
        if (profile.sampleRates.empty()) {
            LOG(ERROR) << __func__ << ": the profile " << profile.name << " has no sample rates";
            return tryRevertingConnection();
        }
    }

    connectedPort.id = getConfig().nextPortId++;
    auto [connectedPortsIt, _] =
            mConnectedDevicePorts.insert(std::pair(connectedPort.id, std::set<int32_t>()));
    ports.push_back(connectedPort);

    std::vector<int32_t> routablePortIds;
    std::vector<AudioRoute> newRoutes;
    auto& routes = getConfig().routes;
    for (auto& r : routes) {
        if (r.sinkPortId == templateId) {
            AudioRoute newRoute;
            newRoute.sourcePortIds = r.sourcePortIds;
            newRoute.sinkPortId = connectedPort.id;
            newRoute.isExclusive = r.isExclusive;
            newRoutes.push_back(std::move(newRoute));
            routablePortIds.insert(routablePortIds.end(), r.sourcePortIds.begin(),
                                   r.sourcePortIds.end());
        } else {
            auto& srcs = r.sourcePortIds;
            if (std::find(srcs.begin(), srcs.end(), templateId) != srcs.end()) {
                srcs.push_back(connectedPort.id);
                routablePortIds.push_back(r.sinkPortId);
            }
        }
    }
    routes.insert(routes.end(), newRoutes.begin(), newRoutes.end());

    // Note: this is a simplistic approach assuming that a mix port can only be populated
    // from a single device port. Implementing support for stuffing dynamic profiles with a superset
    // of all profiles from all routable dynamic device ports would be more involved.
    for (const auto mixPortId : routablePortIds) {
        auto portsIt = findById<AudioPort>(ports, mixPortId);
        if (portsIt != ports.end()) {
            if (portsIt->profiles.empty()) {
                portsIt->profiles = connectedPort.profiles;
                connectedPortsIt->second.insert(portsIt->id);
            } else {
                // Check if profiles are non empty because they were populated
                // by a previous connection. Otherwise, it means that they are
                // not empty because the mix port has static profiles.
                for (const auto cp : mConnectedDevicePorts) {
                    if (cp.second.count(portsIt->id) > 0) {
                        connectedPortsIt->second.insert(portsIt->id);
                        break;
                    }
                }
            }
        }
    }
    *_aidl_return = std::move(connectedPort);
    LOG(DEBUG) << __func__ << ": for template port ID: " << templateId
               << " created new external device port: " << _aidl_return->toString();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::disconnectExternalDevice(int32_t in_portId) {
    auto& ports = getConfig().ports;
    auto portIt = findById<AudioPort>(ports, in_portId);
    if (portIt == ports.end()) {
        LOG(ERROR) << __func__ << ": port id " << in_portId << " not found";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (portIt->ext.getTag() != AudioPortExt::Tag::device) {
        LOG(ERROR) << __func__ << ": port id " << in_portId << " is not a device port";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    auto connectedPortsIt = mConnectedDevicePorts.find(in_portId);
    if (connectedPortsIt == mConnectedDevicePorts.end()) {
        LOG(ERROR) << __func__ << ": port id " << in_portId << " is not a connected device port";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    auto& configs = getConfig().portConfigs;
    auto& initials = getConfig().initialConfigs;
    auto configIt = std::find_if(configs.begin(), configs.end(), [&](const auto& config) {
        if (config.portId == in_portId) {
            // Check if the configuration was provided by the client.
            const auto& initialIt = findById<AudioPortConfig>(initials, config.id);
            return initialIt == initials.end() || config != *initialIt;
        }
        return false;
    });
    if (configIt != configs.end()) {
        LOG(ERROR) << __func__ << ": port id " << in_portId << " has a non-default config with id "
                   << configIt->id;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    // upon this point let platform know about disconnection.
    if (int ret = onExternalDeviceConnectionChanged(*portIt, false /*connected*/); ret) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    if (mTelephony) {
        const auto& extDevice = portIt->ext.get<AudioPortExt::Tag::device>().device;
        mTelephony->onExternalDeviceConnectionChanged(extDevice, false);
    }

    LOG(DEBUG) << __func__ << ": removed device port " << portIt->toString();
    ports.erase(portIt);

    auto& routes = getConfig().routes;
    for (auto routesIt = routes.begin(); routesIt != routes.end();) {
        if (routesIt->sinkPortId == in_portId) {
            routesIt = routes.erase(routesIt);
        } else {
            // Note: the list of sourcePortIds can't become empty because there must
            // be the id of the template port in the route.
            erase_if(routesIt->sourcePortIds, [in_portId](auto src) { return src == in_portId; });
            ++routesIt;
        }
    }

    // Clear profiles for mix ports that are not connected to any other ports.
    std::set<int32_t> mixPortsToClear = std::move(connectedPortsIt->second);
    mConnectedDevicePorts.erase(connectedPortsIt);
    for (const auto& connectedPort : mConnectedDevicePorts) {
        for (int32_t mixPortId : connectedPort.second) {
            mixPortsToClear.erase(mixPortId);
        }
    }
    for (int32_t mixPortId : mixPortsToClear) {
        auto mixPortIt = findById<AudioPort>(ports, mixPortId);
        if (mixPortIt != ports.end()) {
            mixPortIt->profiles = {};
        }
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::getAudioPatches(std::vector<AudioPatch>* _aidl_return) {
    *_aidl_return = getConfig().patches;
    LOG(DEBUG) << __func__ << ": returning " << _aidl_return->size() << " patches";
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::getAudioPort(int32_t in_portId, AudioPort* _aidl_return) {
    auto& ports = getConfig().ports;
    auto portIt = findById<AudioPort>(ports, in_portId);
    if (portIt != ports.end()) {
        *_aidl_return = *portIt;
        LOG(DEBUG) << __func__ << ": returning port by id " << in_portId;
        return ndk::ScopedAStatus::ok();
    }
    LOG(ERROR) << __func__ << ": port id " << in_portId << " not found";
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
}

ndk::ScopedAStatus Module::getAudioPortConfigs(std::vector<AudioPortConfig>* _aidl_return) {
    *_aidl_return = getConfig().portConfigs;
    LOG(DEBUG) << __func__ << ": returning " << _aidl_return->size() << " port configs";
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::getAudioPorts(std::vector<AudioPort>* _aidl_return) {
    *_aidl_return = getConfig().ports;
    LOG(DEBUG) << __func__ << ": returning " << _aidl_return->size() << " ports";
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::getAudioRoutes(std::vector<AudioRoute>* _aidl_return) {
    *_aidl_return = getConfig().routes;
    LOG(DEBUG) << __func__ << ": returning " << _aidl_return->size() << " routes";
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::getAudioRoutesForAudioPort(int32_t in_portId,
                                                      std::vector<AudioRoute>* _aidl_return) {
    auto& ports = getConfig().ports;
    if (auto portIt = findById<AudioPort>(ports, in_portId); portIt == ports.end()) {
        LOG(ERROR) << __func__ << ": port id " << in_portId << " not found";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    auto& routes = getConfig().routes;
    std::copy_if(routes.begin(), routes.end(), std::back_inserter(*_aidl_return),
                 [&](const auto& r) {
                     const auto& srcs = r.sourcePortIds;
                     return r.sinkPortId == in_portId ||
                            std::find(srcs.begin(), srcs.end(), in_portId) != srcs.end();
                 });
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::createStreamContext(
        int32_t in_portConfigId, int64_t in_bufferSizeFrames,
        std::shared_ptr<::aidl::android::hardware::audio::core::IStreamCallback> asyncCallback,
        std::shared_ptr<::aidl::android::hardware::audio::core::IStreamOutEventCallback>
                outEventCallback,
        std::string& streamName, StreamContext* out_context) {
    if (in_bufferSizeFrames <= 0) {
        LOG(ERROR) << __func__ << ": non-positive buffer size " << in_bufferSizeFrames;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (in_bufferSizeFrames < kMinimumStreamBufferSizeFrames) {
        LOG(ERROR) << __func__ << ": insufficient buffer size " << in_bufferSizeFrames
                   << ", must be at least " << kMinimumStreamBufferSizeFrames;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    auto& configs = getConfig().portConfigs;
    auto portConfigIt = findById<AudioPortConfig>(configs, in_portConfigId);
    if (portConfigIt->ext.getTag() != AudioPortExt::Tag::mix) {
        LOG(ERROR) << __func__ << ": could not find out mix port config "
                   << portConfigIt->toString();
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    int ioHandle = portConfigIt->ext.get<AudioPortExt::Tag::mix>().handle;
    streamName += ",io:" + std::to_string(ioHandle) + "]";
    // Since this is a private method, it is assumed that
    // validity of the portConfigId has already been checked.
    const size_t frameSize =
            getFrameSizeInBytes(portConfigIt->format.value(), portConfigIt->channelMask.value());
    if (frameSize == 0) {
        LOG(ERROR) << __func__ << ": could not calculate frame size for port config "
                   << portConfigIt->toString();
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    LOG(DEBUG) << __func__ << ": frame size " << frameSize << " bytes";
    if (frameSize > static_cast<size_t>(kMaximumStreamBufferSizeBytes / in_bufferSizeFrames)) {
        LOG(ERROR) << __func__ << ": buffer size " << in_bufferSizeFrames
                   << " frames is too large, maximum size is "
                   << kMaximumStreamBufferSizeBytes / frameSize;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    const auto& flags = portConfigIt->flags.value();
    StreamContext::DebugParameters params{mDebug.streamTransientStateDelayMs,
                                          mVendorDebug.forceTransientBurst,
                                          mVendorDebug.forceSynchronousDrain};
    const int32_t& nominalLatency = getNominalLatencyMs(*portConfigIt);

    std::weak_ptr<Telephony> wTelephony;
    if (mTelephony) {
        wTelephony = mTelephony.getInstance();
    }

    StreamContext temp(
            std::make_unique<StreamContext::CommandMQ>(1, true /*configureEventFlagWord*/),
            std::make_unique<StreamContext::ReplyMQ>(1, true /*configureEventFlagWord*/),
            portConfigIt->format.value(), portConfigIt->channelMask.value(),
            portConfigIt->sampleRate.value().value,
            std::make_unique<StreamContext::DataMQ>(frameSize * in_bufferSizeFrames), asyncCallback,
            outEventCallback, *portConfigIt, params, nominalLatency, wTelephony, streamName);
    if (temp.isValid()) {
        *out_context = std::move(temp);
    } else {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::openInputStream(const OpenInputStreamArguments& in_args,
                                           OpenInputStreamReturn* _aidl_return) {
    LOG(DEBUG) << __func__ << ": port config id " << in_args.portConfigId << ", buffer size "
               << in_args.bufferSizeFrames << " frames";
    if(!isValidSinkMetadata(in_args.sinkMetadata)) {
        LOG(ERROR) << __func__ << ": invalid metadata " << in_args.sinkMetadata.toString();
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    AudioPort* port = nullptr;
    RETURN_STATUS_IF_ERROR(findPortIdForNewStream(in_args.portConfigId, &port));
    if (port->flags.getTag() != AudioIoFlags::Tag::input) {
        LOG(ERROR) << __func__ << ": port config id " << in_args.portConfigId
                   << " does not correspond to an input mix port";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    std::string streamName = port->name + "[id:" + std::to_string(in_args.portConfigId);
    StreamContext context;
    RETURN_STATUS_IF_ERROR(createStreamContext(in_args.portConfigId, in_args.bufferSizeFrames,
                                               nullptr, nullptr, streamName, &context));
    context.fillDescriptor(&_aidl_return->desc);
    std::shared_ptr<StreamIn> stream;
    RETURN_STATUS_IF_ERROR(createInputStream(std::move(context), in_args.sinkMetadata,
                                             mConfig->microphones, &stream));
    StreamWrapper streamWrapper(stream);
    if (auto patchIt = mPatches.find(in_args.portConfigId); patchIt != mPatches.end()) {
        RETURN_STATUS_IF_ERROR(
                streamWrapper.setConnectedDevices(findConnectedDevices(in_args.portConfigId)));
    }

    if (hasInputMMapFlag(port->flags)) {
        int32_t bufferSizeFrames;
        MmapBufferDescriptor mmapDesc;
        RETURN_STATUS_IF_ERROR(streamWrapper.configureMMapStream(&mmapDesc, &bufferSizeFrames));
        _aidl_return->desc.audio.set<StreamDescriptor::AudioBuffer::Tag::mmap>(std::move(mmapDesc));
        _aidl_return->desc.bufferSizeFrames = bufferSizeFrames;
    }

    auto streamBinder = streamWrapper.getBinder();
    AIBinder_setMinSchedulerPolicy(streamBinder.get(), SCHED_NORMAL, ANDROID_PRIORITY_AUDIO);
    AIBinder_setInheritRt(streamBinder.get(), true);
    mStreams.insert(port->id, in_args.portConfigId, std::move(streamWrapper));
    _aidl_return->stream = std::move(stream);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::openOutputStream(const OpenOutputStreamArguments& in_args,
                                            OpenOutputStreamReturn* _aidl_return) {
    LOG(DEBUG) << __func__ << ": port config id " << in_args.portConfigId << ", has offload info? "
               << (in_args.offloadInfo.has_value()) << ", buffer size " << in_args.bufferSizeFrames
               << " frames";
    if(!isValidSourceMetadata(in_args.sourceMetadata)) {
        LOG(ERROR) << __func__ << ": invalid metadata " << in_args.sourceMetadata.toString();
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    AudioPort* port = nullptr;
    RETURN_STATUS_IF_ERROR(findPortIdForNewStream(in_args.portConfigId, &port));
    if (port->flags.getTag() != AudioIoFlags::Tag::output) {
        LOG(ERROR) << __func__ << ": port config id " << in_args.portConfigId
                   << " does not correspond to an output mix port";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    const bool isOffload = isBitPositionFlagSet(port->flags.get<AudioIoFlags::Tag::output>(),
                                                AudioOutputFlags::COMPRESS_OFFLOAD);
    if (isOffload && !in_args.offloadInfo.has_value()) {
        LOG(ERROR) << __func__ << ": port id " << port->id
                   << " has COMPRESS_OFFLOAD flag set, requires offload info";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    const bool isNonBlocking = isBitPositionFlagSet(port->flags.get<AudioIoFlags::Tag::output>(),
                                                    AudioOutputFlags::NON_BLOCKING);
    if (isNonBlocking && in_args.callback == nullptr) {
        LOG(ERROR) << __func__ << ": port id " << port->id
                   << " has NON_BLOCKING flag set, requires async callback";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    std::string streamName = port->name + "[id:" + std::to_string(in_args.portConfigId);
    StreamContext context;
    RETURN_STATUS_IF_ERROR(createStreamContext(in_args.portConfigId, in_args.bufferSizeFrames,
                                               isNonBlocking ? in_args.callback : nullptr,
                                               in_args.eventCallback, streamName, &context));
    context.fillDescriptor(&_aidl_return->desc);
    std::shared_ptr<StreamOut> stream;
    RETURN_STATUS_IF_ERROR(createOutputStream(std::move(context), in_args.sourceMetadata,
                                              in_args.offloadInfo, &stream));
    StreamWrapper streamWrapper(stream);
    if (auto patchIt = mPatches.find(in_args.portConfigId); patchIt != mPatches.end()) {
        RETURN_STATUS_IF_ERROR(
                streamWrapper.setConnectedDevices(findConnectedDevices(in_args.portConfigId)));
    }

    if (hasOutputMMapFlag(port->flags)) {
        int32_t bufferSizeFrames;
        MmapBufferDescriptor mmapDesc;
        RETURN_STATUS_IF_ERROR(streamWrapper.configureMMapStream(&mmapDesc, &bufferSizeFrames));
        _aidl_return->desc.audio.set<StreamDescriptor::AudioBuffer::Tag::mmap>(std::move(mmapDesc));
        _aidl_return->desc.bufferSizeFrames = bufferSizeFrames;
    } else if (mTelephony && hasOutputVoipRxFlag(port->flags)) {
        // TODO remove this way of handling streams for telephony
        mTelephony.getInstance()->setVoipPlaybackStream(stream);
    }

    auto streamBinder = streamWrapper.getBinder();
    AIBinder_setMinSchedulerPolicy(streamBinder.get(), SCHED_NORMAL, ANDROID_PRIORITY_AUDIO);
    AIBinder_setInheritRt(streamBinder.get(), true);
    mStreams.insert(port->id, in_args.portConfigId, std::move(streamWrapper));
    //    Module::updateStreamOutList(stream);
    _aidl_return->stream = std::move(stream);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::getSupportedPlaybackRateFactors(
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

ndk::ScopedAStatus Module::setAudioPatch(const AudioPatch& in_requested, AudioPatch* _aidl_return) {
    LOG(INFO) << __func__ << ": requested patch " << in_requested.toString();
    if (in_requested.sourcePortConfigIds.empty()) {
        LOG(ERROR) << __func__ << ": requested patch has empty sources list";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (!all_unique<int32_t>(in_requested.sourcePortConfigIds)) {
        LOG(ERROR) << __func__ << ": requested patch has duplicate ids in the sources list";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (in_requested.sinkPortConfigIds.empty()) {
        LOG(ERROR) << __func__ << ": requested patch has empty sinks list";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (!all_unique<int32_t>(in_requested.sinkPortConfigIds)) {
        LOG(ERROR) << __func__ << ": requested patch has duplicate ids in the sinks list";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    auto& configs = getConfig().portConfigs;
    std::vector<int32_t> missingIds;
    auto sources =
            selectByIds<AudioPortConfig>(configs, in_requested.sourcePortConfigIds, &missingIds);
    if (!missingIds.empty()) {
        LOG(ERROR) << __func__ << ": following source port config ids not found: "
                   << ::android::internal::ToString(missingIds);
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    auto sinks = selectByIds<AudioPortConfig>(configs, in_requested.sinkPortConfigIds, &missingIds);
    if (!missingIds.empty()) {
        LOG(ERROR) << __func__ << ": following sink port config ids not found: "
                   << ::android::internal::ToString(missingIds);
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    // bool indicates whether a non-exclusive route is available.
    // If only an exclusive route is available, that means the patch can not be
    // established if there is any other patch which currently uses the sink port.
    std::map<int32_t, bool> allowedSinkPorts;
    auto& routes = getConfig().routes;
    for (auto src : sources) {
        for (const auto& r : routes) {
            const auto& srcs = r.sourcePortIds;
            if (std::find(srcs.begin(), srcs.end(), src->portId) != srcs.end()) {
                if (!allowedSinkPorts[r.sinkPortId]) {  // prefer non-exclusive
                    allowedSinkPorts[r.sinkPortId] = !r.isExclusive;
                }
            }
        }
    }
    for (auto sink : sinks) {
        if (allowedSinkPorts.count(sink->portId) == 0) {
            LOG(ERROR) << __func__ << ": there is no route to the sink port id " << sink->portId;
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
    }

    RETURN_STATUS_IF_ERROR(checkAudioPatchEndpointsMatch(sources, sinks));

    auto& patches = getConfig().patches;
    auto existing = patches.end();
    std::optional<decltype(mPatches)> patchesBackup;
    AudioPatch oldPatch{};
    if (in_requested.id != 0) {
        existing = findById<AudioPatch>(patches, in_requested.id);
        if (existing != patches.end()) {
            patchesBackup = mPatches;
            cleanUpPatch(existing->id);
        } else {
            LOG(ERROR) << __func__ << ": not found existing patch id " << in_requested.id;
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
    }
    // Validate the requested patch.
    for (const auto& [sinkPortId, nonExclusive] : allowedSinkPorts) {
        if (!nonExclusive && mPatches.count(sinkPortId) != 0) {
            LOG(ERROR) << __func__ << ": sink port id " << sinkPortId
                       << "is exclusive and is already used by some other patch";
            if (patchesBackup.has_value()) {
                mPatches = std::move(*patchesBackup);
            }
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
        }
    }

    *_aidl_return = in_requested;
    if (existing == patches.end()) {
        // this suggests to create a new patch.
        _aidl_return->id = getConfig().nextPatchId++;
        _aidl_return->minimumStreamBufferSizeFrames = kMinimumStreamBufferSizeFrames;
        _aidl_return->latenciesMs.clear();
        // LatencyMs for a new patch is provided with arbitary value.
        // Real LatencyMs is fetched via StreamDescriptor::Reply::latencyMs
        constexpr int32_t kLatencyMsDefault = 10;
        _aidl_return->latenciesMs.insert(_aidl_return->latenciesMs.end(),
                                         _aidl_return->sinkPortConfigIds.size(), kLatencyMsDefault);
        onNewPatchCreation(sources, sinks, *_aidl_return);
        patches.push_back(*_aidl_return);
    } else {
        if (in_requested.id == 0) {
            _aidl_return->id = existing->id;
            LOG(DEBUG) << __func__ << "patch id 0 updated to existing patch id " << existing->id;
        }
        // this suggests to update the existing patch.
        oldPatch = *existing;
        // update the existing patch to requested patch in module config.
        *existing = *_aidl_return;
    }
    patchesBackup = mPatches;
    registerPatch(*_aidl_return);
    auto status = updateStreamsConnectedState(oldPatch, *_aidl_return);

    // call this after streams devices got updated
    setAudioPatchTelephony(sources, sinks, *_aidl_return);

    if (!status.isOk()) {
        mPatches = std::move(*patchesBackup);
        if (existing == patches.end()) {
            patches.pop_back();
        } else {
            *existing = oldPatch;
        }
        return status;
    }

    if (oldPatch.id == 0) {
        LOG(INFO) << __func__ << ": "
                  << "created " << getPatchDetails(*_aidl_return) << " "
                  << _aidl_return->toString();
    } else {
        LOG(INFO) << __func__ << ": "
                  << "updated from " << getPatchDetails(oldPatch) << " " << oldPatch.toString()
                  << " to " << getPatchDetails(*_aidl_return) << " " << _aidl_return->toString();
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::setAudioPortConfig(const AudioPortConfig& in_requested,
                                              AudioPortConfig* out_suggested, bool* _aidl_return) {
    LOG(DEBUG) << __func__ << ": requested " << in_requested.toString();
    auto& configs = getConfig().portConfigs;
    auto existing = configs.end();
    if (in_requested.id != 0) {
        if (existing = findById<AudioPortConfig>(configs, in_requested.id);
            existing == configs.end()) {
            LOG(ERROR) << __func__ << ": existing port config id " << in_requested.id
                       << " not found";
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
    }

    const int portId = existing != configs.end() ? existing->portId : in_requested.portId;
    if (portId == 0) {
        LOG(ERROR) << __func__ << ": input port config does not specify portId";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    auto& ports = getConfig().ports;
    auto portIt = findById<AudioPort>(ports, portId);
    if (portIt == ports.end()) {
        LOG(ERROR) << __func__ << ": input port config points to non-existent portId " << portId;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (existing != configs.end()) {
        *out_suggested = *existing;
    } else {
        AudioPortConfig newConfig;
        if (generateDefaultPortConfig(*portIt, &newConfig)) {
            *out_suggested = newConfig;
        } else {
            LOG(ERROR) << __func__ << ": unable generate a default config for port " << portId;
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
    }
    // From this moment, 'out_suggested' is either an existing port config,
    // or a new generated config. Now attempt to update it according to the specified
    // fields of 'in_requested'.

    bool requestedIsValid = true, requestedIsFullySpecified = true;

    AudioIoFlags portFlags = portIt->flags;
    if (in_requested.flags.has_value()) {
        if (in_requested.flags.value() != portFlags) {
            LOG(WARNING) << __func__ << ": requested flags "
                         << in_requested.flags.value().toString() << " do not match port's "
                         << portId << " flags " << portFlags.toString();
            requestedIsValid = false;
        }
    } else {
        requestedIsFullySpecified = false;
    }

    AudioProfile portProfile;
    if (in_requested.format.has_value()) {
        const auto& format = in_requested.format.value();
        if (findAudioProfile(*portIt, format, &portProfile)) {
            out_suggested->format = format;
        } else {
            LOG(WARNING) << __func__ << ": requested format " << format.toString()
                         << " is not found in port's " << portId << " profiles";
            requestedIsValid = false;
        }
    } else {
        requestedIsFullySpecified = false;
    }
    if (!findAudioProfile(*portIt, out_suggested->format.value(), &portProfile)) {
        LOG(ERROR) << __func__ << ": port " << portId << " does not support format "
                   << out_suggested->format.value().toString() << " anymore";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    if (in_requested.channelMask.has_value()) {
        const auto& channelMask = in_requested.channelMask.value();
        if (find(portProfile.channelMasks.begin(), portProfile.channelMasks.end(), channelMask) !=
            portProfile.channelMasks.end()) {
            out_suggested->channelMask = channelMask;
        } else {
            LOG(WARNING) << __func__ << ": requested channel mask " << channelMask.toString()
                         << " is not supported for the format " << portProfile.format.toString()
                         << " by the port " << portId;
            requestedIsValid = false;
        }
    } else {
        requestedIsFullySpecified = false;
    }

    if (in_requested.sampleRate.has_value()) {
        const auto& sampleRate = in_requested.sampleRate.value();
        if (find(portProfile.sampleRates.begin(), portProfile.sampleRates.end(),
                 sampleRate.value) != portProfile.sampleRates.end()) {
            out_suggested->sampleRate = sampleRate;
        } else {
            LOG(WARNING) << __func__ << ": requested sample rate " << sampleRate.value
                         << " is not supported for the format " << portProfile.format.toString()
                         << " by the port " << portId;
            requestedIsValid = false;
        }
    } else {
        requestedIsFullySpecified = false;
    }

    if (in_requested.gain.has_value()) {
        // Let's pretend that gain can always be applied.
        out_suggested->gain = in_requested.gain.value();
    }

    if (in_requested.ext.getTag() != AudioPortExt::Tag::unspecified) {
        if (in_requested.ext.getTag() == out_suggested->ext.getTag()) {
            if (out_suggested->ext.getTag() == AudioPortExt::Tag::mix) {
                // 'AudioMixPortExt.handle' is set by the client, copy from in_requested
                out_suggested->ext.get<AudioPortExt::Tag::mix>().handle =
                        in_requested.ext.get<AudioPortExt::Tag::mix>().handle;
                out_suggested->ext.get<AudioPortExt::Tag::mix>().usecase =
                        in_requested.ext.get<AudioPortExt::Tag::mix>().usecase;
            }
        } else {
            LOG(WARNING) << __func__ << ": requested ext tag "
                         << toString(in_requested.ext.getTag()) << " do not match port's tag "
                         << toString(out_suggested->ext.getTag());
            requestedIsValid = false;
        }
    }

    if (existing == configs.end() && requestedIsValid && requestedIsFullySpecified) {
        out_suggested->id = getConfig().nextPortId++;
        configs.push_back(*out_suggested);
        *_aidl_return = true;
        auto portName = portNameFromPortConfigIds(out_suggested->id);
        if (hasInputHotwordFlag(in_requested.flags.value())) {
            auto& platform = Platform::getInstance();
            platform.updateHotwordPortConfig(*out_suggested);
        }
        LOG(DEBUG) << __func__ << ": created new port config for " << portName << " "
                   << out_suggested->toString();
    } else if (existing != configs.end() && requestedIsValid) {
        *existing = *out_suggested;
        *_aidl_return = true;
        auto portName = portNameFromPortConfigIds(out_suggested->id);
        LOG(DEBUG) << __func__ << ": updated port config for" << portName << " "
                   << out_suggested->toString();
    } else {
        LOG(DEBUG) << __func__ << ": not applied; existing config ? " << (existing != configs.end())
                   << "; requested is valid? " << requestedIsValid << ", fully specified? "
                   << requestedIsFullySpecified;
        *_aidl_return = false;
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::resetAudioPatch(int32_t in_patchId) {
    auto& patches = getConfig().patches;
    auto patchIt = findById<AudioPatch>(patches, in_patchId);
    if (patchIt != patches.end()) {
        resetAudioPatchTelephony(*patchIt);
        auto patchesBackup = mPatches;
        cleanUpPatch(patchIt->id);
        if (auto status = updateStreamsConnectedState(*patchIt, AudioPatch{}); !status.isOk()) {
            mPatches = std::move(patchesBackup);
            return status;
        }
        std::string patchDetails = getPatchDetails(*patchIt);
        patches.erase(patchIt);
        LOG(DEBUG) << __func__ << ": erased patch " << in_patchId << " " << patchDetails;
        return ndk::ScopedAStatus::ok();
    }
    LOG(ERROR) << __func__ << ": patch id " << in_patchId << " not found";
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
}

ndk::ScopedAStatus Module::resetAudioPortConfig(int32_t in_portConfigId) {
    auto& configs = getConfig().portConfigs;
    auto configIt = findById<AudioPortConfig>(configs, in_portConfigId);
    if (configIt != configs.end()) {
        if (mStreams.count(in_portConfigId) != 0) {
            LOG(ERROR) << __func__ << ": port config id " << in_portConfigId
                       << " has a stream opened on it";
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
        }
        auto patchIt = mPatches.find(in_portConfigId);
        if (patchIt != mPatches.end()) {
            LOG(ERROR) << __func__ << ": port config id " << in_portConfigId
                       << " is used by the patch with id " << patchIt->second;
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
        }
        auto& initials = getConfig().initialConfigs;
        auto initialIt = findById<AudioPortConfig>(initials, in_portConfigId);
        auto relatedPortName = portNameFromPortConfigIds(in_portConfigId);
        if (initialIt == initials.end()) {
            configs.erase(configIt);
            LOG(DEBUG) << __func__ << ": erased port config " << in_portConfigId << " "
                       << relatedPortName;
        } else if (*configIt != *initialIt) {
            *configIt = *initialIt;
            LOG(DEBUG) << __func__ << ": reset port config " << in_portConfigId << " "
                       << relatedPortName;
        }
        return ndk::ScopedAStatus::ok();
    }
    LOG(ERROR) << __func__ << ": port config id " << in_portConfigId << " not found";
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
}

ndk::ScopedAStatus Module::getMasterMute(bool* _aidl_return __unused) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Module::setMasterMute(bool in_mute __unused) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Module::getMasterVolume(float* _aidl_return __unused) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Module::setMasterVolume(float in_volume __unused) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Module::getMicMute(bool* _aidl_return) {
    if (!mTelephony) {
        LOG(ERROR) << __func__ << ": Telephony not created ";
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    *_aidl_return = mPlatform.getMicMuteStatus();
    LOG(VERBOSE) << __func__ << ": returning " << *_aidl_return;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::setMicMute(bool in_mute) {
    if (!mTelephony) {
        LOG(ERROR) << __func__ << ": Telephony not created ";
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    LOG(DEBUG) << __func__ << ": " << in_mute;

    mPlatform.setMicMuteStatus(in_mute);

    mTelephony->setMicMute(in_mute);

    int ret = mAudExt.mHfpExtension->audio_extn_hfp_set_mic_mute(in_mute);

    for (const auto& inputMixPortConfigId :
         getActiveInputMixPortConfigIds(getConfig().portConfigs)) {
        mStreams.setStreamMicMute(inputMixPortConfigId, in_mute);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::getMicrophones(std::vector<MicrophoneInfo>* _aidl_return) {
    *_aidl_return = mPlatform.getMicrophoneInfo();
    LOG(VERBOSE) << __func__ << ": returning " << ::android::internal::ToString(*_aidl_return);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::updateAudioMode(AudioMode in_mode) {
    if (!isValidAudioMode(in_mode)) {
        LOG(ERROR) << __func__ << ": invalid mode " << toString(in_mode);
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    // No checks for supported audio modes here, it's an informative notification.
    LOG(DEBUG) << __func__ << ": " << toString(in_mode);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::updateScreenRotation(ScreenRotation in_rotation) {
    LOG(VERBOSE) << __func__ << ": " << toString(in_rotation);
    mPlatform.updateScreenRotation(in_rotation);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::updateScreenState(bool in_isTurnedOn) {
    LOG(VERBOSE) << __func__ << ": " << in_isTurnedOn;
    mPlatform.updateScreenState(in_isTurnedOn);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::getSoundDose(
        std::shared_ptr<::aidl::android::hardware::audio::core::sounddose::ISoundDose>*
                _aidl_return) {
    if (!mSoundDose) {
        mSoundDose = ndk::SharedRefBase::make<SoundDose>();
    }
    *_aidl_return = mSoundDose.getInstance();
    LOG(DEBUG) << __func__ << ": returning instance of ISoundDose: " << _aidl_return->get();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::generateHwAvSyncId(int32_t* _aidl_return) {
    LOG(DEBUG) << __func__;
    (void)_aidl_return;
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Module::getVendorParameters(
        const std::vector<std::string>& in_ids,
        std::vector<::aidl::android::hardware::audio::core::VendorParameter>* _aidl_return) {
    LOG(DEBUG) << __func__ << ": id count: " << in_ids.size();
    for (const auto& id : in_ids) {
        if (id == VendorDebug::kForceTransientBurstName) {
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        } else if (id == VendorDebug::kForceSynchronousDrainName) {
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
    }

    auto results = processGetVendorParameters(in_ids);
    std::move(results.begin(), results.end(), std::back_inserter(*_aidl_return));

    if (_aidl_return->size() != in_ids.size()) {
        LOG(ERROR) << __func__ << ": handled parameters " << _aidl_return->size() << " requested "
                   << in_ids.size();
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::setVendorParameters(
        const std::vector<::aidl::android::hardware::audio::core::VendorParameter>& in_parameters,
        bool in_async) {
    LOG(VERBOSE) << __func__ << ": parameter count " << in_parameters.size()
                 << ", async: " << in_async;
    for (const auto& p : in_parameters) {
        if (p.id == VendorDebug::kForceTransientBurstName) {
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        } else if (p.id == VendorDebug::kForceSynchronousDrainName) {
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        } else {
            struct str_parms* parms = NULL;
            std::string kvpairs = getkvPairsForVendorParameter(in_parameters);
            if (!kvpairs.empty()) {
                parms = str_parms_create_str(kvpairs.c_str());
                mAudExt.audio_extn_set_parameters(parms);
            }
            if (parms) str_parms_destroy(parms);

            mPlatform.setVendorParameters(in_parameters, in_async);
        }
    }
    processSetVendorParameters(in_parameters);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::addDeviceEffect(
        int32_t in_portConfigId,
        const std::shared_ptr<::aidl::android::hardware::audio::effect::IEffect>& in_effect) {
    if (in_effect == nullptr) {
        LOG(DEBUG) << __func__ << ": port id " << in_portConfigId << ", null effect";
    } else {
        LOG(DEBUG) << __func__ << ": port id " << in_portConfigId << ", effect Binder "
                   << in_effect->asBinder().get();
    }
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Module::removeDeviceEffect(
        int32_t in_portConfigId,
        const std::shared_ptr<::aidl::android::hardware::audio::effect::IEffect>& in_effect) {
    if (in_effect == nullptr) {
        LOG(DEBUG) << __func__ << ": port id " << in_portConfigId << ", null effect";
    } else {
        LOG(DEBUG) << __func__ << ": port id " << in_portConfigId << ", effect Binder "
                   << in_effect->asBinder().get();
    }
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Module::getMmapPolicyInfos(AudioMMapPolicyType mmapPolicyType,
                                              std::vector<AudioMMapPolicyInfo>* _aidl_return) {
    LOG(DEBUG) << __func__ << ": mmap policy type " << toString(mmapPolicyType);
    std::set<int32_t> mmapSinks;
    std::set<int32_t> mmapSources;
    auto& ports = getConfig().ports;
    for (const auto& port : ports) {
        if (port.flags.getTag() == AudioIoFlags::Tag::input &&
            isBitPositionFlagSet(port.flags.get<AudioIoFlags::Tag::input>(),
                                 AudioInputFlags::MMAP_NOIRQ)) {
            mmapSinks.insert(port.id);
        } else if (port.flags.getTag() == AudioIoFlags::Tag::output &&
                   isBitPositionFlagSet(port.flags.get<AudioIoFlags::Tag::output>(),
                                        AudioOutputFlags::MMAP_NOIRQ)) {
            mmapSources.insert(port.id);
        }
    }
    if (mmapSources.empty() && mmapSinks.empty()) {
        AudioMMapPolicyInfo policyInfo;
        policyInfo.mmapPolicy = AudioMMapPolicy::NEVER;
        _aidl_return->push_back(policyInfo);
        return ndk::ScopedAStatus::ok();
    }
    for (const auto& route : getConfig().routes) {
        if (mmapSinks.count(route.sinkPortId) != 0) {
            // The sink is a mix port, add the sources if they are device ports.
            for (int sourcePortId : route.sourcePortIds) {
                auto sourcePortIt = findById<AudioPort>(ports, sourcePortId);
                if (sourcePortIt == ports.end()) {
                    // This must not happen
                    LOG(ERROR) << __func__ << ": port id " << sourcePortId << " cannot be found";
                    continue;
                }
                if (sourcePortIt->ext.getTag() != AudioPortExt::Tag::device) {
                    // The source is not a device port, skip
                    continue;
                }
                AudioMMapPolicyInfo policyInfo;
                policyInfo.device = sourcePortIt->ext.get<AudioPortExt::Tag::device>().device;
                // Always return AudioMMapPolicy.AUTO if the device supports mmap for
                // default implementation.
                policyInfo.mmapPolicy = AudioMMapPolicy::AUTO;
                _aidl_return->push_back(policyInfo);
            }
        } else {
            auto sinkPortIt = findById<AudioPort>(ports, route.sinkPortId);
            if (sinkPortIt == ports.end()) {
                // This must not happen
                LOG(ERROR) << __func__ << ": port id " << route.sinkPortId << " cannot be found";
                continue;
            }
            if (sinkPortIt->ext.getTag() != AudioPortExt::Tag::device) {
                // The sink is not a device port, skip
                continue;
            }
            if (count_any(mmapSources, route.sourcePortIds)) {
                AudioMMapPolicyInfo policyInfo;
                policyInfo.device = sinkPortIt->ext.get<AudioPortExt::Tag::device>().device;
                // Always return AudioMMapPolicy.AUTO if the device supports mmap for
                // default implementation.
                policyInfo.mmapPolicy = AudioMMapPolicy::AUTO;
                _aidl_return->push_back(policyInfo);
            }
        }
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::supportsVariableLatency(bool* _aidl_return) {
    LOG(DEBUG) << __func__;
    *_aidl_return = true;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::getAAudioMixerBurstCount(int32_t* _aidl_return) {
    if (!isMmapSupported()) {
        LOG(DEBUG) << __func__ << ": mmap is not supported ";
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    *_aidl_return = DEFAULT_AAUDIO_MIXER_BURST_COUNT;
    LOG(DEBUG) << __func__ << ": returning " << *_aidl_return;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::getAAudioHardwareBurstMinUsec(int32_t* _aidl_return) {
    if (!isMmapSupported()) {
        LOG(DEBUG) << __func__ << ": mmap is not supported ";
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    const std::string kAaudioHwBurst = "aaudio.hw_burst_min_usec";
    auto burstSize = ::android::base::GetUintProperty<size_t>(kAaudioHwBurst, 2000);

    *_aidl_return = burstSize;
    LOG(DEBUG) << __func__ << ": returning " << *_aidl_return;
    return ndk::ScopedAStatus::ok();
}

#if AUDIO_CORE_VERSION >= 4
ndk::ScopedAStatus Module::getFlushFromFrameSupport(const AudioPortConfig& in_config __unused,
                                                    FlushFromFrameSupport* _aidl_return __unused) {
    LOG(VERBOSE) << __func__ << ": do nothing and return unsupported operation";
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
#endif

binder_status_t Module::dump(int fd, const char** args, uint32_t numArgs) {
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

    if (numArgs > 0) {
        bool dumpUnreachable = false;
        bool dumpMemory = false;

        for (uint32_t i = 0; i < numArgs; i++) {
            std::string option = std::string(args[i]);
            if (EqualsIgnoreCase(option, "--memory")) {
                dumpUnreachable = true;
                dumpMemory = true;
            } else if (EqualsIgnoreCase(option, "-m")) {
                dumpMemory = true;
            } else if ((EqualsIgnoreCase(option, "--unreachable"))) {
                dumpUnreachable = true;
            }
        }

        if (dumpMemory) {
            dprintf(fd, "\nDumping memory:\n");
            std::string s = android::dumpMemoryAddresses(100 /* limit */);
            write(fd, s.c_str(), s.size());
        }

        if (dumpUnreachable) {
            dprintf(fd, "\nDumping unreachable memory:\n");
            std::string s =
                    android::GetUnreachableMemoryString(true /* contents */, 100 /* limit */);
            write(fd, s.c_str(), s.size());
        }
    }
    LOG(VERBOSE) << __func__ << " :success";
    return 0;
}

// #################### end of overriding APIs from IModule ####################

// #################### start of overriding APIs from PlatformGlobalCallback ########

void Module::onSoundDose(void* const eventData, const AudioDevice& device) {
    if (!mSoundDose) {
        mSoundDose = ndk::SharedRefBase::make<SoundDose>();
    }
    mSoundDose->onSoundDose(eventData, device);
}

// #################### end of overriding APIs from PlatformGlobalCallback ########

void Module::cleanUpPatch(int32_t patchId) {
    erase_all_values(mPatches, std::set<int32_t>{patchId});
}

void Module::registerPatch(const AudioPatch& patch) {
    auto& configs = getConfig().portConfigs;
    auto do_insert = [&](const std::vector<int32_t>& portConfigIds) {
        for (auto portConfigId : portConfigIds) {
            auto configIt = findById<AudioPortConfig>(configs, portConfigId);
            if (configIt != configs.end()) {
                mPatches.insert(std::pair{portConfigId, patch.id});
                if (configIt->portId != portConfigId) {
                    mPatches.insert(std::pair{configIt->portId, patch.id});
                }
            }
        };
    };
    do_insert(patch.sourcePortConfigIds);
    do_insert(patch.sinkPortConfigIds);
}

ndk::ScopedAStatus Module::updateStreamsConnectedState(const AudioPatch& oldPatch,
                                                       const AudioPatch& newPatch) {
    // Notify streams about the new set of devices they are connected to.
    auto maybeFailure = ndk::ScopedAStatus::ok();
    using Connections =
            std::map<int32_t /*mixPortConfigId*/, std::set<int32_t /*devicePortConfigId*/>>;
    Connections oldConnections, newConnections;
    auto fillConnectionsHelper = [&](Connections& connections,
                                     const std::vector<int32_t>& mixPortCfgIds,
                                     const std::vector<int32_t>& devicePortCfgIds) {
        for (int32_t mixPortCfgId : mixPortCfgIds) {
            connections[mixPortCfgId].insert(devicePortCfgIds.begin(), devicePortCfgIds.end());
        }
    };
    auto fillConnections = [&](Connections& connections, const AudioPatch& patch) {
        if (std::find_if(patch.sourcePortConfigIds.begin(), patch.sourcePortConfigIds.end(),
                         [&](int32_t portConfigId) { return mStreams.count(portConfigId) > 0; }) !=
            patch.sourcePortConfigIds.end()) {
            // Sources are mix ports.
            fillConnectionsHelper(connections, patch.sourcePortConfigIds, patch.sinkPortConfigIds);
        } else if (std::find_if(patch.sinkPortConfigIds.begin(), patch.sinkPortConfigIds.end(),
                                [&](int32_t portConfigId) {
                                    return mStreams.count(portConfigId) > 0;
                                }) != patch.sinkPortConfigIds.end()) {
            // Sources are device ports.
            fillConnectionsHelper(connections, patch.sinkPortConfigIds, patch.sourcePortConfigIds);
        }  // Otherwise, there are no streams to notify.
    };
    auto restoreOldConnections = [&](const std::set<int32_t>& mixPortIds,
                                     const bool continueWithEmptyDevices) {
        for (const auto mixPort : mixPortIds) {
            if (auto it = oldConnections.find(mixPort);
                continueWithEmptyDevices || it != oldConnections.end()) {
                AudioDevice noneDevice;
                const std::vector<AudioDevice> d =
                        it != oldConnections.end()
                                ? getDevicesFromDevicePortConfigIds(it->second)
                                : std::vector<AudioDevice>({noneDevice}) /*None Device Fail-Safe*/;
                if (auto status = mStreams.setStreamConnectedDevices(mixPort, d); status.isOk()) {
                    LOG(WARNING) << ":updateStreamsConnectedState: rollback: mix port config:"
                                 << mixPort
                                 << (d.empty() ? "; not connected"
                                               : std::string("; connected to ") +
                                                         ::android::internal::ToString(d));
                } else {
                    // can't do much about rollback failures
                    LOG(ERROR)
                            << ":updateStreamsConnectedState: rollback: failed for mix port config:"
                            << mixPort;
                }
            }
        }
    };
    fillConnections(oldConnections, oldPatch);
    fillConnections(newConnections, newPatch);

    /**
     * Illustration of oldConnections and newConnections
     *
     * oldConnections {
     * a : {A,B,C},
     * b : {D},
     * d : {H,I,J},
     * e : {N,O,P},
     * f : {Q,R},
     * g : {T,U,V},
     * }
     *
     * newConnections {
     * a : {A,B,C},
     * c : {E,F,G},
     * d : {K,L,M},
     * e : {N,P},
     * f : {Q,R,S},
     * g : {U,V,W},
     * }
     *
     * Expected routings:
     *      'a': is ignored both in disconnect step and connect step,
     *           due to same devices both in oldConnections and newConnections.
     *      'b': handled only in disconnect step with empty devices because 'b' is only present
     *           in oldConnections.
     *      'c': handled only in connect step with {E,F,G} devices because 'c' is only present
     *           in newConnections.
     *      'd': handled only in connect step with {K,L,M} devices because 'd' is also present
     *           in newConnections and it is ignored in disconnected step.
     *      'e': handled only in connect step with {N,P} devices because 'e' is also present
     *           in newConnections and it is ignored in disconnect step. please note that there
     *           is no exclusive disconnection for device {O}.
     *      'f': handled only in connect step with {Q,R,S} devices because 'f' is also present
     *           in newConnections and it is ignored in disconnect step. Even though stream is
     *           already connected with {Q,R} devices and connection happens with {Q,R,S}.
     *      'g': handled only in connect step with {U,V,W} devices because 'g' is also present
     *           in newConnections and it is ignored in disconnect step. There is no exclusive
     *           disconnection with devices {T,U,V}.
     *
     *       If, any failure, will lead to restoreOldConnections (rollback).
     *       The aim of the restoreOldConnections is to make connections back to oldConnections.
     *       Failures in restoreOldConnections aren't handled.
     */

    std::set<int32_t> idsToConnectBackOnFailure;
    // disconnection step
    for (const auto& [oldMixPortConfigId, oldDevicePortConfigIds] : oldConnections) {
        if (auto it = newConnections.find(oldMixPortConfigId); it == newConnections.end()) {
            idsToConnectBackOnFailure.insert(oldMixPortConfigId);
            /**
             * None Device Fail-Safe
             *
             * Although setting empty devices on stream is allowed momentarily.
             * But read's or write's to stream when empty devices leads to failures.
             *
             * Configure stream devices to the NONE device as a fail-safe mechanism.
             * This ensures that any attempts to read from or write to the stream when no device is
             * connected are handled gracefully.
             *
             * Note: This scenario is not expected to occur. The HAL client (i.e., framework) must
             * ensure that this situation does not arise.
             */
            AudioDevice noneDevice;
            if (auto status = mStreams.setStreamConnectedDevices(oldMixPortConfigId, {noneDevice});
                status.isOk()) {
                LOG(DEBUG) << __func__ << ": The stream on port config id " << oldMixPortConfigId
                           << " has been disconnected";
            } else {
                maybeFailure = std::move(status);
                // proceed to rollback even on one failure
                break;
            }
        }
    }

    if (!maybeFailure.isOk()) {
        restoreOldConnections(idsToConnectBackOnFailure, false /*continueWithEmptyDevices*/);
        LOG(WARNING) << __func__ << ": failed to disconnect from old patch. attempted rollback";
        return maybeFailure;
    }

    std::set<int32_t> idsToRollbackOnFailure;
    // connection step
    for (const auto& [newMixPortConfigId, newDevicePortConfigIds] : newConnections) {
        const auto connectedDevices = getDevicesFromDevicePortConfigIds(newDevicePortConfigIds);
        if (auto it = oldConnections.find(newMixPortConfigId);
            it == oldConnections.end() || it->second != newDevicePortConfigIds ||
            /* if bluetooth device, force route to the streams due to A2DP|SCO suspend on or off*/
            hasBluetoothDevice(connectedDevices)) {
            idsToRollbackOnFailure.insert(newMixPortConfigId);
            if (connectedDevices.empty()) {
                // This is important as workers use the vector size to derive the connection status.
                LOG(FATAL) << __func__ << ": No connected devices found for port config id "
                           << newMixPortConfigId;
            }
            if (auto status =
                        mStreams.setStreamConnectedDevices(newMixPortConfigId, connectedDevices);
                status.isOk()) {
                LOG(DEBUG) << __func__ << ": The stream on port config id " << newMixPortConfigId
                           << " has been connected to: "
                           << ::android::internal::ToString(connectedDevices);
            } else {
                maybeFailure = std::move(status);
                // proceed to rollback even on one failure
                break;
            }
        }
    }

    if (!maybeFailure.isOk()) {
        restoreOldConnections(idsToConnectBackOnFailure, false /*continueWithEmptyDevices*/);
        restoreOldConnections(idsToRollbackOnFailure, true /*continueWithEmptyDevices*/);
        LOG(WARNING) << __func__ << ": failed to connect for new patch. attempted rollback";
        return maybeFailure;
    }

    return ndk::ScopedAStatus::ok();
}

void Module::populateConnectedProfiles() {
    auto& config = getConfig();
    for (const AudioPort& port : config.ports) {
        if (port.ext.getTag() == AudioPortExt::device) {
            if (auto devicePort = port.ext.get<AudioPortExt::device>();
                !devicePort.device.type.connection.empty() && port.profiles.empty()) {
                if (auto connIt = config.connectedProfiles.find(port.id);
                    connIt == config.connectedProfiles.end()) {
                    config.connectedProfiles.emplace(port.id,
                                                     getStandard16And24BitPcmAudioProfiles());
                }
            }
        }
    }
}

std::unique_ptr<ModuleConfig> Module::initializeConfig() {
    std::unique_ptr<ModuleConfig> config = std::move(ModuleConfig::getPrimaryConfiguration());
    return config;
}

ModuleConfig& Module::getConfig() {
    if (!mConfig) {
        mConfig = std::move(initializeConfig());
    }
    return *mConfig;
}

std::vector<AudioDevice> Module::getDevicesFromDevicePortConfigIds(
        const std::set<int32_t>& devicePortConfigIds) {
    std::vector<AudioDevice> result;
    auto& configs = getConfig().portConfigs;
    for (const auto& id : devicePortConfigIds) {
        auto it = findById<AudioPortConfig>(configs, id);
        if (it != configs.end() && it->ext.getTag() == AudioPortExt::Tag::device) {
            result.push_back(it->ext.template get<AudioPortExt::Tag::device>().device);
        } else {
            LOG(FATAL) << __func__ << ": "
                       << ": failed to find device for id" << id;
        }
    }
    return result;
}

std::vector<AudioDevice> Module::findConnectedDevices(int32_t portConfigId) {
    return getDevicesFromDevicePortConfigIds(findConnectedPortConfigIds(portConfigId));
}

std::set<int32_t> Module::findConnectedPortConfigIds(int32_t portConfigId) {
    std::set<int32_t> result;
    auto patchIdsRange = mPatches.equal_range(portConfigId);
    auto& patches = getConfig().patches;
    for (auto it = patchIdsRange.first; it != patchIdsRange.second; ++it) {
        auto patchIt = findById<AudioPatch>(patches, it->second);
        if (patchIt == patches.end()) {
            LOG(FATAL) << __func__ << ": patch with id " << it->second << " taken from mPatches "
                       << "not found in the configuration";
        }
        if (std::find(patchIt->sourcePortConfigIds.begin(), patchIt->sourcePortConfigIds.end(),
                      portConfigId) != patchIt->sourcePortConfigIds.end()) {
            result.insert(patchIt->sinkPortConfigIds.begin(), patchIt->sinkPortConfigIds.end());
        } else {
            result.insert(patchIt->sourcePortConfigIds.begin(), patchIt->sourcePortConfigIds.end());
        }
    }
    return result;
}

ndk::ScopedAStatus Module::findPortIdForNewStream(int32_t in_portConfigId, AudioPort** port) {
    auto& configs = getConfig().portConfigs;
    auto portConfigIt = findById<AudioPortConfig>(configs, in_portConfigId);
    if (portConfigIt == configs.end()) {
        LOG(ERROR) << __func__ << ": existing port config id " << in_portConfigId << " not found";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    const int32_t portId = portConfigIt->portId;
    // In our implementation, configs of mix ports always have unique IDs.
    CHECK(portId != in_portConfigId);
    auto& ports = getConfig().ports;
    auto portIt = findById<AudioPort>(ports, portId);
    if (portIt == ports.end()) {
        LOG(ERROR) << __func__ << ": port id " << portId << " used by port config id "
                   << in_portConfigId << " not found";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (mStreams.count(in_portConfigId) != 0) {
        LOG(ERROR) << __func__ << ": port config id " << in_portConfigId
                   << " already has a stream opened on it";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    if (portIt->ext.getTag() != AudioPortExt::Tag::mix) {
        LOG(ERROR) << __func__ << ": port config id " << in_portConfigId
                   << " does not correspond to a mix port";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    const size_t maxOpenStreamCount = portIt->ext.get<AudioPortExt::Tag::mix>().maxOpenStreamCount;
    if (maxOpenStreamCount != 0 && mStreams.count(portId) >= maxOpenStreamCount) {
        LOG(ERROR) << __func__ << ": port id " << portId
                   << " has already reached maximum allowed opened stream count: "
                   << maxOpenStreamCount;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    *port = &(*portIt);
    return ndk::ScopedAStatus::ok();
}

void Module::onPrepareToDisconnectExternalDevice(
        const ::aidl::android::media::audio::common::AudioPort& audioPort __unused) {
    LOG(DEBUG) << __func__ << ": do nothing and return";
}

std::string Module::portNameFromPortConfigIds(int portConfigId) {
    auto& portConfigs = getConfig().portConfigs;
    auto portConfigIt = findById<AudioPortConfig>(portConfigs, portConfigId);
    if (portConfigIt != portConfigs.end()) {
        auto& ports = getConfig().ports;
        auto portIt = findById<AudioPort>(ports, portConfigIt->portId);
        return portIt->name;
    }

    return "";
}

std::string Module::getPatchDetails(
        const ::aidl::android::hardware::audio::core::AudioPatch& patch) {
    auto sourcePortConfigs = patch.sourcePortConfigIds;
    auto sinkPortConfigs = patch.sinkPortConfigIds;

    std::string result = "[";

    for (auto src : sourcePortConfigs) {
        result += portNameFromPortConfigIds(src);
        result += " ";
    }

    result += " -> ";

    for (auto sink : sinkPortConfigs) {
        result += portNameFromPortConfigIds(sink);
        result += " ";
    }

    result += " ]";
    return result;
}

bool Module::isMmapSupported() {
    if (mIsMmapSupported.has_value()) {
        return mIsMmapSupported.value();
    }
    std::vector<AudioMMapPolicyInfo> mmapPolicyInfos;
    if (!getMmapPolicyInfos(AudioMMapPolicyType::DEFAULT, &mmapPolicyInfos).isOk()) {
        mIsMmapSupported = false;
    } else {
        mIsMmapSupported =
                std::find_if(mmapPolicyInfos.begin(), mmapPolicyInfos.end(), [](const auto& info) {
                    return info.mmapPolicy == AudioMMapPolicy::AUTO ||
                           info.mmapPolicy == AudioMMapPolicy::ALWAYS;
                }) != mmapPolicyInfos.end();
    }
    return mIsMmapSupported.value();
}

ndk::ScopedAStatus Module::populateConnectedDevicePort(AudioPort* connectedDevicePort,
                                                       const int32_t templateDevicePortId) {
    auto& externalDeviceProfiles = getConfig().mExternalDevicePortProfiles;
    auto connectedProfilesIt = externalDeviceProfiles.find(templateDevicePortId);

    if (connectedProfilesIt != externalDeviceProfiles.end()) {
        connectedDevicePort->profiles = connectedProfilesIt->second;
    } else {
        LOG(ERROR) << __func__ << ": failed to find profiles for template device port ID: "
                   << templateDevicePortId;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    LOG(VERBOSE) << __func__ << ": template device port ID: " << templateDevicePortId
                 << " attached profiles: " << connectedDevicePort->profiles;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::checkAudioPatchEndpointsMatch(
        const std::vector<AudioPortConfig*>& sources __unused,
        const std::vector<AudioPortConfig*>& sinks __unused) {
    LOG(VERBOSE) << __func__ << ": do nothing and return ok";
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::onMasterMuteChanged(bool mute __unused) {
    LOG(VERBOSE) << __func__ << ": do nothing and return ok";
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::onMasterVolumeChanged(float volume __unused) {
    LOG(VERBOSE) << __func__ << ": do nothing and return ok";
    return ndk::ScopedAStatus::ok();
}

std::string Module::toStringInternal() {
    std::ostringstream os;
    os << "--- Module start ---" << std::endl;
    os << getConfig().toString() << std::endl;

    os << std::endl << " --- mPatches ---" << std::endl;
    std::for_each(mPatches.cbegin(), mPatches.cend(), [&](const auto& pair) {
        os << "PortConfigId/PortId:" << pair.first << " Patch Id:" << pair.second << std::endl;
    });
    os << std::endl << " --- mPatches end ---" << std::endl << std::endl;

    os << mStreams.toString();

    os << mPlatform.toString() << std::endl;
    os << "--- Module end ---" << std::endl;
    return os.str();
}

void Module::dumpInternal(const std::string& identifier) {
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

ndk::ScopedAStatus Module::createInputStream(StreamContext&& context,
                                             const SinkMetadata& sinkMetadata,
                                             const std::vector<MicrophoneInfo>& microphones,
                                             std::shared_ptr<StreamIn>* result) {
    if (context.isMmap()) {
        auto status = createStreamInstance<StreamInMmap>(result, std::move(context), sinkMetadata,
                                                  microphones);
        Module::inListMutex.lock();
        Module::updateStreamInList(*result);
        Module::inListMutex.unlock();
        return status;
    }

    createStreamInstance<StreamInPrimary>(result, std::move(context), sinkMetadata, microphones);
    Module::inListMutex.lock();
    Module::updateStreamInList(*result);
    if (mTelephony) {
        mTelephony->mStreamInPrimary = *result;
    }
    Module::inListMutex.unlock();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Module::createOutputStream(StreamContext&& context,
                                              const SourceMetadata& sourceMetadata,
                                              const std::optional<AudioOffloadInfo>& offloadInfo,
                                              std::shared_ptr<StreamOut>* result) {
    if (mPlatform.isSoundCardDown() &&
        (hasOutputDirectFlag(context.getMixPortConfig().flags.value()) ||
         hasOutputCompressOffloadFlag(context.getMixPortConfig().flags.value()))) {
        LOG(ERROR) << __func__ << ": avoid direct or compress streams as sound card is down";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    if (context.isMmap()) {
        auto status = createStreamInstance<StreamOutMmap>(result, std::move(context), sourceMetadata,
                                                   offloadInfo);
        Module::outListMutex.lock();
        Module::updateStreamOutList(*result);
        Module::outListMutex.unlock();
        return status;
    }

    createStreamInstance<StreamOutPrimary>(result, std::move(context), sourceMetadata, offloadInfo);
    Module::outListMutex.lock();
    Module::updateStreamOutList(*result);
    // save primary out stream weak ptr, as some other modules need it.
    if (mTelephony) {
        mTelephony->mStreamOutPrimary = *result;
    }

    Module::outListMutex.unlock();
    return ndk::ScopedAStatus::ok();
}

std::vector<::aidl::android::media::audio::common::AudioProfile> Module::getDynamicProfiles(
        const ::aidl::android::media::audio::common::AudioPort& audioPort) {
    if (isUsbDevice(audioPort.ext.get<AudioPortExt::Tag::device>().device)) {
        /* as of now, we do dynamic fetching for usb devices*/
        auto dynamicProfiles = mPlatform.getDynamicProfiles(audioPort);
        return dynamicProfiles;
    }
    return {};
}

void Module::onNewPatchCreation(const std::vector<AudioPortConfig*>& sources,
                                const std::vector<AudioPortConfig*>& sinks, AudioPatch& newPatch) {
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

void Module::setAudioPatchTelephony(const std::vector<AudioPortConfig*>& sources,
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

void Module::resetAudioPatchTelephony(const AudioPatch& patch) {
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

int Module::onExternalDeviceConnectionChanged(
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

    return 0;
}

int32_t Module::getNominalLatencyMs(const AudioPortConfig& mixPortConfig) {
    return mPlatform.getLatencyMs(mixPortConfig);
}

// start of module parameters handling

bool Module::processSetVendorParameters(const std::vector<VendorParameter>& parameters) {
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

    for (const auto& [key, value] : pendingActions) {
        const auto search = mFeatureToSetHandlerMap.find(key);
        if (search == mFeatureToSetHandlerMap.cend()) {
            LOG(VERBOSE) << __func__
                         << ": no handler set on Feature:" << static_cast<int>(search->first);
            continue;
        }
        auto handler = std::bind(search->second, this, value);
        handler();  // a dynamic dispatch to a SetHandler
    }
    return true;
}

void Module::onSetGenericParameters(const std::vector<VendorParameter>& params) {
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
        } else if (Parameters::kUvVoiceCueEnable == param.id) {
            uint32_t usecaseMask = 0;
            if (!parseUvVoiceCueStatusConfig(paramValue, &usecaseMask)) {
                LOG(ERROR) << __func__ << ": failed to parse "
                           << Parameters::kUvVoiceCueEnable
                           << " value: " << paramValue;
                continue;
            }
            LOG(INFO) << __func__ << ": UV config received, usecase_mask=0x"
                      << std::hex << usecaseMask << std::dec;
            mPlatform.setUvVoiceCueStatusConfig(usecaseMask);
            updateVoiceCueStatus(usecaseMask);
            mTelephony->updateVoiceCue(usecaseMask);
            if (usecaseMask & UV_FLUENCE_VOIP_BIT) {
                mPlatform.setVoiceCueOnVoipEnable(true);
            } else {
                mPlatform.setVoiceCueOnVoipEnable(false);
            }
        } else if (Parameters::kUvVoiceCueBytes == param.id) {
            updateVoiceCueBytes(std::move(paramValue));
        }
    }
}

void Module::onSetHDRParameters(const std::vector<VendorParameter>& params) {
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

void Module::onSetTelephonyParameters(const std::vector<VendorParameter>& parameters) {
    if (!mTelephony) {
        LOG(ERROR) << __func__ << ": Telephony not created";
        return;
    }

    Telephony::SetUpdates setUpdates{};
    bool isSetUpdate = false;
    bool isDeviceMuted = false;
    std::string muteDirection{""};
    bool isDeviceMuteUpdate = false;
    int deviceType = 0;

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
        } else if (Parameters::kVoiceCRSDevice == p.id) {
            deviceType = getInt64FromString(paramValue);
            mTelephony->setCrsDeviceFromParameters(deviceType);
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
            LOG(DEBUG) << __func__ << " : translation Rx mute set as" << isOn;
            if (mTelephony->isVoipActive()) {
                setVoipRxMute(isOn);
            } else {
                mTelephony->updateVoiceVolume();
            }
        } else if (Parameters::kVoiceTranslationTxMute == p.id) {
            const auto isOn = getBoolFromString(paramValue);
            mPlatform.setTranslationTxMuteState(isOn);
            LOG(DEBUG) << __func__ << " : translation Tx mute set as" << isOn;
            if (mTelephony->isVoipActive()) {
                setVoipTxMute(isOn);
            } else {
                mTelephony->setMicMute(isOn);
            }
        } else if (Parameters::kTranslationConfig == p.id) {
            mTelephony->CallTranslationManager(paramValue);
        } else if (Parameters::kVoiceNsRxConfig == p.id) {
            const bool bypass = paramValue == "true" ? true : false;
            mTelephony->updateVoiceNsRxConfigMode(bypass);
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

void Module::onSetWFDParameters(const std::vector<VendorParameter>& parameters) {
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

void Module::onSetFTMParameters(const std::vector<VendorParameter>& parameters) {
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

void Module::onSetHapticsParameters(const std::vector<VendorParameter>& parameters) {
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

void Module::setVoipTxMute(bool mute_state) {
    LOG(DEBUG) << __func__ << ": mute :" << mute_state;
    constexpr auto recordVoipFlags = static_cast<int32_t>(
            1 << static_cast<int32_t>(
                    ::aidl::android::media::audio::common::AudioInputFlags::VOIP_TX));
    for (const auto& inputMixPortConfigId :
         getActiveInputMixPortConfigIds(getConfig().portConfigs)) {
        // Find the corresponding port config
        auto portConfigIt =
                std::find_if(getConfig().portConfigs.begin(), getConfig().portConfigs.end(),
                             [&inputMixPortConfigId](const auto& config) {
                                 return config.id == inputMixPortConfigId;
                             });
        if (portConfigIt != getConfig().portConfigs.end()) {
            const auto& portConfig = *portConfigIt;

            // check VoIP TX streams
            if (portConfig.ext.getTag() ==
                        ::aidl::android::media::audio::common::AudioPortExt::Tag::mix &&
                portConfig.flags &&
                portConfig.flags.value().getTag() ==
                        ::aidl::android::media::audio::common::AudioIoFlags::Tag::input &&
                portConfig.flags.value()
                                .get<::aidl::android::media::audio::common::AudioIoFlags::Tag::
                                             input>() == recordVoipFlags &&
                portConfig.ext.get<::aidl::android::media::audio::common::AudioPortExt::Tag::mix>()
                                .usecase.getTag() == ::aidl::android::media::audio::common::
                                                             AudioPortMixExtUseCase::Tag::source &&
                portConfig.ext.get<::aidl::android::media::audio::common::AudioPortExt::Tag::mix>()
                                .usecase.get<::aidl::android::media::audio::common::
                                                     AudioPortMixExtUseCase::Tag::source>() ==
                        ::aidl::android::media::audio::common::AudioSource::VOICE_COMMUNICATION) {
                LOG(INFO) << __func__ << ": Found VoIP TX stream with ID " << inputMixPortConfigId;
                mStreams.setStreamMicMute(inputMixPortConfigId, mute_state);
            }
        }
    }
}
void Module::setVoipRxMute(bool state) {
    pal_stream_handle_t* voipRxHandle = mPlatform.getVoipRxStreamHandle();
    if (voipRxHandle != nullptr) {
        LOG(INFO) << __func__ << ":found voiprx pal handle";
        if (int32_t ret = ::pal_stream_set_mute(voipRxHandle, state); ret) {
            LOG(ERROR) << __func__ << " pal_stream_set_mute failed!!! ret:" << ret;
            return;
        }
    }
}
void Module::updateVoiceCueBytes(const std::string&& byteData) {
    int32_t ret = -1;
    size_t dataSize = 0;
    uint8_t *ptr = nullptr;
    LOG(DEBUG) << __func__ << ": Enter";
    ptr = stringToUint8Array(std::move(byteData), &dataSize);
    if(ptr == nullptr || dataSize == 0) {
        LOG(ERROR) << __func__ << ": not able to get valid data from string" << ret;
        return;
    }
    size_t byteSize = sizeof(pal_param_payload) + dataSize;
    std::unique_ptr<uint8_t[]> bytes = std::make_unique<uint8_t[]>(byteSize);
    pal_param_payload *palParamPayload = reinterpret_cast<pal_param_payload*>(bytes.get());
    palParamPayload->payload_size = dataSize;
    memcpy(palParamPayload->payload, ptr, dataSize);
    free(ptr);
    ret = ::pal_set_param(PAL_PARAM_ID_UV_VOICE_CUE_DATA_BYTE,
                          (void*)palParamPayload,
                          byteSize);
    if (ret != 0) {
        LOG(ERROR) << __func__ << ": failed to set PAL_PARAM_ID_UV_VOICE_CUE_DATA_BYTE" << ret;
    }
    LOG(DEBUG) << __func__ << ": Exit";
    return;
}
void Module::updateVoiceCueStatus(uint32_t usecaseMask) {
    int32_t ret = -1;
    uv_fluence_config_t uvConfig{};

    uvConfig.usecase_mask = usecaseMask;
    uvConfig.voice_cue_param = nullptr;
    uvConfig.param_size = 0;

    size_t byteSize = sizeof(pal_param_payload) + sizeof(uv_fluence_config_t);
    std::unique_ptr<uint8_t[]> bytes = std::make_unique<uint8_t[]>(byteSize);
    pal_param_payload *palParamPayload = reinterpret_cast<pal_param_payload*>(bytes.get());
    palParamPayload->payload_size = sizeof(uv_fluence_config_t);

    std::memcpy(palParamPayload->payload, &uvConfig, sizeof(uv_fluence_config_t));
    LOG(DEBUG) << __func__ << ": Enter, usecase_mask=0x"
               << std::hex << uvConfig.usecase_mask << std::dec;
    ret = ::pal_set_param(PAL_PARAM_ID_UV_VOICE_CUE_ENABLE,
                          palParamPayload,
                          palParamPayload->payload_size);
    if (ret) {
        LOG(ERROR) << __func__ << ": failed to set PAL_PARAM_ID_UV_VOICE_CUE_ENABLE "
                   << ret;
    }
    LOG(DEBUG) << __func__ << ": Exit";
}
uint8_t* Module::stringToUint8Array(const std::string&& str, size_t* size) {
    std::istringstream iss(str);
    std::vector<uint8_t> cache;
    std::string token;

    while (std::getline(iss, token, ',')) {
        int value = std::stoi(token);
        cache.push_back(static_cast<uint8_t>(value));
    }

    size_t dataSize = cache.size();
    uint8_t* resArray = (uint8_t *) calloc(1, dataSize);
    std::copy(cache.begin(), cache.end(), resArray);

    if (size != nullptr) {
      *size = dataSize;
    }
    return resArray;
}
bool Module::parseUvVoiceCueStatusConfig(const std::string& value,
                                   uint32_t* usecaseMask) {
    if (!usecaseMask) {
        return false;
    }
    *usecaseMask = 0;
    std::stringstream ss(value);
    std::string token;

    while (std::getline(ss, token, ',')) {
        size_t pos = token.find(':');
        if (pos == std::string::npos) {
            LOG(ERROR) << __func__ << ": invalid token: " << token;
            return false;
        }
        std::string key = token.substr(0, pos);
        std::string boolStr = token.substr(pos + 1);
        if (boolStr == "false") {
            continue;
        }
        if (boolStr != "true") {
            LOG(ERROR) << __func__ << ": invalid bool value: " << boolStr;
            return false;
        }
        if (key == "audio") {
            *usecaseMask |= UV_FLUENCE_AUDIO_BIT;
        } else if (key == "voice") {
            *usecaseMask |= UV_FLUENCE_TELEPHONY_BIT;
        } else if (key == "voip") {
            *usecaseMask |= UV_FLUENCE_VOIP_BIT;
        } else if (key == "sva") {
            *usecaseMask |= UV_FLUENCE_SVA_BIT;
        } else {
            LOG(ERROR) << __func__ << ": unknown key: " << key;
            return false;
        }
    }
    return true;
}
// static
Module::SetParameterToFeatureMap Module::fillSetParameterToFeatureMap() {
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
                                 {Parameters::kVoiceCRSDevice, Feature::TELEPHONY},
                                 {Parameters::kVoiceIsCRsDeviceSupported, Feature::TELEPHONY},
                                 {Parameters::kVolumeBoost, Feature::TELEPHONY},
                                 {Parameters::kVoiceSlowTalk, Feature::TELEPHONY},
                                 {Parameters::kVoiceHDVoice, Feature::TELEPHONY},
                                 {Parameters::kVoiceDeviceMute, Feature::TELEPHONY},
                                 {Parameters::kVoiceDirection, Feature::TELEPHONY},
                                 {Parameters::kVoiceTranslationRxMute, Feature::TELEPHONY},
                                 {Parameters::kVoiceTranslationTxMute, Feature::TELEPHONY},
                                 {Parameters::kTranslationConfig, Feature::TELEPHONY},
                                 {Parameters::kVoiceNsRxConfig, Feature::TELEPHONY},
                                 {Parameters::kUvVoiceCueEnable, Feature::GENERIC},
                                 {Parameters::kUvVoiceCueBytes, Feature::GENERIC},
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
                                 {Parameters::kHapticsIntensity, Feature::HAPTICS}};
    return map;
}

// static
Module::FeatureToSetHandlerMap Module::fillFeatureToSetHandlerMap() {
    FeatureToSetHandlerMap map{
            {Feature::GENERIC, &Module::onSetGenericParameters},
            {Feature::HDR, &Module::onSetHDRParameters},
            {Feature::TELEPHONY, &Module::onSetTelephonyParameters},
            {Feature::WFD, &Module::onSetWFDParameters},
            {Feature::FTM, &Module::onSetFTMParameters},
            {Feature::HAPTICS, &Module::onSetHapticsParameters},
    };
    return map;
}

std::vector<VendorParameter> Module::processGetVendorParameters(
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
    for (const auto& [key, value] : pendingActions) {
        const auto search = mFeatureToGetHandlerMap.find(key);
        if (search == mFeatureToGetHandlerMap.cend()) {
            LOG(ERROR) << __func__
                       << ": no handler set on Feature:" << static_cast<int>(search->first);
            continue;
        }
        auto handler = std::bind(search->second, this, value);
        auto keyResult = handler();  // a dynamic dispatch to GetHandler
        result.insert(result.end(), keyResult.begin(), keyResult.end());
    }
    return result;
}

std::vector<VendorParameter> Module::onGetAudioExtnParams(const std::vector<std::string>& ids) {
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

std::vector<VendorParameter> Module::onGetGenericParams(const std::vector<std::string>& ids) {
    std::vector<VendorParameter> results{};
    for (const auto& id : ids) {
        if (id == Parameters::kOffloadPlaySpeedSupported) {
            LOG(DEBUG) << __func__ << " " << id << " supported " << mOffloadSpeedSupported;
            std::string value = (mOffloadSpeedSupported ? "true" : "false");
            auto param = makeVendorParameter(id, value);
            results.push_back(param);
        } else if (id == Parameters::kAospClipTransitionSupport) {
            LOG(DEBUG) << __func__ << " supports " << id;
            std::string value = "true";
            auto param = makeVendorParameter(id, value);
            results.push_back(param);
        }
    }
    return results;
}

std::vector<VendorParameter> Module::onGetBluetoothParams(const std::vector<std::string>& ids) {
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
            // if a2dp enabled is true then suspend is 0, else suspend is 1
            parcel.value = a2dpEnabled ? "0" : "1";
            setParameter(parcel, param);
            results.push_back(param);
        }
    }
    return results;
}

std::vector<VendorParameter> Module::onGetHDRParameters(const std::vector<std::string>& ids) {
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

std::vector<VendorParameter> Module::onGetTelephonyParameters(const std::vector<std::string>& ids) {
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
        } else if (id == Parameters::kVoiceIsCRsDeviceSupported) {
            VendorParameter param;
            param.id = id;
            VString parcel;
            parcel.value = mTelephony->isCrsCallDeviceSupported() ? "1" : "0";
            setParameter(parcel, param);
            results.push_back(param);
        }
    }
    return results;
}

std::vector<VendorParameter> Module::onGetWFDParameters(const std::vector<std::string>& ids) {
    std::vector<VendorParameter> results{};
    for (const auto& id : ids) {
        if (id == Parameters::kCanOpenProxy) {
            VendorParameter param;
            param.id = id;
            VString parcel;
            parcel.value = "1";  // This "1" indicates WFD client can try AHAL Capture.
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

std::vector<VendorParameter> Module::onGetFTMParameters(const std::vector<std::string>& ids) {
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
Module::GetParameterToFeatureMap Module::fillGetParameterToFeatureMap() {
    GetParameterToFeatureMap map{{Parameters::kHdrRecord, Feature::HDR},
                                 {Parameters::kWnr, Feature::HDR},
                                 {Parameters::kAns, Feature::HDR},
                                 {Parameters::kOrientation, Feature::HDR},
                                 {Parameters::kInverted, Feature::HDR},
                                 {Parameters::kHdrChannelCount, Feature::HDR},
                                 {Parameters::kHdrSamplingRate, Feature::HDR},
                                 {Parameters::kFacing, Feature::HDR},
                                 {Parameters::kVoiceIsCRsSupported, Feature::TELEPHONY},
                                 {Parameters::kVoiceIsCRsDeviceSupported, Feature::TELEPHONY},
                                 {Parameters::kA2dpSuspended, Feature::BLUETOOTH},
                                 {Parameters::kCanOpenProxy, Feature::WFD},
                                 {Parameters::kWfdProxyRecordActive, Feature::WFD},
                                 {Parameters::kWfdIPAsProxyDevConnected, Feature::WFD},
                                 {Parameters::kFTMParam, Feature::FTM},
                                 {Parameters::kFTMSPKRParam, Feature::FTM},
                                 {Parameters::kFMStatus, Feature::AUDIOEXTENSION}};
    return map;
}

// static
Module::FeatureToGetHandlerMap Module::fillFeatureToGetHandlerMap() {
    FeatureToGetHandlerMap map{{Feature::HDR, &Module::onGetHDRParameters},
                               {Feature::TELEPHONY, &Module::onGetTelephonyParameters},
                               {Feature::BLUETOOTH, &Module::onGetBluetoothParams},
                               {Feature::WFD, &Module::onGetWFDParameters},
                               {Feature::FTM, &Module::onGetFTMParameters},
                               {Feature::AUDIOEXTENSION, &Module::onGetAudioExtnParams},
                               {Feature::GENERIC, &Module::onGetGenericParams}};
    return map;
}

// end of module parameters handling

const std::string Module::VendorDebug::kForceTransientBurstName = "aosp.forceTransientBurst";
const std::string Module::VendorDebug::kForceSynchronousDrainName = "aosp.forceSynchronousDrain";

std::vector<std::weak_ptr<::qti::audio::core::StreamOut>> Module::mStreamsOut;
std::vector<std::weak_ptr<::qti::audio::core::StreamIn>> Module::mStreamsIn;

std::mutex Module::outListMutex;
std::mutex Module::inListMutex;

}  // namespace qti::audio::core
