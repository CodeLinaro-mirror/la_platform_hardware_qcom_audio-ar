# MM_AUDIO_AR
ifeq ($(TARGET_GVMGH_SPECIFIC), false)
MM_AUDIO_AR := acdb_cal.acdb

MM_AUDIO_AR += agmplay
MM_AUDIO_AR += agmcap
MM_AUDIO_AR += agmhostless
MM_AUDIO_AR += libautohal_pal
MM_AUDIO_AR += libagm
MM_AUDIO_AR += libagm_compress_plugin
MM_AUDIO_AR += libagm_mixer_plugin
MM_AUDIO_AR += libagm_pcm_plugin
MM_AUDIO_AR += libagmmixer
MM_AUDIO_AR += vendor.qti.hardware.AGMIPC@1.0
MM_AUDIO_AR += vendor.qti.hardware.AGMIPC@1.0-impl
MM_AUDIO_AR += vendor.qti.hardware.AGMIPC@1.0-service
MM_AUDIO_AR += init.qti.AGMIPC.sh
MM_AUDIO_AR += libar-pal
MM_AUDIO_AR += libhfp_pal
MM_AUDIO_AR += libhfp_ag_pal
MM_AUDIO_AR += lib_default_plugin_controls
MM_AUDIO_AR += lib_default_set_param_plugin_controls
MM_AUDIO_AR += libqtigefar
MM_AUDIO_AR += libicc_pal

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
MM_AUDIO_AR += capi_irq_comm.so
endif
endif

MM_AUDIO_AR += sound_trigger.primary.$(TARGET_BOARD_PLATFORM).ar
PRODUCT_PACKAGES += $(MM_AUDIO_AR)

#-------
# audio specific
# ------
TARGET_USES_AOSP := true
TARGET_USES_AOSP_FOR_AUDIO := false

#To enable audio_effects in msmnile_gvmq
ifeq ($(TARGET_BOARD_PLATFORM)$(TARGET_BOARD_SUFFIX), msmnile_gvmq)
AUDIO_FRAMEWORK_AUDIOREACH := true
endif
# Audio configuration file
-include $(TOPDIR)vendor/qcom/opensource/audio-hal-ar/primary-hal/configs/msmnile_au/msmnile_au.mk

ifeq ($(ENABLE_HYP), false)
ifeq ($(TARGET_BOARD_AUTO),true)
ifeq ($(TARGET_USES_RRO), true)
PRODUCT_PACKAGES += CarServiceOverlayVendor \
                    CarFrameworksOverlayVendor
endif
endif
endif
endif
