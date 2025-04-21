PRODUCT_COPY_FILES += \
    vendor/qcom/opensource/audio-hal-ar/primary-hal/configs/gen4_au/cdc/audio_config.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_config.xml \

PRODUCT_PACKAGES += \
    libAudioConfigOem \
    libAWXPAL \

PRODUCT_ODM_PROPERTIES += \
vendor.audio.feature.oem_extension.enable=true
persist.vendor.max_vol_startup=16
