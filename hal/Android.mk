ifneq ($(AUDIO_DISABLE_HAL),true)
include $(call all-subdir-makefiles)
endif
