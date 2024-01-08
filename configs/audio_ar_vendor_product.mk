AUDIO_USE_STUB_HAL := false
ifeq ($(TARGET_USES_QMAA),true)
ifeq ($(TARGET_USES_QMAA_OVERRIDE_AUDIO), false)
AUDIO_USE_STUB_HAL := true
endif
endif

# MM_AUDIO_AR
ifeq ($(TARGET_GVMGH_SPECIFIC), false)
MM_AUDIO_AR := acdb_cal.acdb
MM_AUDIO_AR += libautohal_pal

# AGM service is not used for GY
ifneq ($(TARGET_USES_GY), true)
MM_AUDIO_AR += libagm
MM_AUDIO_AR += libagm_compress_plugin
MM_AUDIO_AR += libagm_mixer_plugin
MM_AUDIO_AR += libagm_pcm_plugin
MM_AUDIO_AR += vendor.qti.hardware.AGMIPC@1.0
MM_AUDIO_AR += vendor.qti.hardware.AGMIPC@1.0-impl
MM_AUDIO_AR += vendor.qti.hardware.AGMIPC@1.0-service
MM_AUDIO_AR += init.qti.AGMIPC.sh
else
MM_AUDIO_AR += libarpowerpolicy
endif #ends TARGET_USES_GY

# Will remove these two 32 bit version once update to new tinyalsa lib
MM_AUDIO_AR += agmplay_32
MM_AUDIO_AR += agmcap_32
# ends 32-bit agmplay and agmcap

MM_AUDIO_AR += agmplay
MM_AUDIO_AR += agmcap
MM_AUDIO_AR += agmhostless
MM_AUDIO_AR += libagmmixer
MM_AUDIO_AR += libar-pal
MM_AUDIO_AR += libhfp_pal
MM_AUDIO_AR += libhfp_ag_pal
MM_AUDIO_AR += lib_default_plugin_controls
MM_AUDIO_AR += lib_default_set_param_plugin_controls
MM_AUDIO_AR += libqtigefar
MM_AUDIO_AR += libicc_pal
MM_AUDIO_AR += libqcompostprocbundle.ar
MM_AUDIO_AR += libqcomvisualizer.ar
MM_AUDIO_AR += libqcomvoiceprocessing.ar

ifeq ($(ENABLE_HYP), false)
ifeq ($(TARGET_GVMGH_SPECIFIC), false)
MM_AUDIO_AR += workspaceFileXml.qwsp
MM_AUDIO_AR += acdb_cal.acdbdelta
MM_AUDIO_AR += capi_avc
MM_AUDIO_AR += capi_peq
MM_AUDIO_AR += capi_bmt
MM_AUDIO_AR += capi_sumx
MM_AUDIO_AR += capi_fnb
MM_AUDIO_AR += capi_load
MM_AUDIO_AR += capi_gpio
MM_AUDIO_AR += capi_irq_comm
ifneq ( ,$(filter T Tiramisu 13, $(PLATFORM_VERSION)))
MM_AUDIO_AR += libarpowerpolicy
endif #ends Tiramisu
endif #ends TARGET_GVMGH_SPECIFIC
ifeq ($(AUDIO_USE_STUB_HAL),false)
MM_AUDIO_AR += libams
MM_AUDIO_AR += libamscore
MM_AUDIO_AR += libamsclient
MM_AUDIO_AR += libamsosal
MM_AUDIO_AR += vendor.qti.hardware.AMSIPC@1.0
MM_AUDIO_AR += vendor.qti.hardware.AMSIPC@1.0-impl
MM_AUDIO_AR += vendor.qti.hardware.AMSIPC@1.0-service
MM_AUDIO_AR += init.qti.AMSIPC.sh
MM_AUDIO_AR += ams_test
MM_AUDIO_AR += libar-gpr-ams
MM_AUDIO_AR += ams_core.cfg
endif #ends AUDIO_USE_STUB_HAL
endif #ends ENABLE_HYP

MM_AUDIO_AR += sound_trigger.primary.$(TARGET_BOARD_PLATFORM).ar
PRODUCT_PACKAGES += $(MM_AUDIO_AR)

#-------
# audio specific
# ------
TARGET_USES_AOSP := true
TARGET_USES_AOSP_FOR_AUDIO := false

#enable audio_effects
ifeq ($(ENABLE_HYP), true)
AUDIO_FRAMEWORK_AUDIOREACH := true
endif #ends ENABLE_HYP

# Audio configuration file
ifeq ($(AUDIO_USE_STUB_HAL), true)
TARGET_USES_AOSP_FOR_AUDIO := true
-include $(TOPDIR)vendor/qcom/opensource/audio-hal-ar/primary-hal/configs/common/default.mk
else
ifeq ($(call is-board-platform-in-list, gen4), true)
-include $(TOPDIR)vendor/qcom/opensource/audio-hal-ar/primary-hal/configs/gen4_au/gen4_au.mk
else
-include $(TOPDIR)vendor/qcom/opensource/audio-hal-ar/primary-hal/configs/msmnile_au/msmnile_au.mk
endif #ends gen4
endif # AUDIO_USE_STUB_HAL

ifeq ($(ENABLE_HYP), false)
ifeq ($(TARGET_BOARD_AUTO),true)
ifeq ($(TARGET_USES_RRO), true)
PRODUCT_PACKAGES += CarServiceOverlayVendor \
                    CarFrameworksOverlayVendor
endif #ends TARGET_USES_RRO
endif #ends TARGET_BOARD_AUTO
endif #ends ENABLE_HYP
endif #ends TARGET_GVMGH_SPECIFIC
