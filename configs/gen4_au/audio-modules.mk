ifeq ($(TARGET_USES_QMAA),true)
    ifneq ($(TARGET_USES_QMAA_OVERRIDE_AUDIO),true)
    #QMAA Mode is enabled
    TARGET_IS_HEADLESS := true
    endif
endif

ifneq ($(filter $(TARGET_BOARD_DERIVATIVE_SUFFIX), _sdv _cdcsdv),)
$(warning ****** DISABLING AUDIO HAL FOR RBVM ******)
AUDIO_DISABLE_HAL = true
endif

#Packages that should not be installed in QMAA are enabled here
ifneq ($(TARGET_IS_HEADLESS),true)

#AGM
AUDIO_AGM := libagmclient.so
AUDIO_AGM += libagm
AUDIO_AGM += libagm_compress_plugin
AUDIO_AGM += libagm_mixer_plugin
AUDIO_AGM += libagm_pcm_plugin
AUDIO_AGM += vendor.qti.hardware.AGMIPC@1.0
AUDIO_AGM += vendor.qti.hardware.AGMIPC@1.0-impl
AUDIO_AGM += vendor.qti.hardware.AGMIPC@1.0-service
AUDIO_AGM += init.qti.AGMIPC.sh

#PAL Module
AUDIO_PAL := libar-pal
AUDIO_PAL += lib_bt_bundle
AUDIO_PAL += lib_bt_aptx
AUDIO_PAL += lib_bt_ble
AUDIO_PAL += catf
AUDIO_PAL += PalTest
AUDIO_PAL += libaudiochargerlistener
AUDIO_PAL += libhfp_pal
AUDIO_PAL += libautooemextension

AUDIO_PAL += lib_default_plugin_controls
ifeq ($(TARGET_USES_CDC_HW), true)
AUDIO_PAL += lib_oem_plugin_controls
endif
AUDIO_PAL += lib_default_set_param_plugin_controls
AUDIO_PAL += libqtigefar
AUDIO_PAL += libicc_pal
AUDIO_PAL += libagmmixer
AUDIO_PAL += libhfp_ag_pal
#PAL Service
AUDIO_PAL += libpalclient

ifeq (,$(filter $(TARGET_BUILD_VARIANT),eng,userdebug))
AUDIO_TEST += agmplay
AUDIO_TEST += agmcap
AUDIO_TEST += agmhostless
endif # eng & userdebug builds

# C2 Audio
#AUDIO_C2 := libqc2audio_base


AUDIO_MODULES += acdb_cal.acdb
AUDIO_MODULES += workspaceFileXml.qwsp

AUDIO_MODULES += $(AUDIO_AGM)
AUDIO_MODULES += $(AUDIO_PAL)
AUDIO_MODULES += $(AUDIO_C2)
AUDIO_MODULES += $(AUDIO_TEST)

# sound trigger aidl library
#AUDIO_MODULES += libsoundtriggerhal.qti

# enable Listen Sound Model aidl 1.0
#AUDIO_MODULES += \
    liblistensoundmodelaidl

# AIDL Audio modules
ifneq ($(AUDIO_DISABLE_HAL),true)
AUDIO_MODULES += \
    audiohalservice.qti \
    libaudiocorehal.qti \
    libaudiocorehal.default \
    libaudioeffecthal.qti
endif
# add modules for fuzzing
ifneq ($(filter audio,$(QC_HWASAN))$(filter hwaddress,$(SANITIZE_TARGET)),)
AUDIO_MODULES += fuzz-audio-hal
endif

endif
