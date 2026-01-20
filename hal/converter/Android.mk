LOCAL_PATH := $(call my-dir)
CURRENT_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE  := libaudioplatformconverter.qti
LOCAL_MODULE_OWNER  := qti
LOCAL_MODULE_TAGS   := optional
LOCAL_VENDOR_MODULE := true

LOCAL_C_INCLUDES    += \
     $(LOCAL_PATH)/include

LOCAL_EXPORT_C_INCLUDE_DIRS   := $(LOCAL_PATH)/include

LOCAL_SRC_FILES := \
    PlatformConverter.cpp

LOCAL_SHARED_LIBRARIES := \
    $(LATEST_ANDROID_AUDIO_CORE) \
    $(LATEST_ANDROID_MEDIA_AUDIO_COMMON_TYPES) \
    libbase \
    libstagefright_foundation \
    libar-pal

include $(BUILD_SHARED_LIBRARY)
