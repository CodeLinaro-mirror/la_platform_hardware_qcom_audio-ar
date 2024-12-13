ifneq ($(AUDIO_USE_STUB_HAL), true)
LOCAL_PATH := $(call my-dir)
CURRENT_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE            := libaudiocorehal.qti
LOCAL_VENDOR_MODULE     := true
LOCAL_MODULE_RELATIVE_PATH := hw

LOCAL_C_INCLUDES    :=  $(LOCAL_PATH)/include \
                        $(LOCAL_PATH)/extensions/include

LOCAL_CFLAGS := \
    -DBACKEND_NDK \
    -Wall \
    -Wextra \
    -Werror \
    -Wthread-safety

LOCAL_SRC_FILES := \
    CoreService.cpp \
    Bluetooth.cpp \
    Module.cpp \
    ModulePrimary.cpp \
    ModuleStub.cpp \
    SoundDose.cpp \
    Stream.cpp \
    StreamStub.cpp \
    Telephony.cpp \
    StreamInPrimary.cpp \
    StreamOutPrimary.cpp \
    HalOffloadEffects.cpp \
    extensions/AudioExtension.cpp \
    extensions/auto_hal.cpp

LOCAL_HEADER_LIBRARIES :=  \
    libxsdc-utils \
    libaudioeffects \
    liberror_headers \
    libaudioclient_headers \
    libaudio_system_headers \
    libmedia_helper_headers \
    libmedia_helper_headers \
    libarpal_headers

ifeq ($(ENABLE_QCOM_HAL_AUDIO_FOCUS),true)
LOCAL_CFLAGS += -DENABLE_QCOM_HAL_AUDIO_FOCUS
endif

ifeq ($(ENABLE_QCOM_AMPERE_AUDIO),true)
LOCAL_CFLAGS += -DENABLE_QCOM_AMPERE_AUDIO
endif

ifeq ($(AUDIO_FEATURE_ENABLED_ECNR_HAL),true)
LOCAL_SRC_FILES += StreamOutPrimaryOEM.cpp
LOCAL_SRC_FILES += StreamInPrimaryOEM.cpp
LOCAL_CFLAGS += -DECNR_HAL_ENABLE
# Disabling SRC by default
#LOCAL_CPPFLAGS += -DECNR_HAL_SRC_CP

ifneq (,$(filter userdebug eng,$(TARGET_BUILD_VARIANT)))
LOCAL_CPPFLAGS += -DECNR_HAL_TUNE
LOCAL_CPPFLAGS += -DECNR_HAL_DUMP_ENABLE
endif
endif

ifneq (,$(filter userdebug eng,$(TARGET_BUILD_VARIANT)))
LOCAL_CPPFLAGS += -DPCM_DUMP_HAL_ENABLE
endif


#    defaults: [
#        "latest_android_media_audio_common_types_ndk_shared",
#        "latest_android_hardware_audio_core_ndk_shared",
#    ],
# mk equivalent find a way to fix this in mk file // TODO
#    android.media.audio.common.types-V2-ndk \
#    android.hardware.audio.core-V1-ndk

LOCAL_STATIC_LIBRARIES := \
    libaudiohalutils.qti \
    libaudio_module_config.qti \
    libaudiocore.extension

LOCAL_WHOLE_STATIC_LIBRARIES := \
    libaudioplatform.qti

LOCAL_SHARED_LIBRARIES := \
    libaudioaidlcommon \
    libbase \
    libbinder_ndk \
    libcutils \
    liblog \
    libdl \
    libhidlbase \
    libhardware \
    libfmq \
    libmedia_helper \
    libstagefright_foundation \
    libutils \
    libaudioutils \
    libxml2 \
    android.hardware.common-V2-ndk \
    $(LATEST_ANDROID_MEDIA_AUDIO_COMMON_TYPES) \
    $(LATEST_ANDROID_HARDWARE_AUDIO_CORE) \
    $(LATEST_ANDROID_HARDWARE_AUDIO_EFFECT) \
    android.hardware.audio.core.sounddose-V1-ndk \
    libar-pal \
    libaudioserviceexampleimpl \
    libaudioplatformconverter.qti \
    qti-audio-types-aidl-V1-ndk

ifeq ($(ENABLE_QCOM_HAL_AUDIO_FOCUS),true)
LOCAL_SHARED_LIBRARIES += \
    android.hardware.automotive.audiocontrol-V4-ndk \
    alliance.hardware.automotive.audiocontrol.internal-V2-ndk \
    libexpat
endif

ifeq ($(ENABLE_QCOM_AMPERE_AUDIO), true)
LOCAL_SHARED_LIBRARIES += ampere.hardware.interfaces.automotive.audioparameterparser-V1-ndk
endif

include $(BUILD_SHARED_LIBRARY)

include $(CURRENT_PATH)/fuzzer/Android.mk
include $(CURRENT_PATH)/extensions/Android.mk
include $(CURRENT_PATH)/platform/Android.mk
include $(CURRENT_PATH)/utils/Android.mk
endif
