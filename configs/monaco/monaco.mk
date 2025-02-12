DEVICE_SKU := $(TARGET_PRODUCT)

#Audio paths
CONFIG_PAL_SRC_DIR := $(TOPDIR)$(BOARD_OPENSOURCE_DIR)/pal/configs/monaco
CONFIG_HAL_SRC_DIR := $(TOPDIR)$(BOARD_OPENSOURCE_DIR)/audio-hal/primary-hal/configs/monaco
CONFIG_HAL_COMMON_SRC_DIR := $(TOPDIR)$(BOARD_OPENSOURCE_DIR)/audio-hal/primary-hal/configs/common
CONFIG_SKU_OUT_DIR := $(TARGET_COPY_OUT_VENDOR)/etc/audio/sku_$(DEVICE_SKU)
# Audio product definitions
include $(CONFIG_HAL_SRC_DIR)/audio-modules.mk
PRODUCT_PACKAGES += $(AUDIO_MODULES)
#AUDIO_FEATURE_FLAGS
ifeq ($(TARGET_USES_QMAA_OVERRIDE_AUDIO), false)
ifeq ($(TARGET_USES_QMAA),true)
AUDIO_USE_STUB_HAL := true
endif
endif

ifneq ($(AUDIO_USE_STUB_HAL), true)
BOARD_USES_ALSA_AUDIO := true

ifeq ($(TARGET_SUPPORTS_WEAR_ANDROID), true)
AUDIO_FEATURE_ENABLED_CODEC_2_0 := true
endif

ifneq ($(TARGET_USES_AOSP_FOR_AUDIO), true)
AUDIO_FEATURE_ENABLED_AUDIOSPHERE := true
ifeq ($(filter R% r%,$(TARGET_PLATFORM_VERSION)),)
AUDIO_FEATURE_ENABLED_3D_AUDIO := true
endif
endif
ifeq ($(TARGET_USES_AGM_HIDL), true)
AUDIO_FEATURE_ENABLED_AGM_HIDL := true
endif
AUDIO_FEATURE_ENABLED_DLKM := false
AUDIO_FEATURE_ENABLED_INSTANCE_ID := true
AUDIO_FEATURE_ENABLED_DYNAMIC_LOG := true
MM_AUDIO_ENABLED_FTM := true
TARGET_USES_QCOM_MM_AUDIO := true
AUDIO_FEATURE_ENABLED_SVA_MULTI_STAGE := true
ifeq ($(TARGET_SUPPORTS_WEAR_AON),true)
AUDIO_FEATURE_ENABLE_BT_A2DP_LPI := true
AUDIO_FEATURE_ENABLED_MCS := true
endif
TARGET_USES_QTI_TINYCOMPRESS := false

PRODUCT_PACKAGES += $(AUDIO_AGM)
PRODUCT_PACKAGES += $(AUDIO_PAL)
ifeq ($(AUDIO_FEATURE_ENABLED_CODEC_2_0), true)
PRODUCT_PACKAGES += $(AUDIO_C2)
endif

PRODUCT_COPY_FILES += \
    $(CONFIG_HAL_SRC_DIR)/audio_effects.conf:$(CONFIG_SKU_OUT_DIR)/audio_effects.conf \
    $(CONFIG_HAL_SRC_DIR)/audio_effects.xml:$(CONFIG_SKU_OUT_DIR)/audio_effects.xml \
    $(CONFIG_HAL_SRC_DIR)/audio_effects_config.xml:$(CONFIG_SKU_OUT_DIR)/audio_effects_config.xml \
    $(CONFIG_PAL_SRC_DIR)/mixer_paths_monaco_idp.xml:$(CONFIG_SKU_OUT_DIR)/mixer_paths_monaco_idp.xml \
    $(CONFIG_PAL_SRC_DIR)/mixer_paths_monaco_idp_amic.xml:$(CONFIG_SKU_OUT_DIR)/mixer_paths_monaco_idp_amic.xml \
    $(CONFIG_PAL_SRC_DIR)/mixer_paths_monaco_idp_wsa.xml:$(CONFIG_SKU_OUT_DIR)/mixer_paths_monaco_idp_wsa.xml \
    $(CONFIG_PAL_SRC_DIR)/mixer_paths_monaco_idp_slate.xml:$(CONFIG_SKU_OUT_DIR)/mixer_paths_monaco_idp_slate.xml \
    $(CONFIG_PAL_SRC_DIR)/mixer_paths_monaco_idp_slate_amic.xml:$(CONFIG_SKU_OUT_DIR)/mixer_paths_monaco_idp_slate_amic.xml \
    $(CONFIG_PAL_SRC_DIR)/mixer_paths_monaco_idp_slate_wsa.xml:$(CONFIG_SKU_OUT_DIR)/mixer_paths_monaco_idp_slate_wsa.xml \
    frameworks/native/data/etc/android.hardware.audio.pro.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.audio.pro.xml \
    $(CONFIG_PAL_SRC_DIR)/resourcemanager_monaco_idp.xml:$(CONFIG_SKU_OUT_DIR)/resourcemanager_monaco_idp.xml \
    $(CONFIG_PAL_SRC_DIR)/resourcemanager_monaco_idp_amic.xml:$(CONFIG_SKU_OUT_DIR)/resourcemanager_monaco_idp_amic.xml \
    $(CONFIG_PAL_SRC_DIR)/resourcemanager_monaco_idp_wsa.xml:$(CONFIG_SKU_OUT_DIR)/resourcemanager_monaco_idp_wsa.xml \
    $(CONFIG_PAL_SRC_DIR)/resourcemanager_monaco_idp_slate.xml:$(CONFIG_SKU_OUT_DIR)/resourcemanager_monaco_idp_slate.xml \
    $(CONFIG_PAL_SRC_DIR)/resourcemanager_monaco_idp_slate_amic.xml:$(CONFIG_SKU_OUT_DIR)/resourcemanager_monaco_idp_slate_amic.xml \
    $(CONFIG_PAL_SRC_DIR)/resourcemanager_monaco_idp_slate_wsa.xml:$(CONFIG_SKU_OUT_DIR)/resourcemanager_monaco_idp_slate_wsa.xml \
    $(CONFIG_PAL_SRC_DIR)/usecaseKvManager.xml:$(TARGET_COPY_OUT_VENDOR)/etc/usecaseKvManager.xml \
    $(CONFIG_PAL_SRC_DIR)/card-defs.xml:$(TARGET_COPY_OUT_VENDOR)/etc/card-defs.xml

ifeq ($(AUDIO_FEATURE_ENABLED_MCS),true)
PRODUCT_COPY_FILES += \
    $(CONFIG_PAL_SRC_DIR)/mcs_defs_monaco_idp_slate.xml:$(CONFIG_SKU_OUT_DIR)/mcs_defs_monaco_idp_slate.xml \
    $(CONFIG_PAL_SRC_DIR)/mcs_defs_monaco_idp.xml:$(CONFIG_SKU_OUT_DIR)/mcs_defs_monaco_idp.xml \
    $(CONFIG_PAL_SRC_DIR)/mcs_defs_monaco_idp_wsa.xml:$(CONFIG_SKU_OUT_DIR)/mcs_defs_monaco_idp_wsa.xml
endif

#XML Audio configuration files
ifeq ($(TARGET_SUPPORTS_WEAR_ANDROID), true)
PRODUCT_COPY_FILES += \
    $(CONFIG_HAL_SRC_DIR)/audio_policy_configuration.xml:$(CONFIG_SKU_OUT_DIR)/audio_policy_configuration.xml
endif
ifeq ($(TARGET_SUPPORTS_WEAR_OS), true)
PRODUCT_COPY_FILES += \
    $(CONFIG_HAL_SRC_DIR)/audio_policy_configuration_lw.xml:$(CONFIG_SKU_OUT_DIR)/audio_policy_configuration.xml
else
PRODUCT_COPY_FILES += \
    $(CONFIG_HAL_COMMON_SRC_DIR)/audio_policy_configuration.xml:$(CONFIG_SKU_OUT_DIR)/audio_policy_configuration.xml
endif
PRODUCT_COPY_FILES += \
    $(CONFIG_HAL_SRC_DIR)/audio_module_config_primary_lw.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio/audio_module_config_primary.xml

# XML config file for memory logger
PRODUCT_COPY_FILES += \
    $(CONFIG_HAL_SRC_DIR)/mem_logger_config.xml:$(TARGET_COPY_OUT_VENDOR)/etc/mem_logger_config.xml

PRODUCT_COPY_FILES += \
    $(TOPDIR)frameworks/av/services/audiopolicy/config/a2dp_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/a2dp_audio_policy_configuration.xml \
    $(TOPDIR)frameworks/av/services/audiopolicy/config/audio_policy_volumes.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_volumes.xml \
    $(TOPDIR)frameworks/av/services/audiopolicy/config/default_volume_tables.xml:$(TARGET_COPY_OUT_VENDOR)/etc/default_volume_tables.xml \
    $(TOPDIR)frameworks/av/services/audiopolicy/config/r_submix_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/r_submix_audio_policy_configuration.xml \
    $(TOPDIR)frameworks/av/services/audiopolicy/config/usb_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/usb_audio_policy_configuration.xml \
    $(CONFIG_HAL_COMMON_SRC_DIR)/bluetooth_qti_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/bluetooth_qti_audio_policy_configuration.xml

# C2 Audio files
ifeq ($(AUDIO_FEATURE_ENABLED_CODEC_2_0), true)
PRODUCT_COPY_FILES += \
    $(CONFIG_HAL_COMMON_SRC_DIR)/codec2/media_codecs_c2_audio.xml:vendor/etc/media_codecs_c2_audio.xml \
    $(CONFIG_HAL_COMMON_SRC_DIR)/media_codecs_vendor_audio.xml:vendor/etc/media_codecs_vendor_audio.xml \
    $(CONFIG_HAL_COMMON_SRC_DIR)/codec2/service/1.0/c2audio.vendor.base-arm.policy:vendor/etc/seccomp_policy/c2audio.vendor.base-arm.policy \
    $(CONFIG_HAL_COMMON_SRC_DIR)/codec2/service/1.0/c2audio.vendor.base-arm64.policy:vendor/etc/seccomp_policy/c2audio.vendor.base-arm64.policy \
    $(CONFIG_HAL_COMMON_SRC_DIR)/codec2/service/1.0/c2audio.vendor.ext-arm.policy:vendor/etc/seccomp_policy/c2audio.vendor.ext-arm.policy \
    $(CONFIG_HAL_COMMON_SRC_DIR)/codec2/service/1.0/c2audio.vendor.ext-arm64.policy:vendor/etc/seccomp_policy/c2audio.vendor.ext-arm64.policy
endif
PRODUCT_COPY_FILES += \
    $(CONFIG_HAL_SRC_DIR)/vendor_audio_interfaces.xml:$(CONFIG_SKU_OUT_DIR)/vendor_audio_interfaces.xml

# Low latency audio buffer size in frames
PRODUCT_PROPERTY_OVERRIDES += \
    vendor.audio_hal.period_size=192

#Enable audio track offload by default
PRODUCT_PROPERTY_OVERRIDES += \
vendor.audio.offload.track.enable=true

#Disable Multiple offload sesison
PRODUCT_PROPERTY_OVERRIDES += \
vendor.audio.offload.multiple.enabled=false

#flac sw decoder 24 bit decode capability
PRODUCT_PROPERTY_OVERRIDES += \
vendor.audio.flac.sw.decoder.24bit=true

# A2DP offload support
PRODUCT_PROPERTY_OVERRIDES += \
ro.bluetooth.a2dp_offload.supported=true

# Disable A2DP offload
PRODUCT_PROPERTY_OVERRIDES += \
persist.bluetooth.a2dp_offload.disabled=false

#enable software decoders for ALAC and APE
PRODUCT_PROPERTY_OVERRIDES += \
vendor.audio.use.sw.alac.decoder=true
PRODUCT_PROPERTY_OVERRIDES += \
vendor.audio.use.sw.ape.decoder=true

#enable software decoder for MPEG-H
PRODUCT_PROPERTY_OVERRIDES += \
vendor.audio.use.sw.mpegh.decoder=true

#enable hw aac encoder by default
PRODUCT_PROPERTY_OVERRIDES += \
vendor.audio.hw.aac.encoder=false

#offload minimum duration set to 30sec
PRODUCT_PRODUCT_PROPERTIES += \
audio.offload.min.duration.secs=30

#ADM Buffering size in ms
PRODUCT_PROPERTY_OVERRIDES += \
vendor.audio.adm.buffering.ms=2

#enable headset calibration
PRODUCT_PROPERTY_OVERRIDES += \
vendor.audio.volume.headset.gain.depcal=true
ifeq ($(AUDIO_FEATURE_ENABLED_CODEC_2_0), true)
#enable c2 based encoders/decoders as default NT decoders/encoders
PRODUCT_PROPERTY_OVERRIDES += \
vendor.audio.c2.preferred=true

#Enable dmaBuf heap usage by C2 components
PRODUCT_PROPERTY_OVERRIDES += \
debug.c2.use_dmabufheaps=1

#Enable qc2 audio sw flac frame decode
PRODUCT_PROPERTY_OVERRIDES += \
vendor.qc2audio.per_frame.flac.dec.enabled=true

ifneq ($(GENERIC_ODM_IMAGE),true)
$(warning "Enabling codec2.0 SW only for non-generic odm build variant")
#Rank OMX SW codecs lower than OMX HW codecs
PRODUCT_PROPERTY_OVERRIDES += debug.stagefright.omx_default_rank=0
endif
endif
endif

#enable keytone FR
PRODUCT_PROPERTY_OVERRIDES += \
vendor.audio.hal.output.suspend.supported=true

#enable AAC frame ctl for A2DP sinks
PRODUCT_PROPERTY_OVERRIDES += \
persist.vendor.bt.aac_frm_ctl.enabled=true

#add dynamic feature flags here
PRODUCT_PROPERTY_OVERRIDES += \
vendor.audio.feature.a2dp_offload.enable=true \
vendor.audio.feature.battery_listener.enable=true \
vendor.audio.feature.hfp.enable=true \
vendor.audio.feature.kpi_optimize.enable=true \
vendor.audio.gsl.shmem.dmaheap.uncached=true
AUDIO_FEATURE_ENABLED_GKI := true
BUILD_AUDIO_TECHPACK_SOURCE := true

include $(CONFIG_HAL_SRC_DIR)/audio-properties.mk
