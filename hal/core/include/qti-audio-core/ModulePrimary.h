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
 * ​​​​​Changes from Qualcomm Innovation Center are provided under the following license:
 * Copyright (c) 2023-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once

#include <qti-audio-core/Bluetooth.h>
#include <qti-audio-core/Module.h>
#include <qti-audio-core/Platform.h>

#ifdef ENABLE_QCOM_AMPERE_AUDIO
#include <aidl/ampere/hardware/interfaces/automotive/audioparameterparser/RadioVendorParameterExt.h>
#include <aidl/ampere/hardware/interfaces/automotive/audioparameterparser/CarPlayVendorParameterExt.h>
#include <aidl/ampere/hardware/interfaces/automotive/audioparameterparser/AudioControlVendorParameterExt.h>

#include <aidl/android/hardware/automotive/audiocontrol/Reasons.h>
#include <aidl/android/hardware/automotive/audiocontrol/AudioGainConfigInfo.h>
#include <aidl/ampere/hardware/interfaces/automotive/audioparameterparser/CarPlayVendorParameterExt.h>
#include <aidl/ampere/hardware/interfaces/automotive/audioparameterparser/RadioVendorParameterExt.h>
#include <aidl/ampere/hardware/interfaces/automotive/audioparameterparser/AudioControlVendorParameterExt.h>

namespace aidl {
namespace alliance {
namespace hardware {
namespace automotive {
namespace audiocontrol {
namespace internal {

class IAudioControlInternal;

} // namespace internal
} // namespace audiocontrol
} // namespace automotive
} // namespace hardware
} // namespace alliance
} // namespace aidl

using ::aidl::ampere::hardware::interfaces::automotive::audioparameterparser::CarPlayVendorParameterExt;
using ::aidl::ampere::hardware::interfaces::automotive::audioparameterparser::AudioControlVendorParameterExt;
using ::aidl::ampere::hardware::interfaces::automotive::audioparameterparser::RadioVendorParameterExt;
using RadioAudioSource = ::aidl::ampere::hardware::interfaces::automotive::audioparameterparser::RadioVendorParameterExt::AudioSource;
using Type = ::aidl::ampere::hardware::interfaces::automotive::audioparameterparser::AudioControlVendorParameterExt::MasterMuteRequest::Type;
using ::aidl::android::hardware::automotive::audiocontrol::AudioGainConfigInfo;
using ::aidl::android::hardware::automotive::audiocontrol::Reasons;
#endif

extern "C" __attribute__((visibility("default"))) void extn_set_mute_config_for_address(char* address, bool muted, float volume);

typedef enum power_policy_status_t {
    POWER_POLICY_STATUS_OFFLINE,
    POWER_POLICY_STATUS_ONLINE
} power_policy_status_t;

extern "C" void extn_out_set_power_policy(uint8_t enable);
extern "C" void extn_in_set_power_policy(uint8_t enable);


namespace qti::audio::core {

#ifdef ENABLE_QCOM_AMPERE_AUDIO
struct FocusSession;
#endif

class ModulePrimary final : public Module {
  public:
    ModulePrimary();

    // #################### start of overriding APIs from IModule ####################
    virtual ndk::ScopedAStatus setAudioPortConfig(const ::aidl::android::media::audio::common::AudioPortConfig& in_requested, ::aidl::android::media::audio::common::AudioPortConfig* out_suggested, bool* _aidl_return) override;
    binder_status_t dump(int fd, const char** args, uint32_t numArgs) override;
    ndk::ScopedAStatus getBluetooth(
            std::shared_ptr<::aidl::android::hardware::audio::core::IBluetooth>* _aidl_return)
            override;
    ndk::ScopedAStatus getBluetoothA2dp(
            std::shared_ptr<::aidl::android::hardware::audio::core::IBluetoothA2dp>* _aidl_return)
            override;
    ndk::ScopedAStatus getBluetoothLe(
            std::shared_ptr<::aidl::android::hardware::audio::core::IBluetoothLe>* _aidl_return)
            override;
    ndk::ScopedAStatus getTelephony(
            std::shared_ptr<::aidl::android::hardware::audio::core::ITelephony>* _aidl_return)
            override;
    ndk::ScopedAStatus setVendorParameters(
            const std::vector<::aidl::android::hardware::audio::core::VendorParameter>&
                    in_parameters,
            bool in_async) override;
    ndk::ScopedAStatus getVendorParameters(
            const std::vector<std::string>& in_ids,
            std::vector<::aidl::android::hardware::audio::core::VendorParameter>* _aidl_return)
            override;
    ndk::ScopedAStatus getMicMute(bool* _aidl_return) override;
    ndk::ScopedAStatus setMicMute(bool in_mute) override;
    ndk::ScopedAStatus getMicrophones(
            std::vector<::aidl::android::media::audio::common::MicrophoneInfo>* _aidl_return)
            override;
    ndk::ScopedAStatus updateScreenState(bool in_isTurnedOn) override;
    ndk::ScopedAStatus updateScreenRotation(
            ::aidl::android::hardware::audio::core::IModule::ScreenRotation in_rotation) override;
    ndk::ScopedAStatus getSupportedPlaybackRateFactors(
            SupportedPlaybackRateFactors* _aidl_return) override;
    // #################### end of overriding APIs from IModule ####################
#ifdef ENABLE_QCOM_AMPERE_AUDIO
    void setUpPriorityFocus();
    static std::shared_ptr<::aidl::alliance::hardware::automotive::audiocontrol::internal::IAudioControlInternal> getAudioControlInternalService();
    static std::shared_ptr<::aidl::alliance::hardware::automotive::audiocontrol::internal::IAudioControlInternal> mAudioControlInternalProxy;
#endif
    // Mutex for stream lists protection
    static std::mutex outListMutex;
    static std::mutex inListMutex;

    static std::vector<std::weak_ptr<StreamOut>>& getOutStreams() { return mStreamsOut; }
    static std::vector<std::weak_ptr<StreamIn>>& getInStreams() { return mStreamsIn; }
    static std::string globalAudioSource;

  protected:
    // #################### start of overriding APIs from Module ####################
    std::string toStringInternal() override;
    void dumpInternal(const std::string& identifier = "no_id") override;
    ndk::ScopedAStatus createInputStream(
            StreamContext&& context,
            const ::aidl::android::hardware::audio::common::SinkMetadata& sinkMetadata,
            const std::vector<::aidl::android::media::audio::common::MicrophoneInfo>& microphones,
            std::shared_ptr<StreamIn>* result) override;
    ndk::ScopedAStatus createOutputStream(
            StreamContext&& context,
            const ::aidl::android::hardware::audio::common::SourceMetadata& sourceMetadata,
            const std::optional<::aidl::android::media::audio::common::AudioOffloadInfo>&
                    offloadInfo,
            std::shared_ptr<StreamOut>* result) override;
    std::vector<::aidl::android::media::audio::common::AudioProfile> getDynamicProfiles(
            const ::aidl::android::media::audio::common::AudioPort& audioPort) override;
    void onNewPatchCreation(
            const std::vector<::aidl::android::media::audio::common::AudioPortConfig*>& sources,
            const std::vector<::aidl::android::media::audio::common::AudioPortConfig*>& sinks,
            ::aidl::android::hardware::audio::core::AudioPatch& newPatch) override;
    void setAudioPatchTelephony(
            const std::vector<::aidl::android::media::audio::common::AudioPortConfig*>& sources,
            const std::vector<::aidl::android::media::audio::common::AudioPortConfig*>& sinks,
            const ::aidl::android::hardware::audio::core::AudioPatch& newPatch) override;
    void resetAudioPatchTelephony(
            const ::aidl::android::hardware::audio::core::AudioPatch&) override;
    int onExternalDeviceConnectionChanged(
            const ::aidl::android::media::audio::common::AudioPort& audioPort,
            bool connected) override;
    int32_t getNominalLatencyMs(
            const ::aidl::android::media::audio::common::AudioPortConfig&) override;
    // #################### end of overriding APIs from Module ####################

    // start of Module Parameters

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
        GENERIC = 0, // this enum groups much generic parameters
        TELEPHONY,
        BLUETOOTH,
        HDR,
        WFD,
        FTM, // Factory Test Mode
        AUDIOEXTENSION,
        HAPTICS,
#ifdef ENABLE_QCOM_AMPERE_AUDIO
        AUDIOSOURCE,
        CARPLAY,
        AUDIOCONTROL,
#endif
    };

    // For set parameters
    using SetHandler = std::function<void(
            ModulePrimary*,
            const std::vector<::aidl::android::hardware::audio::core::VendorParameter>&)>;
    using SetParameterToFeatureMap = std::map<std::string, Feature>;
    using FeatureToSetHandlerMap = std::map<Feature, SetHandler>;
    static SetParameterToFeatureMap fillSetParameterToFeatureMap();
    static FeatureToSetHandlerMap fillFeatureToSetHandlerMap();
    using FeatureToVendorParametersMap =
            std::map<Feature, std::vector<::aidl::android::hardware::audio::core::VendorParameter>>;

    // For get parameters
    using GetHandler =
            std::function<std::vector<::aidl::android::hardware::audio::core::VendorParameter>(
                    ModulePrimary*, const std::vector<std::string>&)>;
    using GetParameterToFeatureMap = std::map<std::string, Feature>;
    using FeatureToGetHandlerMap = std::map<Feature, GetHandler>;
    static GetParameterToFeatureMap fillGetParameterToFeatureMap();
    static FeatureToGetHandlerMap fillFeatureToGetHandlerMap();
    using FeatureToStringMap = std::map<Feature, std::vector<std::string>>;
    // end of Module Parameters
    static std::vector<std::weak_ptr<::qti::audio::core::StreamOut>> mStreamsOut;
    static std::vector<std::weak_ptr<::qti::audio::core::StreamIn>> mStreamsIn;

    static void updateStreamOutList(const std::shared_ptr<StreamOut> streamOut) {
        mStreamsOut.push_back(streamOut);
    }
    static void updateStreamInList(const std::shared_ptr<StreamIn> streamIn) {
        mStreamsIn.push_back(streamIn);
    }

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
#ifdef ENABLE_QCOM_AMPERE_AUDIO
    // GetHandler for Carplay
    std::vector<::aidl::android::hardware::audio::core::VendorParameter> onGetCarplayParams(
            const std::vector<std::string>&);
    std::string carplayParamConverter(::aidl::ampere::hardware::interfaces::automotive::audioparameterparser::CarPlayVendorParameterExt::Rate carplayparams);
    // GetHandler for AudioControl
    std::vector<::aidl::android::hardware::audio::core::VendorParameter> onGetAudioControlParams(
            const std::vector<std::string>&);
    // SetHandler For Carplay
    void onSetCarplayParameters(
            const std::vector<::aidl::android::hardware::audio::core::VendorParameter>&);
    // SetHandler For AudioControl
    void onSetAudioControlParameters(
            const std::vector<::aidl::android::hardware::audio::core::VendorParameter>&);
    ::android::status_t setAudioControlParameter(
                        const ::aidl::android::hardware::audio::core::VendorParameter& param);

    void onsetRadioVendorParameter(const std::vector<::aidl::android::hardware::audio::core::VendorParameter>& params);
    ::android::status_t setRadioVendorParameter(const ::aidl::android::hardware::audio::core::VendorParameter& param);

    ndk::ScopedAStatus handleMasterMute(
                const AudioControlVendorParameterExt::MasterMuteRequest& request);
    std::vector<AudioGainConfigInfo> getAudioGainConfigsForSinks();
#endif
  protected:
    bool mVolumeGaincheck=false;
    const SetParameterToFeatureMap mSetParameterToFeatureMap{fillSetParameterToFeatureMap()};
    const FeatureToSetHandlerMap mFeatureToSetHandlerMap{fillFeatureToSetHandlerMap()};
    const GetParameterToFeatureMap mGetParameterToFeatureMap{fillGetParameterToFeatureMap()};
    const FeatureToGetHandlerMap mFeatureToGetHandlerMap{fillFeatureToGetHandlerMap()};
    ChildInterface<::aidl::android::hardware::audio::core::IBluetooth> mBluetooth;
    ChildInterface<::aidl::android::hardware::audio::core::IBluetoothA2dp> mBluetoothA2dp;
    ChildInterface<::aidl::android::hardware::audio::core::IBluetoothLe> mBluetoothLe;
    Platform& mPlatform{Platform::getInstance()};
    AudioExtension& mAudExt{AudioExtension::getInstance()};

  private:
#ifdef ENABLE_QCOM_AMPERE_AUDIO
    ::android::status_t setCarPlayParameter(
            const ::aidl::android::hardware::audio::core::VendorParameter&);
    static std::unordered_map<std::string, FocusSession> mActiveFocusDevices;
#endif
    const std::string mGainVolumecheckProperty{"vendor.audio.feature.oemgainconversion.enable"};
    bool mOffloadSpeedSupported;
};

class MuteConfig {
public:
    static MuteConfig& GetInstance() {
        static MuteConfig instance;
        return instance;
    }
    ModuleConfig& getConfig();
    static void set_mute_config_for_address(char* address, bool muted, float volume);

private:
    MuteConfig() {
        std::cout << "MuteConfig Instance Created" << std::endl;
    }
    ~MuteConfig() {}
    static std::vector<float> getVol;
};

} // namespace qti::audio::core
