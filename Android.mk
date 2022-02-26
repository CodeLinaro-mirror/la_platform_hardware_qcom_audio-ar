ifneq ($(AUDIO_USE_STUB_HAL), true)
ifeq ($(TARGET_USES_QCOM_MM_AUDIO), true)

MY_LOCAL_PATH := $(call my-dir)

include $(MY_LOCAL_PATH)/hal-pal/Android.mk
include $(MY_LOCAL_PATH)/hal-pal/audio_extn/Android.mk
include $(MY_LOCAL_PATH)/audio-effects/Android.mk

endif
endif
