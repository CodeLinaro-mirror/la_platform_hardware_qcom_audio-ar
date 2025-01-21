ifneq ($(AUDIO_USE_STUB_HAL), true)
ifeq ($(AUDIO_AMPERE_EFFECTS), true)
LOCAL_PATH = $(call my-dir)
include $(LOCAL_PATH)/rsleffects/Android.mk
include $(CLEAR_VARS)
endif
endif
