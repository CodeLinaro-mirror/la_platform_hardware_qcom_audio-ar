#Audio product definitions
include vendor/qcom/opensource/audio-hal/primary-hal/configs/audio-generic-modules.mk
PRODUCT_PACKAGES += $(AUDIO_GENERIC_MODULES)

PRODUCT_PACKAGES_DEBUG += $(MM_AUDIO_DBG)

$(warning audio check QC_HWASAN: $(QC_HWASAN) sanitize_target $(SANITIZE_TARGET))
$(call add_soong_config_namespace,vendor_audio_hwasan_config)
ifneq ($(filter audio, $(QC_HWASAN)),)
$(warning audio hwasan enabled at module level)
AUDIO_FEATURE_USE_HWASAN_ARTIFACTS := true
PRODUCT_HWASAN_INCLUDE_PATHS += \
    vendor/qcom/opensource/audio-hal \
    vendor/qcom/opensource/pal \
    vendor/qcom/opensource/agm
endif

# Pro Audio feature
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.audio.pro.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.audio.pro.xml

SOONG_CONFIG_qtiaudio_var00 := false
SOONG_CONFIG_qtiaudio_var11 := false
SOONG_CONFIG_qtiaudio_var22 := false
SOONG_CONFIG_qtiaudio_hwasan := false
SOONG_CONFIG_qtiaudio_hy00 := false
SOONG_CONFIG_qtiaudio_hy11 := false
SOONG_CONFIG_qtiaudio_hy22 := false

ifeq (,$(wildcard $(QCPATH)/audio-algos))
    SOONG_CONFIG_qtiaudio_var11 := true
    SOONG_CONFIG_qtiaudio_hy11 := true
endif

ifeq (,$(wildcard $(QCPATH)/mm-audio))
    SOONG_CONFIG_qtiaudio_var22 := true
    SOONG_CONFIG_qtiaudio_hy22 := true
endif

ifneq ($(filter hwaddress,$(SANITIZE_TARGET)),)
$(warning audio hwasan enabled at target level)
AUDIO_FEATURE_USE_HWASAN_ARTIFACTS := true
SOONG_CONFIG_qtiaudio_hwasan := true
endif


############# start of choosing AHAL core version ###############################

# CAM AHAL experimenting with AHAL to support the latest Android platform
# versions N and N-1.

# Idea is to support any target compiling with N or N-1.
# Right now N is 17 or otherwise called CinnamonBun.
# Based on the Android version, we would like to piggyback the core HAL version too.
# This is required because, primary HAL cannot use different AIDL version than
# other AHAL's like USB or R_SUBMIX. Hence, we try match them.
# Todo, try to expose its AIDL version. Todo with Google.

AHAL_INTERNAL_PLATFORM_VERSION_MAJOR := $(word 1,$(subst ., ,$(PLATFORM_VERSION)))

# Decide CORE_HAL_AIDL_VERSION:
# - Dev/trunk: PLATFORM_VERSION_CODENAME is a codename (e.g. CinnamonBun)
# - Release:   PLATFORM_VERSION is numeric-ish (e.g. 17 / 17.0 / 17.0.0)
ifneq (,$(filter CinnamonBun,$(PLATFORM_VERSION_CODENAME)))
  CORE_HAL_AIDL_VERSION := 4
  $(warning Audio HAL codename '$(PLATFORM_VERSION_CODENAME)' chooses CORE_HAL_AIDL_VERSION=$(CORE_HAL_AIDL_VERSION))
else ifeq ($(AHAL_INTERNAL_PLATFORM_VERSION_MAJOR),17)
  CORE_HAL_AIDL_VERSION := 4
  $(warning Audio HAL platform major '$(AHAL_INTERNAL_PLATFORM_VERSION_MAJOR)' chooses CORE_HAL_AIDL_VERSION=$(CORE_HAL_AIDL_VERSION))
else
  CORE_HAL_AIDL_VERSION := 3
  $(warning Audio HAL fallback (PLATFORM_VERSION='$(PLATFORM_VERSION)', CODENAME='$(PLATFORM_VERSION_CODENAME)') chooses CORE_HAL_AIDL_VERSION=$(CORE_HAL_AIDL_VERSION))
endif

$(warning Audio HAL compiling with \
  PLATFORM_VERSION=$(PLATFORM_VERSION) \
  PLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION) \
  PLATFORM_VERSION_CODENAME=$(PLATFORM_VERSION_CODENAME) \
  RELEASE_PLATFORM_VERSION_LAST_STABLE=$(RELEASE_PLATFORM_VERSION_LAST_STABLE) \
  AHAL_INTERNAL_PLATFORM_VERSION_MAJOR=$(AHAL_INTERNAL_PLATFORM_VERSION_MAJOR) \
  CORE_HAL_AIDL_VERSION=$(CORE_HAL_AIDL_VERSION))

############# end of choosing AHAL core version #################################

# this feature flag is only set when hwasan is enabled (local or global)
ifeq ($(AUDIO_FEATURE_USE_HWASAN_ARTIFACTS), true)
$(warning audio use hwasan artifacts)
$(call add_soong_config_var_value,vendor_audio_hwasan_config,use_hwasan,true)
else
$(call add_soong_config_var_value,vendor_audio_hwasan_config,use_hwasan,false)
endif

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
-include $(TOPDIR)vendor/qcom/opensource/audio-hal/primary-hal/configs/common/default.mk
else
-include $(TOPDIR)vendor/qcom/opensource/audio-hal/primary-hal/configs/$(TARGET_BOARD_PLATFORM)/$(TARGET_BOARD_PLATFORM).mk
endif

-include $(TOPDIR)vendor/qcom/opensource/audio-hal/primary-hal/configs/common/version_manager/versions.mk

ifneq ($(BUILD_AUDIO_TECHPACK_SOURCE), true)
    SOONG_CONFIG_qtiaudio_var00 := true
    SOONG_CONFIG_qtiaudio_var11 := true
    SOONG_CONFIG_qtiaudio_var22 := true
    SOONG_CONFIG_qtiaudio_hy00 := true
    SOONG_CONFIG_qtiaudio_hy11 := true
    SOONG_CONFIG_qtiaudio_hy22 := true
endif

.PHONY: audio_tp audio_tp_hal

audio_tp: audio_tp_hal

audio_tp_hal: $(AUDIO_MODULES) $(AUDIO_GENERIC_MODULES)
