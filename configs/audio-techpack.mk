include vendor/qcom/opensource/audio-kernel/audio_kernel_modules.mk

.PHONY: audio_tp_dlkm
audio_tp_dlkm: $(AUDIO_KERNEL_MODULES)
