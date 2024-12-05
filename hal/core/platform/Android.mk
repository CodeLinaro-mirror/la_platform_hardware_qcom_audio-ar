LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE  := libaudioplatform.qti
LOCAL_MODULE_OWNER  := qti
LOCAL_MODULE_TAGS   := optional
LOCAL_VENDOR_MODULE := true

LOCAL_C_INCLUDES    += \
     $(LOCAL_PATH)/include \
     $(LOCAL_PATH)/../extensions/include \
     $(TOP)/system/media/audio/include \
     $(TOP)/hardware/libhardware/include

LOCAL_EXPORT_C_INCLUDE_DIRS   := $(LOCAL_PATH)/include

LOCAL_SRC_FILES := \
    Platform.cpp \
    AudioUsecase.cpp \
    PlatformUtils.cpp

ifeq ($(AUDIO_FEATURE_ENABLED_ECNR_HAL),true)
LOCAL_CFLAGS += -DECNR_HAL_ENABLE

ifneq (,$(filter userdebug eng,$(TARGET_BUILD_VARIANT)))
LOCAL_CPPFLAGS += -DECNR_HAL_TUNE
LOCAL_CPPFLAGS += -DECNR_HAL_DUMP_ENABLE
endif

endif

LOCAL_WHOLE_STATIC_LIBRARIES := libaudio_microphoneinfo_parser

LOCAL_HEADER_LIBRARIES := \
     libarpal_headers

LOCAL_STATIC_LIBRARIES := \
    libaudiohalutils.qti

LOCAL_SHARED_LIBRARIES := \
    libbinder_ndk \
    libbase \
    libstagefright_foundation \
    android.hardware.audio.core-V2-ndk \
    android.media.audio.common.types-V3-ndk \
    libaudioaidlcommon \
    qti-audio-types-aidl-V1-ndk \
    libaudioplatformconverter.qti \
    libar-pal

include $(BUILD_STATIC_LIBRARY)
