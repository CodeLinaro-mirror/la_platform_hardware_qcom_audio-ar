/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once

#include <string>

namespace qti::audio::core::Parameters {

/**
 * Since the parameters from the Android framework enables or disables features
 * which would impact small to big level, It is highly recommended to write
 * verbose comments for each parameter. As Parameter is composition 'id' and 'its
 * possibles values', hence list all the values with verbose explaination
 **/

// HDR Recording
const static std::string kHdrRecord{"hdr_record_on"};
const static std::string kHdrChannelCount{"hdr_audio_channel_count"};
const static std::string kHdrSamplingRate{"hdr_audio_sampling_rate"};
const static std::string kWnr{"wnr_on"};
const static std::string kAns{"ans_on"};
const static std::string kOrientation{"orientation"};
const static std::string kInverted{"inverted"};
const static std::string kFacing{"facing"};

/**
 * Audio Zoom parameters for camcorder recording.
 *
 * kAudioZoom ("audio_zoom_on"):
 *   Enables or disables audio zoom on the active input stream.
 *   Applied only when the audio source is CAMCORDER.
 *   Possible values:
 *     "true"  - enable audio zoom; sets the PAL device custom key to
 *               "audio-record-zoom-on" so the DSP uses the zoom topology.
 *     "false" - disable audio zoom; reverts to the default recording topology.
 *   Persistence: the state is held in Platform::mAudioZoomEnabled for the
 *   lifetime of the HAL session and re-applied on every configurePalDevices()
 *   call while enabled.
 *
 * kAudioZoomFactor ("zoom_factor"):
 *   Sets the zoom strength as a floating-point multiplier.
 *   Sent to the DSP via PAL_PARAM_ID_AUDIO_ZOOM_FACTOR on every active
 *   input stream.
 *   Possible values: any positive float (e.g. 1.0 = no zoom, 2.0 = 2x zoom).
 *   Persistence: the last value is cached in Platform::mAudioZoomFactor and
 *   can be retrieved via the corresponding get-parameter path.
 */
const static std::string kAudioZoom{"audio_zoom_on"};
const static std::string kAudioZoomFactor{"zoom_factor"};

// voice
const static std::string kVoiceCallState{"call_state"};
const static std::string kVoiceCallType{"call_type"};
const static std::string kVoiceVSID{"vsid"};
const static std::string kVoiceDeviceMute{"device_mute"};
const static std::string kVolumeBoost{"volume_boost"};
const static std::string kVoiceDirection{"direction"};
const static std::string kVoiceSlowTalk{"st_enable"};
const static std::string kVoiceHDVoice{"hd_voice"};
const static std::string kVoiceIsCRsSupported{"isCRSsupported"};
const static std::string kVoiceCRSCall{"crs_call"};
const static std::string kVoiceCRSVolume{"CRS_volume"};
/** kVoiceCRSDevice : Use this parameter to set the device config to
* AHAL for CRS call.
**/
const static std::string kVoiceCRSDevice{"crs_output_device"};
/** kVoiceIsCRsDeviceSupported : Use this parameter to check if AHAL
* can support device config from Telecom.
**/
const static std::string kVoiceIsCRsDeviceSupported{"isCRSDeviceSupported"};
/** kVoiceTranslationRxMute : helps to set the Voice/VoIP Rx Volume
* to mute when the param is set to enabled during the
* voice call translation usecase running.
**/
const static std::string kVoiceTranslationRxMute{"voice_translation_rx_mute"};
/** kVoiceTranslationTxMute : helps to set the Voice/VoIP Tx Volume
* to mute when the param is set to enabled during the
* voice call translation usecase running.
**/
const static std::string kVoiceTranslationTxMute{"voice_translation_tx_mute"};
/** kTranslationConfig : Use this parameter to set the config to
* ASR, TTS and NMT modules for the Voice Call Translation graph.
**/
const static std::string kTranslationConfig{"translation_config"};
/** kVoiceNsRxConfig : Use this parameter to set the config to
* Fluence NN NS module for enbale or disable the module during call.
**/
const static std::string kVoiceNsRxConfig{"voice_ns_rx_config"};
// FFECNS/FNN-UV
/** kUvVoiceCueEnable : Use this parameter to enable or disable
 * the User-Verification (UV) audio cue feature across usecases
 * such as Audio Record, SVA , Voice and VoIP usecases. When enabled, the
 * associated UV audio cue bytes (model/profile) are applied.
 **/
const static std::string kUvVoiceCueEnable{"uv_voice_cue_enable"};
/** kUvVoiceCueBytes : Use this parameter to provide the UV
 * audio cue/model data as a comma-separated list of values.
 * The HAL converts this string into a byte array and
 * forwards to configure the UV audio cue processing.
 **/
const static std::string kUvVoiceCueBytes{"uv_voice_cue_bytes"};

// WFD
const static std::string kCanOpenProxy{"can_open_proxy"};
const static std::string kWfdChannelMap{"wfd_channel_cap"};
const static std::string kWfdProxyRecordActive{"proxyRecordActive"};

/**
 * USE_IP_IN_DEVICE_FOR_PROXY_RECORD: Use this parameter to set/unset if ip-v4 in device
 * in getting used a proxy device. Set it before making the device available and unset
 * it while making device unavailable.
 **/
const static std::string kWfdIPAsProxyDevConnected{"USE_IP_IN_DEVICE_FOR_PROXY_RECORD"};
/**
 * clients have need to hardcode
 * frame count requirement per read.
 * Ideally, client should be able read
 * as AHAL provided. Still, AHAL supports
 * this way to set module vendor parameter
 * to request a custom FMQ size from client
 * FMQ size.
 * example:
 * As the session starts, client sets
 * proxy_record_fmq_size = 480

 * As session ends, client unsets
 * proxy_record_fmq_size = 0

 * After the session of proxy record finishes,
 * client is resposible to unset the module
 * vendor parameter.

 * For upcoming requirements, this way is
 * depreciated.
 **/
const static std::string kProxyRecordFMQSize{"proxy_record_fmq_size"};

// Generic
const static std::string kInCallMusic{"icmd_playback"};
const static std::string kUHQA{"UHQA"};
const static std::string kOffloadPlaySpeedSupported{"offloadVariableRateSupported"};
const static std::string kSupportsHwSuspend{"supports_hw_suspend"};
const static std::string kIsDirectPCMTrack{"is_direct_pcm_track"};
/**
 * translate_record : AUDIO_FLUENCE_FFECNS PCM_RECORD
 * Use this parameter for the Voice Translation usecase.
 * Set param support for APK to select FFECNS record and populate
 * custom key for FFECNS record based on the setparam.
 **/
 static std::string kTranslateRecord{"translate_record"};


 /**
  * This module-level parameter indicates whether the module's stream-out-async supports clip
  * transition. If the value is set to a valid string like "true", clip transition is supported. If
  * no value is returned, clip transition is not supported. This parameter becomes redundant when
  * the AIDL HAL core interface version is above 3, as clip transition is mandatory for HAL core
  * interface versions above 3.
  */
 const static std::string kAospClipTransitionSupport{"aosp.clipTransitionSupport"};

 // FTM
 const static std::string kFbspCfgWaitTime{"fbsp_cfg_wait_time"};
 const static std::string kFbspFTMWaitTime{"fbsp_cfg_ftm_time"};
 const static std::string kFbspValiWaitTime{"fbsp_v_vali_wait_time"};
 const static std::string kFbspValiValiTime{"fbsp_v_vali_vali_time"};
 const static std::string kTriggerSpeakerCall{"trigger_spkr_cal"};
 const static std::string kFTMParam{"get_ftm_param"};
 const static std::string kFTMSPKRParam{"get_spkr_cal"};

 // Audio Extn
 const static std::string kFMStatus{"fm_status"};

 // Bluetooth
 const static std::string kA2dpSuspended{"A2dpSuspended"};

 // Haptics
 const static std::string kHapticsVolume{"haptics_volume"};
 const static std::string kHapticsIntensity{"haptics_intensity"};
}; // namespace qti::audio::core::Parameters
