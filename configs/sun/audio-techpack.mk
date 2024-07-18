include $(BOARD_OPENSOURCE_DIR)/audio-hal/primary-hal/configs/sun/audio-modules.mk
include $(BOARD_OPENSOURCE_DIR)/audio-hal/primary-hal/configs/audio-generic-modules.mk
include $(BOARD_OPENSOURCE_DIR)/audio-kernel/audio_kernel_modules.mk


.PHONY: audio_tp audio_tp_hal audio_tp_dlkm

audio_tp: audio_tp_hal audio_tp_dlkm

audio_tp_hal: $(AUDIO_MODULES) $(AUDIO_GENERIC_MODULES)

audio_tp_dlkm: $(AUDIO_KERNEL_MODULES)
