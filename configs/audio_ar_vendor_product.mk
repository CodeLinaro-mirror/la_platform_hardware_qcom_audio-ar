#Audio product definitions
# Override cdc specific xml files, if CDC HW is enabled
ifeq ($(TARGET_USES_CDC_HW),true)
-include vendor/qcom/proprietary/audio_hal_cust/hal/config/cdc_overlay_audio_configs.mk
endif
include vendor/qcom/opensource/audio-hal-ar/primary-hal/configs/audio-generic-modules.mk
PRODUCT_PACKAGES += $(AUDIO_GENERIC_MODULES)

PRODUCT_PACKAGES_DEBUG += $(MM_AUDIO_DBG)

#----------------------------------------------------------------------
# audio specific
#----------------------------------------------------------------------
TARGET_USES_AOSP := false
TARGET_USES_AOSP_FOR_AUDIO := false

ifeq ($(TARGET_USES_QMAA_OVERRIDE_AUDIO), false)
ifeq ($(TARGET_USES_QMAA),true)
AUDIO_USE_STUB_HAL := true
TARGET_USES_AOSP_FOR_AUDIO := true
endif
endif
ifeq ($(AUDIO_USE_STUB_HAL), true)
include $(TOPDIR)vendor/qcom/opensource/audio-hal-ar/primary-hal/configs/common/default.mk
else
$(warning TARGET_BOARD_PLATFORM is $(TARGET_BOARD_PLATFORM))
ifneq ( ,$(filter $(TARGET_BOARD_PLATFORM)$(TARGET_BOARD_SUFFIX), gen4_gvm))
include vendor/qcom/opensource/audio-hal-ar/primary-hal/configs/gen4_au/gen4_au.mk
ifeq ($(TARGET_USES_CDC_HW),true)
-include vendor/qcom/proprietary/audio_hal_cust/hal/config/cdc_audio.mk
endif
endif
ifneq ( ,$(filter $(TARGET_BOARD_PLATFORM)$(TARGET_BOARD_SUFFIX), gen5_gvm gen5_gvm_cmu gen5_gvm_gy auto_gen_prime))
include vendor/qcom/opensource/audio-hal-ar/primary-hal/configs/gen5_au/gen5_au.mk
endif
ifeq ($(TARGET_BOARD_PLATFORM)$(TARGET_BOARD_SUFFIX), msmnile_gvmq)
include vendor/qcom/opensource/audio-hal-ar/primary-hal/configs/msmnile_gvmq/msmnile_gvmq.mk
endif
endif

$(warning audio check QC_HWASAN: $(QC_HWASAN) sanitize_target $(SANITIZE_TARGET))
$(call add_soong_config_namespace,vendor_audio_hwasan_config)
ifneq ($(filter audio, $(QC_HWASAN)),)
$(warning audio hwasan enabled at module level)
AUDIO_FEATURE_USE_HWASAN_ARTIFACTS := true
PRODUCT_HWASAN_INCLUDE_PATHS += \
    vendor/qcom/opensource/audio-hal-ar \
    vendor/qcom/opensource/pal \
    vendor/qcom/opensource/agm
endif

# Pro Audio feature
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.audio.pro.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.audio.pro.xml

$(call soong_config_set, qtiaudio, var00, false)
$(call soong_config_set, qtiaudio, var11, false)
$(call soong_config_set, qtiaudio, var22, false)
$(call soong_config_set, qtiaudio, hwasan, false)


ifneq ($(BUILD_AUDIO_TECHPACK_SOURCE), true)
    $(call soong_config_set, qtiaudio, var00, true)
    $(call soong_config_set, qtiaudio, var11, true)
    $(call soong_config_set, qtiaudio, var22, true)
endif
ifeq (,$(wildcard $(QCPATH)/mm-audio-noship))
    $(call soong_config_set, qtiaudio, var11, true)
endif
ifeq (,$(wildcard $(QCPATH)/mm-audio))
    $(call soong_config_set, qtiaudio, var22, true)
endif

ifneq ($(filter hwaddress,$(SANITIZE_TARGET)),)
$(warning audio hwasan enabled at target level)
AUDIO_FEATURE_USE_HWASAN_ARTIFACTS := true
$(call soong_config_set, qtiaudio, hwasan, true)
endif

# this feature flag is only set when hwasan is enabled (local or global)
ifeq ($(AUDIO_FEATURE_USE_HWASAN_ARTIFACTS), true)
$(warning audio use hwasan artifacts)
$(call add_soong_config_var_value,vendor_audio_hwasan_config,use_hwasan,true)
else
$(call add_soong_config_var_value,vendor_audio_hwasan_config,use_hwasan,false)
endif
