include vendor/qcom/opensource/audio-hal/primary-hal/configs/audio-generic-modules.mk
include vendor/qcom/opensource/audio-kernel/audio_kernel_modules.mk

ifeq ($(TARGET_BOARD_PLATFORM), canoe)
include vendor/qcom/opensource/audio-hal/primary-hal/configs/canoe/audio-modules.mk
else
include vendor/qcom/opensource/audio-hal/primary-hal/configs/sun/audio-modules.mk
endif


.PHONY: audio_tp audio_tp_hal audio_tp_dlkm

audio_tp: audio_tp_hal audio_tp_dlkm

audio_tp_hal: $(AUDIO_MODULES) $(AUDIO_GENERIC_MODULES)

audio_tp_dlkm: $(AUDIO_KERNEL_MODULES)
