LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_SRC_FILES := src/qti_gef_api.cpp

LOCAL_C_INCLUDES  += $(LOCAL_PATH)/api \
                     $(LOCAL_PATH)/inc \
                     system/core/include/utils

LOCAL_CFLAGS += -Werror -Wall

LOCAL_CFLAGS += -DGEF_PLATFORM_NAME=$(TARGET_BOARD_PLATFORM)

ifeq ($(strip $(AUDIO_FEATURE_ENABLED_GCOV)),true)
LOCAL_CFLAGS += --coverage -fprofile-arcs -ftest-coverage
LOCAL_CPPFLAGS += --coverage -fprofile-arcs -ftest-coverage
LOCAL_STATIC_LIBRARIES += libprofile_rt
endif

LOCAL_SHARED_LIBRARIES := \
        $(AHAL_DEFAULT_AIDL_INTERFACE_DEPENDENCIES) \
        liblog \
        libcutils \
        libdl \
        libar-pal \
        libbase \
        libaudioplatformconverter.qti

LOCAL_HEADER_LIBRARIES := libaudio_system_headers \
                          libhardware_headers \
                          libaudioeffectsaidlqti_headers

LOCAL_MODULE := libqtigefar
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := qti
LOCAL_VENDOR_MODULE := true

include $(BUILD_SHARED_LIBRARY)
