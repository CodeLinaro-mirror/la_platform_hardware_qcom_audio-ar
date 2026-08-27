PRODUCT_SOONG_NAMESPACES += \
    vendor/qcom/opensource/audio-hal-ar/primary-hal/configs/msmnile_gvmq \

PRODUCT_PRIVATE_SEPOLICY_DIRS += frameworks/av/services/audiopolicy/engineconfigurable/sepolicy

PRODUCT_PACKAGES += \
    audio_policy_engine_configuration_va.xml \
    parameter-framework.policy \
    libaudiopolicyengineconfigurable \
    libpolicy-subsystem \
    libparameter

PRODUCT_PACKAGES_DEBUG += remote-process
