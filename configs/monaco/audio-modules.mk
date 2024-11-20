ifeq ($(TARGET_USES_QMAA),true)
    ifneq ($(TARGET_USES_QMAA_OVERRIDE_AUDIO),true)
    #QMAA Mode is enabled
    TARGET_IS_HEADLESS := true
    endif
endif

#Packages that should not be installed in QMAA are enabled here
ifneq ($(TARGET_IS_HEADLESS),true)

#AGM
AUDIO_AGM := libagmclient
AUDIO_AGM += libagmservice

ifneq ($(strip $(AUDIO_FEATURE_ENABLED_AGM_HIDL)), true)
AUDIO_AGM += libagmipcservice
endif
AUDIO_AGM += libagm
AUDIO_AGM += agmplay
AUDIO_AGM += agmcap
AUDIO_AGM += libagmmixer
AUDIO_AGM += agmcompressplay
AUDIO_AGM += libagm_mixer_plugin
AUDIO_AGM += libagm_pcm_plugin
AUDIO_AGM += libagm_compress_plugin
AUDIO_AGM += agmcompresscap
AUDIO_AGM += agmvoiceui
AUDIO_AGM += agmhostless

#PAL Module
AUDIO_PAL := libar-pal
AUDIO_PAL += catf
AUDIO_PAL += libaudiochargerlistener
AUDIO_PAL += lib_bt_bundle
AUDIO_PAL += lib_bt_aptx
AUDIO_PAL += lib_bt_ble
AUDIO_PAL += libhfp_pal
BOARD_SUPPORTS_OPENSOURCE_STHAL := true

AUDIO_HARDWARE += audio.usb.default
AUDIO_HARDWARE += audio.r_submix.default
AUDIO_HARDWARE += audio.primary.monaco

#HAL Wrapper
AUDIO_WRAPPER := libqahw
AUDIO_WRAPPER += libqahwwrapper

#PAL Service
AUDIO_PAL += libpalclient
AUDIO_PAL += libpalipcservice

ifeq ($(AUDIO_FEATURE_ENABLED_CODEC_2_0), true)
# C2 Audio
AUDIO_C2 := libqc2audio_base
AUDIO_C2 += libqc2audio_utils
AUDIO_C2 += libqc2audio_platform
AUDIO_C2 += libqc2audio_core
AUDIO_C2 += libqc2audio_basecodec
AUDIO_C2 += libqc2audio_hooks
AUDIO_C2 += libqc2audio_swaudiocodec
AUDIO_C2 += libqc2audio_swaudiocodec_data_common
AUDIO_C2 += libqc2audio_hwaudiocodec
AUDIO_C2 += libqc2audio_hwaudiocodec_data_common
AUDIO_C2 += vendor.qti.media.c2audio@1.0-service
AUDIO_C2 += qc2audio_test
AUDIO_C2 += libEvrcSwCodec
AUDIO_C2 += libQcelp13SwCodec
endif


#AUDIO_TEST := mcs_test
#AUDIO_TEST += ar_util_in_test_example


AUDIO_MODULES := ftm_test_config
AUDIO_MODULES += ftm_test_config_monaco-idp-snd-card
AUDIO_MODULES += audioadsprpcd
AUDIO_MODULES += IDP_acdb_cal_monaco_slate.acdb
AUDIO_MODULES += IDP_workspaceFileXml_monaco_slate.qwsp
AUDIO_MODULES += IDP_acdb_cal_monaco_slate_amic.acdb
AUDIO_MODULES += IDP_workspaceFileXml_monaco_slate_amic.qwsp
AUDIO_MODULES += IDP_acdb_cal_monaco_slate_wsa.acdb
AUDIO_MODULES += IDP_workspaceFileXml_monaco_slate_wsa.qwsp
AUDIO_MODULES += fai__2.6.0_0.0__3.0.0_0.0__eai_1.10.pmd
AUDIO_MODULES += fai__3.0.0_0.0__eai_1.10.pmd
AUDIO_MODULES += fai__3.0.0_0.0__eai_1.36_enpu2.pmd

AUDIO_MODULES += IDP_acdb_cal_monaco.acdb
AUDIO_MODULES += IDP_workspaceFileXml_monaco.qwsp
AUDIO_MODULES += IDP_acdb_cal_monaco_amic.acdb
AUDIO_MODULES += IDP_workspaceFileXml_monaco_amic.qwsp
AUDIO_MODULES += IDP_acdb_cal_monaco_wsa.acdb
AUDIO_MODULES += IDP_workspaceFileXml_monaco_wsa.qwsp
AUDIO_MODULES += libhotword_intf
AUDIO_MODULES += libvui_intf
AUDIO_MODULES += mm-audio-ftm

ifeq ($(PRODUCT_ENABLE_QESDK),true)
AUDIO_MODULES += libvui_dmgr
AUDIO_MODULES += libvui_dmgr_client
AUDIO_MODULES += qsap_voiceui
AUDIO_MODULES += qsap_voiceui.policy
endif

ifeq ($(strip $(AUDIO_FEATURE_ENABLED_MCS)), true)
 AUDIO_MODULES += libmcs
endif

AUDIO_MODULES += $(AUDIO_AGM)
AUDIO_MODULES += $(AUDIO_PAL)
AUDIO_MODULES += $(AUDIO_C2)
AUDIO_MODULES += $(AUDIO_TEST)

 # sound trigger aidl library
AUDIO_MODULES += libsoundtriggerhal.qti

# enable Listen Sound Model aidl 1.0
AUDIO_MODULES += \
    liblistensoundmodelaidl \
    liblistensoundmodel2vendor \
    vendor.qti.hardware.ListenSoundModelAidl-V1-ndk.vendor
# AIDL Audio modules

AUDIO_MODULES += \
    audiohalservice.qti \
    libaudiocorehal.qti \
    libaudiocorehal.default \
    libaudioeffecthal.qti

# AIDL AHAL VENDOR EXTENSION
AUDIO_MODULES += \
    libaudiohalvendorextn

LATEST_ANDROID_HARDWARE_AUDIO_EFFECT := android.hardware.audio.effect-V3-ndk
LATEST_ANDROID_HARDWARE_COMMON := android.hardware.common-V2-ndk
LATEST_ANDROID_MEDIA_ADUIO_COMMON_TYPES := android.media.audio.common.types-V4-ndk
LATEST_ANDROID_HARDWARE_COMMON_FMQ := android.hardware.common.fmq-V1-ndk

# to have similar to cc_defaults in make files
EFFECTS_DEFAULTS_SHARED_LIBRARIES := \
    $(LATEST_ANDROID_HARDWARE_AUDIO_EFFECT) \
    $(LATEST_ANDROID_HARDWARE_COMMON) \
    $(LATEST_ANDROID_MEDIA_ADUIO_COMMON_TYPES) \
    $(LATEST_ANDROID_HARDWARE_COMMON_FMQ) \
    libaudioaidlcommon \
    libbase \
    libbinder_ndk \
    libcutils \
    libfmq \
    libutils

EFFECTS_DEFAULTS_HEADERS_LIBRARIES := \
    libaudioeffectsaidlqti_headers \
    libaudio_system_headers \
    libaudioutils_headers \
    libsystem_headers

# add modules for fuzzing
ifneq ($(filter audio,$(QC_HWASAN))$(filter hwaddress,$(SANITIZE_TARGET)),)
AUDIO_MODULES += fuzz-audio-hal
endif

endif
