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

LOCAL_WHOLE_STATIC_LIBRARIES := libaudio_microphoneinfo_parser

LOCAL_STATIC_LIBRARIES := \
    libaudiohalutils.qti

LOCAL_SHARED_LIBRARIES := \
    $(LATEST_ANDROID_AUDIO_CORE) \
    $(LATEST_ANDROID_MEDIA_AUDIO_COMMON_TYPES) \
    $(LATEST_QTI_AUDIO_TYPES_AIDL) \
    libbinder_ndk \
    libbase \
    libstagefright_foundation \
    libaudioaidlcommon \
    libaudioplatformconverter.qti \
    libar-pal

include $(BUILD_STATIC_LIBRARY)
