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

#pragma once

#include <aidl/android/hardware/audio/core/BnModule.h>
#include <extensions/AudioExtension.h>
#include <qti-audio-core/Bluetooth.h>
#include <qti-audio-core/ChildInterface.h>
#include <qti-audio-core/ModuleConfig.h>
#include <qti-audio-core/Platform.h>
#include <qti-audio-core/PlatformGlobalCallback.h>
#include <qti-audio-core/SoundDose.h>
#include <qti-audio-core/Stream.h>
#include <qti-audio-core/Telephony.h>

#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace qti::audio::core {

class Module final : public ::aidl::android::hardware::audio::core::BnModule,
                     public PlatformGlobalCallback {
  public:
    Module();

    static std::vector<std::weak_ptr<StreamOut>>& getOutStreams() { return mStreamsOut; }
    static std::vector<std::weak_ptr<StreamIn>>& getInStreams() { return mStreamsIn; }

    // Mutex for stream lists protection
    static std::mutex outListMutex;
    static std::mutex inListMutex;

    // This value is used for all AudioPatches.
    static constexpr int32_t kMinimumStreamBufferSizeFrames = 48;
    // The maximum stream buffer size is 1 GiB = 2 ** 30 bytes;
    static constexpr int32_t kMaximumStreamBufferSizeBytes = 1 << 30;

  private:
    // #################### start of overriding APIs from IModule ####################
    ndk::ScopedAStatus setModuleDebug(
            const ::aidl::android::hardware::audio::core::ModuleDebug& in_debug) final;
    ndk::ScopedAStatus getTelephony(
            std::shared_ptr<::aidl::android::hardware::audio::core::ITelephony>* _aidl_return)
            final;
    ndk::ScopedAStatus getBluetooth(
            std::shared_ptr<::aidl::android::hardware::audio::core::IBluetooth>* _aidl_return)
            final;
    ndk::ScopedAStatus getBluetoothA2dp(
            std::shared_ptr<::aidl::android::hardware::audio::core::IBluetoothA2dp>* _aidl_return)
            final;
    ndk::ScopedAStatus getBluetoothLe(
            std::shared_ptr<::aidl::android::hardware::audio::core::IBluetoothLe>* _aidl_return)
            final;
    ndk::ScopedAStatus prepareToDisconnectExternalDevice(int32_t in_portId) final;
    ndk::ScopedAStatus connectExternalDevice(
            const ::aidl::android::media::audio::common::AudioPort& in_templateIdAndAdditionalData,
            ::aidl::android::media::audio::common::AudioPort* _aidl_return) final;
    ndk::ScopedAStatus disconnectExternalDevice(int32_t in_portId) final;
    ndk::ScopedAStatus getAudioPatches(
            std::vector<::aidl::android::hardware::audio::core::AudioPatch>* _aidl_return) final;
    ndk::ScopedAStatus getAudioPort(
            int32_t in_portId,
            ::aidl::android::media::audio::common::AudioPort* _aidl_return) final;
    ndk::ScopedAStatus getAudioPortConfigs(
            std::vector<::aidl::android::media::audio::common::AudioPortConfig>* _aidl_return)
            final;
    ndk::ScopedAStatus getAudioPorts(
            std::vector<::aidl::android::media::audio::common::AudioPort>* _aidl_return) final;
    ndk::ScopedAStatus getAudioRoutes(
            std::vector<::aidl::android::hardware::audio::core::AudioRoute>* _aidl_return) final;
    ndk::ScopedAStatus getAudioRoutesForAudioPort(
            int32_t in_portId,
            std::vector<::aidl::android::hardware::audio::core::AudioRoute>* _aidl_return) final;
    ndk::ScopedAStatus openInputStream(
            const ::aidl::android::hardware::audio::core::IModule::OpenInputStreamArguments&
                    in_args,
            ::aidl::android::hardware::audio::core::IModule::OpenInputStreamReturn* _aidl_return)
            final;
    ndk::ScopedAStatus openOutputStream(
            const ::aidl::android::hardware::audio::core::IModule::OpenOutputStreamArguments&
                    in_args,
            ::aidl::android::hardware::audio::core::IModule::OpenOutputStreamReturn* _aidl_return)
            final;
    ndk::ScopedAStatus getSupportedPlaybackRateFactors(
            SupportedPlaybackRateFactors* _aidl_return) final;
    ndk::ScopedAStatus setAudioPatch(
            const ::aidl::android::hardware::audio::core::AudioPatch& in_requested,
            ::aidl::android::hardware::audio::core::AudioPatch* _aidl_return) final;
    ndk::ScopedAStatus setAudioPortConfig(
            const ::aidl::android::media::audio::common::AudioPortConfig& in_requested,
            ::aidl::android::media::audio::common::AudioPortConfig* out_suggested,
            bool* _aidl_return) final;
    ndk::ScopedAStatus resetAudioPatch(int32_t in_patchId) final;
    ndk::ScopedAStatus resetAudioPortConfig(int32_t in_portConfigId) final;
    ndk::ScopedAStatus getMasterMute(bool* _aidl_return) final;
    ndk::ScopedAStatus setMasterMute(bool in_mute) final;
    ndk::ScopedAStatus getMasterVolume(float* _aidl_return) final;
    ndk::ScopedAStatus setMasterVolume(float in_volume) final;
    ndk::ScopedAStatus getMicMute(bool* _aidl_return) final;
    ndk::ScopedAStatus setMicMute(bool in_mute) final;
    ndk::ScopedAStatus getMicrophones(
            std::vector<::aidl::android::media::audio::common::MicrophoneInfo>* _aidl_return) final;
    ndk::ScopedAStatus updateAudioMode(
            ::aidl::android::media::audio::common::AudioMode in_mode) final;
    ndk::ScopedAStatus updateScreenRotation(
            ::aidl::android::hardware::audio::core::IModule::ScreenRotation in_rotation) final;
    ndk::ScopedAStatus updateScreenState(bool in_isTurnedOn) final;
    ndk::ScopedAStatus getSoundDose(
            std::shared_ptr<::aidl::android::hardware::audio::core::sounddose::ISoundDose>*
                    _aidl_return) final;
    ndk::ScopedAStatus generateHwAvSyncId(int32_t* _aidl_return) final;
    ndk::ScopedAStatus getVendorParameters(
            const std::vector<std::string>& in_ids,
            std::vector<::aidl::android::hardware::audio::core::VendorParameter>* _aidl_return)
            final;
    ndk::ScopedAStatus setVendorParameters(
            const std::vector<::aidl::android::hardware::audio::core::VendorParameter>&
                    in_parameters,
            bool in_async) final;
    ndk::ScopedAStatus addDeviceEffect(
            int32_t in_portConfigId,
            const std::shared_ptr<::aidl::android::hardware::audio::effect::IEffect>& in_effect)
            final;
    ndk::ScopedAStatus removeDeviceEffect(
            int32_t in_portConfigId,
            const std::shared_ptr<::aidl::android::hardware::audio::effect::IEffect>& in_effect)
            final;
    ndk::ScopedAStatus getMmapPolicyInfos(
            ::aidl::android::media::audio::common::AudioMMapPolicyType mmapPolicyType,
            std::vector<::aidl::android::media::audio::common::AudioMMapPolicyInfo>* _aidl_return)
            final;
    ndk::ScopedAStatus supportsVariableLatency(bool* _aidl_return) final;
    ndk::ScopedAStatus getAAudioMixerBurstCount(int32_t* _aidl_return) final;
    ndk::ScopedAStatus getAAudioHardwareBurstMinUsec(int32_t* _aidl_return) final;

#if AUDIO_CORE_VERSION >= 4
    ndk::ScopedAStatus getFlushFromFrameSupport(
            const ::aidl::android::media::audio::common::AudioPortConfig& in_config,
            ::aidl::android::media::audio::common::FlushFromFrameSupport* _aidl_return) final;
#endif

    binder_status_t dump(int fd, const char** args, uint32_t numArgs) final;
    // #################### end of overriding APIs from IModule ####################

    // #################### start of overriding APIs from PlatformGlobalCallback ########
    void onSoundDose(void* const eventData,
                     const ::aidl::android::media::audio::common::AudioDevice&) final;
    // #################### end of overriding APIs from PlatformGlobalCallback ########

    struct VendorDebug {
        static const std::string kForceTransientBurstName;
        static const std::string kForceSynchronousDrainName;
        bool forceTransientBurst = false;
        bool forceSynchronousDrain = false;
    };

    // ids of device ports created at runtime via 'connectExternalDevice'.
    // Also stores a list of ids of mix ports with dynamic profiles that were populated from
    // the connected port. This list can be empty, thus an int->int multimap can't be used.
    using ConnectedDevicePorts = std::map<int32_t, std::set<int32_t>>;
    // Maps port ids and port config ids to patch ids.
    // Multimap because both ports and configs can be used by multiple patches.
    using Patches = std::multimap<int32_t, int32_t>;

    /**
     * Features to be provided by Set/Get Parameters.
     * Each Feature can be associated to one or more semantically related Parameters id's.
     * Each Feature has atmost one set handler or atmost one get handler or both.
     * Such a group of Parameters acquires a Feature enum and will be
     * dealt either by set or get or both handlers.
     *
     * Example:
     * {k1,k2,k3} => F1 => SH,GH
     * {k3,k5} => F2 => SH
     * {k7} => F3 => GH
     *
     * k* -> parameter's Ids,
     * F* -> Feature enums,
     * SH -> SetHandler
     * GH -> GetHandler
     **/
    enum class Feature : uint16_t {
        GENERIC = 0,  // this enum groups much generic parameters
        TELEPHONY,
        BLUETOOTH,
        HDR,
        WFD,
        FTM,  // Factory Test Mode
        AUDIOEXTENSION,
        HAPTICS,
    };

    // For set parameters
    using SetHandler = std::function<void(
            Module*, const std::vector<::aidl::android::hardware::audio::core::VendorParameter>&)>;
    using SetParameterToFeatureMap = std::map<std::string, Feature>;
    using FeatureToSetHandlerMap = std::map<Feature, SetHandler>;

    using FeatureToVendorParametersMap =
            std::map<Feature, std::vector<::aidl::android::hardware::audio::core::VendorParameter>>;

    // For get parameters
    using GetHandler =
            std::function<std::vector<::aidl::android::hardware::audio::core::VendorParameter>(
                    Module*, const std::vector<std::string>&)>;
    using GetParameterToFeatureMap = std::map<std::string, Feature>;
    using FeatureToGetHandlerMap = std::map<Feature, GetHandler>;

    using FeatureToStringMap = std::map<Feature, std::vector<std::string>>;

    // Methods originally in Module, possibly overridden by ModulePrimary
    static void updateStreamOutList(const std::shared_ptr<StreamOut> streamOut) {
        mStreamsOut.push_back(streamOut);
    }
    static void updateStreamInList(const std::shared_ptr<StreamIn> streamIn) {
        mStreamsIn.push_back(streamIn);
    }

    void setVoipTxMute(bool mute_state);
    void setVoipRxMute(bool state);

    // start of module parameters handling
    bool processSetVendorParameters(
            const std::vector<::aidl::android::hardware::audio::core::VendorParameter>&);
    // setHandler for Generic
    void onSetGenericParameters(
            const std::vector<::aidl::android::hardware::audio::core::VendorParameter>&);
    // SetHandler For HDR
    void onSetHDRParameters(
            const std::vector<::aidl::android::hardware::audio::core::VendorParameter>&);
    // SetHandler For Telephony
    void onSetTelephonyParameters(
            const std::vector<::aidl::android::hardware::audio::core::VendorParameter>&);
    // SetHandler For WFD
    void onSetWFDParameters(
            const std::vector<::aidl::android::hardware::audio::core::VendorParameter>&);
    // SetHandler For FTM
    void onSetFTMParameters(
            const std::vector<::aidl::android::hardware::audio::core::VendorParameter>&);
    // SetHandler For Haptics
    void onSetHapticsParameters(
            const std::vector<::aidl::android::hardware::audio::core::VendorParameter>&);

    std::vector<::aidl::android::hardware::audio::core::VendorParameter> processGetVendorParameters(
            const std::vector<std::string>&);
    // GetHandler for HDR
    std::vector<::aidl::android::hardware::audio::core::VendorParameter> onGetHDRParameters(
            const std::vector<std::string>&);
    // GetHandler for Telephony
    std::vector<::aidl::android::hardware::audio::core::VendorParameter> onGetTelephonyParameters(
            const std::vector<std::string>&);
    // GetHandler for WFD
    std::vector<::aidl::android::hardware::audio::core::VendorParameter> onGetWFDParameters(
            const std::vector<std::string>&);
    // GetHandler for FTM
    std::vector<::aidl::android::hardware::audio::core::VendorParameter> onGetFTMParameters(
            const std::vector<std::string>&);
    std::vector<::aidl::android::hardware::audio::core::VendorParameter> onGetAudioExtnParams(
            const std::vector<std::string>&);
    std::vector<::aidl::android::hardware::audio::core::VendorParameter> onGetBluetoothParams(
            const std::vector<std::string>&);
    std::vector<::aidl::android::hardware::audio::core::VendorParameter> onGetGenericParams(
            const std::vector<std::string>&);
    // end of module parameters handling

    ndk::ScopedAStatus createInputStream(
            StreamContext&& context,
            const ::aidl::android::hardware::audio::common::SinkMetadata& sinkMetadata,
            const std::vector<::aidl::android::media::audio::common::MicrophoneInfo>& microphones,
            std::shared_ptr<StreamIn>* result);
    ndk::ScopedAStatus createOutputStream(
            StreamContext&& context,
            const ::aidl::android::hardware::audio::common::SourceMetadata& sourceMetadata,
            const std::optional<::aidl::android::media::audio::common::AudioOffloadInfo>&
                    offloadInfo,
            std::shared_ptr<StreamOut>* result);
    std::vector<::aidl::android::media::audio::common::AudioProfile> getDynamicProfiles(
            const ::aidl::android::media::audio::common::AudioPort& audioPort);

    void onNewPatchCreation(
            const std::vector<::aidl::android::media::audio::common::AudioPortConfig*>& sources,
            const std::vector<::aidl::android::media::audio::common::AudioPortConfig*>& sinks,
            ::aidl::android::hardware::audio::core::AudioPatch& newPatch);
    void onPrepareToDisconnectExternalDevice(
            const ::aidl::android::media::audio::common::AudioPort& audioPort);

    void setAudioPatchTelephony(
            const std::vector<::aidl::android::media::audio::common::AudioPortConfig*>& sources,
            const std::vector<::aidl::android::media::audio::common::AudioPortConfig*>& sinks,
            const ::aidl::android::hardware::audio::core::AudioPatch& patch);
    void resetAudioPatchTelephony(const ::aidl::android::hardware::audio::core::AudioPatch&);
    std::string toStringInternal();
    /**
     * Call this API only for debugging purpose
     **/
    void dumpInternal(const std::string& identifier = "no_id");

    // If the module is unable to populate the connected device port correctly,
    // the returned error code must correspond to the errors of
    // `IModule.connectedExternalDevice` method.
    ndk::ScopedAStatus populateConnectedDevicePort(
            ::aidl::android::media::audio::common::AudioPort* connectedDevicePort,
            const int32_t templateDevicePortId);
    // If the module finds that the patch endpoints configurations are not
    // matched, the returned error code must correspond to the errors of
    // `IModule.setAudioPatch` method.
    ndk::ScopedAStatus checkAudioPatchEndpointsMatch(
            const std::vector<::aidl::android::media::audio::common::AudioPortConfig*>& sources,
            const std::vector<::aidl::android::media::audio::common::AudioPortConfig*>& sinks);
    int onExternalDeviceConnectionChanged(
            const ::aidl::android::media::audio::common::AudioPort& audioPort, bool connected);
    ndk::ScopedAStatus onMasterMuteChanged(bool mute);
    ndk::ScopedAStatus onMasterVolumeChanged(float volume);
    std::unique_ptr<ModuleConfig> initializeConfig();
    /* fetch the nominal latency for the given mix port config */
    int32_t getNominalLatencyMs(const ::aidl::android::media::audio::common::AudioPortConfig&);

    // Utility and helper functions accessible to subclasses.
    void cleanUpPatch(int32_t patchId);
    ndk::ScopedAStatus createStreamContext(
            int32_t in_portConfigId, int64_t in_bufferSizeFrames,
            std::shared_ptr<::aidl::android::hardware::audio::core::IStreamCallback> asyncCallback,
            std::shared_ptr<::aidl::android::hardware::audio::core::IStreamOutEventCallback>
                    outEventCallback,
            std::string& streamName, StreamContext* out_context);
    std::vector<::aidl::android::media::audio::common::AudioDevice> findConnectedDevices(
            int32_t portConfigId);
    std::set<int32_t> findConnectedPortConfigIds(int32_t portConfigId);
    ndk::ScopedAStatus findPortIdForNewStream(
            int32_t in_portConfigId, ::aidl::android::media::audio::common::AudioPort** port);
    ModuleConfig& getConfig();
    const ConnectedDevicePorts& getConnectedDevicePorts() const { return mConnectedDevicePorts; }
    std::vector<::aidl::android::media::audio::common::AudioDevice>
    getDevicesFromDevicePortConfigIds(const std::set<int32_t>& devicePortConfigIds);
    bool getMasterMute() const { return mMasterMute; }
    bool getMasterVolume() const { return mMasterVolume; }
    const Patches& getPatches() const { return mPatches; }
    const Streams& getStreams() const { return mStreams; }
    bool isMmapSupported();
    void populateConnectedProfiles();
    template <typename C>
    std::set<int32_t> portIdsFromPortConfigIds(C portConfigIds);

    // helper functions to print human readable string for portconfig names and routes
    std::string portNameFromPortConfigIds(int portConfigId);
    std::string getPatchDetails(const ::aidl::android::hardware::audio::core::AudioPatch& patch);

    void registerPatch(const ::aidl::android::hardware::audio::core::AudioPatch& patch);
    ndk::ScopedAStatus updateStreamsConnectedState(
            const ::aidl::android::hardware::audio::core::AudioPatch& oldPatch,
            const ::aidl::android::hardware::audio::core::AudioPatch& newPatch);

    static SetParameterToFeatureMap fillSetParameterToFeatureMap();
    static FeatureToSetHandlerMap fillFeatureToSetHandlerMap();
    static GetParameterToFeatureMap fillGetParameterToFeatureMap();
    static FeatureToGetHandlerMap fillFeatureToGetHandlerMap();

    static std::vector<std::weak_ptr<::qti::audio::core::StreamOut>> mStreamsOut;
    static std::vector<std::weak_ptr<::qti::audio::core::StreamIn>> mStreamsIn;

    std::unique_ptr<ModuleConfig> mConfig;
    ::aidl::android::hardware::audio::core::ModuleDebug mDebug;
    VendorDebug mVendorDebug;
    ConnectedDevicePorts mConnectedDevicePorts;
    Streams mStreams;
    Patches mPatches;
    bool mMasterMute = false;
    float mMasterVolume = 1.0f;
    ChildInterface<SoundDose> mSoundDose;
    std::optional<bool> mIsMmapSupported;

    ChildInterface<Telephony> mTelephony;
    const SetParameterToFeatureMap mSetParameterToFeatureMap{fillSetParameterToFeatureMap()};
    const FeatureToSetHandlerMap mFeatureToSetHandlerMap{fillFeatureToSetHandlerMap()};
    const GetParameterToFeatureMap mGetParameterToFeatureMap{fillGetParameterToFeatureMap()};
    const FeatureToGetHandlerMap mFeatureToGetHandlerMap{fillFeatureToGetHandlerMap()};
    ChildInterface<::aidl::android::hardware::audio::core::IBluetooth> mBluetooth;
    ChildInterface<::aidl::android::hardware::audio::core::IBluetoothA2dp> mBluetoothA2dp;
    ChildInterface<::aidl::android::hardware::audio::core::IBluetoothLe> mBluetoothLe;
    Platform& mPlatform{Platform::getInstance()};
    AudioExtension& mAudExt{AudioExtension::getInstance()};

    bool mOffloadSpeedSupported;
};

}  // namespace qti::audio::core
