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
AUDIO_AGM += libagmipcservice
AUDIO_AGM += libagm
AUDIO_AGM += agmplay
AUDIO_AGM += agmcap
AUDIO_AGM += libagmmixer
AUDIO_AGM += agmcompressplay
AUDIO_AGM += libagm_mixer_plugin
AUDIO_AGM += libagm_pcm_plugin
AUDIO_AGM += libtinycompress_module_agm
AUDIO_AGM += agmcompresscap
AUDIO_AGM += agmvoiceui
AUDIO_AGM += agmhostless
AUDIO_AGM += libagm_mixer_plugin_native
AUDIO_AGM += libagm_pcm_plugin_native
AUDIO_AGM += libtinycompress_module_agm_native

#PAL Module
AUDIO_PAL := libar-pal
AUDIO_PAL += PalTest
AUDIO_PAL += libaudiochargerlistener
AUDIO_PAL += libhfp_pal
ifneq ($(call soong_config_get, qtiaudio, hy11), true)
ifneq ($(call soong_config_get, qtiaudio, hy22), true)
AUDIO_PAL += catf
endif
endif

#PAL Service
AUDIO_PAL += libpalclient
AUDIO_PAL += libpalipcservice
AUDIO_PAL += libpaleventnotifier
#PAL Stream Modules
AUDIO_PAL += libstream_pcm
AUDIO_PAL += libstream_soundtrigger
#PAL Sessions Modules
AUDIO_PAL += libsession_agm
AUDIO_PAL += libsession_pcm
AUDIO_PAL += libsession_pcm_config
AUDIO_PAL += libsession_config_utils
#PAL Device Modules
AUDIO_PAL += libdev_ec_ref
AUDIO_PAL += libdev_ext_ec
AUDIO_PAL += libdev_handset
AUDIO_PAL += libdev_handset_mic
AUDIO_PAL += libdev_headphone
AUDIO_PAL += libdev_headset_mic
AUDIO_PAL += libdev_speaker
AUDIO_PAL += libdev_speaker_mic
AUDIO_PAL += libdev_dummy

# C2 Audio
AUDIO_C2 := libqc2audio_base
AUDIO_C2 += libqc2audio_utils
AUDIO_C2 += libqc2audio_platform
AUDIO_C2 += libqc2audio_core
AUDIO_C2 += libqc2audio_basecodec
AUDIO_C2 += libqc2audio_hooks
AUDIO_C2 += libqc2audio_swaudiocodec
AUDIO_C2 += vendor.qti.media.c2audio@1.0-service
AUDIO_C2 += libEvrcSwCodec
AUDIO_C2 += libQcelp13SwCodec
AUDIO_C2 += c2audio.vendor.base-arm.policy
AUDIO_C2 += c2audio.vendor.ext-arm.policy
AUDIO_C2 += c2audio.vendor.base-arm64.policy
AUDIO_C2 += c2audio.vendor.ext-arm64.policy

AUDIO_TEST := mcs_test
AUDIO_TEST += ar_util_in_test_example
AUDIO_MODULES := ftm_test_config
AUDIO_MODULES += ftm_test_config_shikra-dsp-snd-card
AUDIO_MODULES += ftm_test_config_shikra-cpu-snd-card

AUDIO_MODULES += shikra_dsp_acdb_cal.acdb
AUDIO_MODULES += shikra_dsp_workspaceFileXml.qwsp
AUDIO_MODULES += shikra_cpu_acdb_cal.acdb
AUDIO_MODULES += shikra_cpu_workspaceFileXml.qwsp

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
    vendor.qti.hardware.ListenSoundModelAidl-V1-ndk.vendor \
    Manifest_IListenSoundModel.xml

# AIDL Audio modules

AUDIO_MODULES += \
    audiohalservice.qti \
    libaudiocorehal.qti \
    libaudiocorehal.default \
    libaudioeffecthal.qti

# add modules for fuzzing
ifneq ($(filter audio,$(QC_HWASAN))$(filter hwaddress,$(SANITIZE_TARGET)),)
AUDIO_MODULES += fuzz-audio-hal
endif

endif
