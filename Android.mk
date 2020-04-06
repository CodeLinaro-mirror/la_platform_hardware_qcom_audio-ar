ifneq ($(AUDIO_USE_STUB_HAL), true)
ifeq ($(TARGET_USES_QCOM_MM_AUDIO), true)

MY_LOCAL_PATH := $(call my-dir)

include $(MY_LOCAL_PATH)/hal-pal/Android.mk
include $(MY_LOCAL_PATH)/hal-pal/audio_extn/Android.mk
include $(MY_LOCAL_PATH)/audio-effects/Android.mk
endif

#include $(MY_LOCAL_PATH)/hal/audio_extn/Android.mk
ifneq ($(TARGET_BOARD_AUTO),true)
include $(MY_LOCAL_PATH)/voice_processing/Android.mk
include $(MY_LOCAL_PATH)/mm-audio/Android.mk
include $(MY_LOCAL_PATH)/visualizer/Android.mk
include $(MY_LOCAL_PATH)/post_proc/Android.mk
include $(MY_LOCAL_PATH)/qahw/Android.mk
include $(MY_LOCAL_PATH)/qahw_api/Android.mk
endif
endif

ifeq ($(USE_LEGACY_AUDIO_DAEMON), true)
include $(MY_LOCAL_PATH)/audiod/Android.mk
endif

endif
endif
