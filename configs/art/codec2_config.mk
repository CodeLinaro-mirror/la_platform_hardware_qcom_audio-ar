
# =============================================================================
# Codec2 Audio Configuration
# Please have clarity if you are changing this
# =============================================================================

# Define the build flag
CODEC2_AUDIO_ENABLEMENT ?= false

ifeq ($(strip $(CODEC2_AUDIO_ENABLEMENT)),true)

# --- Codec2 Packages ---
AUDIO_C2 := libqc2audio_base
AUDIO_C2 += libqc2audio_utils
AUDIO_C2 += libqc2audio_platform
AUDIO_C2 += libqc2audio_core
AUDIO_C2 += libqc2audio_basecodec
AUDIO_C2 += libqc2audio_hooks
AUDIO_C2 += libqc2audio_swaudiocodec
AUDIO_C2 += libqc2audio_hwaudiocodec
AUDIO_C2 += vendor.qti.media.c2audio@1.0-service
AUDIO_C2 += libEvrcSwCodec
AUDIO_C2 += libQcelp13SwCodec
AUDIO_C2 += c2audio.vendor.base-arm.policy
AUDIO_C2 += c2audio.vendor.ext-arm.policy
AUDIO_C2 += c2audio.vendor.base-arm64.policy
AUDIO_C2 += c2audio.vendor.ext-arm64.policy

# --- Codec2 Config Files ---
PRODUCT_COPY_FILES += \
    $(TOPDIR)vendor/qcom/opensource/audio-hal/primary-hal/configs/art/codec2/media_codecs_c2_audio.xml:vendor/etc/media_codecs_c2_audio.xml \
    $(TOPDIR)vendor/qcom/opensource/audio-hal/primary-hal/configs/art/codec2/service/1.0/c2audio.vendor.base-arm.policy:vendor/etc/seccomp_policy/c2audio.vendor.base-arm.policy \
    $(TOPDIR)vendor/qcom/opensource/audio-hal/primary-hal/configs/art/codec2/service/1.0/c2audio.vendor.base-arm64.policy:vendor/etc/seccomp_policy/c2audio.vendor.base-arm64.policy \
    $(TOPDIR)vendor/qcom/opensource/audio-hal/primary-hal/configs/art/codec2/service/1.0/c2audio.vendor.ext-arm.policy:vendor/etc/seccomp_policy/c2audio.vendor.ext-arm.policy \
    $(TOPDIR)vendor/qcom/opensource/audio-hal/primary-hal/configs/art/codec2/service/1.0/c2audio.vendor.ext-arm64.policy:vendor/etc/seccomp_policy/c2audio.vendor.ext-arm64.policy

# --- Codec2 System Properties ---
# enable c2 based encoders/decoders as default NT decoders/encoders
PRODUCT_PROPERTY_OVERRIDES += \
    vendor.audio.c2.preferred=true

# Enable dmaBuf heap usage by C2 components
PRODUCT_PROPERTY_OVERRIDES += \
    debug.c2.use_dmabufheaps=1

# Enable C2 suspend
PRODUCT_PROPERTY_OVERRIDES += \
    vendor.qc2audio.suspend.enabled=true

# Enable qc2 audio sw flac frame decode
PRODUCT_PROPERTY_OVERRIDES += \
    vendor.qc2audio.per_frame.flac.dec.enabled=true

ifneq ($(GENERIC_ODM_IMAGE),true)
$(warning "Enabling codec2.0 SW only for non-generic odm build variant")
# Rank OMX SW codecs lower than OMX HW codecs
PRODUCT_PROPERTY_OVERRIDES += debug.stagefright.omx_default_rank=0
endif

endif # CODEC2_AUDIO_ENABLEMENT
