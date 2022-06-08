# MM_AUDIO_AR
ifeq ($(TARGET_BOARD_PLATFORM)$(TARGET_BOARD_SUFFIX), msmnile_gvmq)
MM_AUDIO_AR := acdb_cal.acdb

MM_AUDIO_AR += agmplay
MM_AUDIO_AR += agmcap
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
MM_AUDIO_AR += lib_default_plugin_controls

MM_AUDIO_AR += sound_trigger.primary.$(TARGET_BOARD_PLATFORM).ar
PRODUCT_PACKAGES += $(MM_AUDIO_AR)

#-------
# audio specific
# ------
TARGET_USES_AOSP := true
TARGET_USES_AOSP_FOR_AUDIO := true

# sdm845 specific rules
ifeq ($(TARGET_BOARD_PLATFORM),sdm845)
TARGET_USES_AOSP := false
TARGET_USES_AOSP_FOR_AUDIO := false
endif

# Audio configuration file
-include $(TOPDIR)vendor/qcom/opensource/audio-hal-ar/primary-hal/configs/msmnile_au/msmnile_au.mk
endif
