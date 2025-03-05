LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE            := libaudiocore.extension
LOCAL_VENDOR_MODULE     := true
LOCAL_C_INCLUDES            := $(LOCAL_PATH)/include \
                               $(LOCAL_PATH)/../platform/include \
                               $(LOCAL_PATH)/../utils/include \
                               $(LOCAL_PATH)/../include \
                               $(LOCAL_PATH)/.. \
                               $(LOCAL_PATH)/../module_config/include

LOCAL_EXPORT_C_INCLUDE_DIRS   := $(LOCAL_PATH)/include

LOCAL_CFLAGS := -Wall -Wextra -Werror -Wthread-safety

LOCAL_SRC_FILES := \
    AudioExtension.cpp \
    auto_hal.cpp

LOCAL_HEADER_LIBRARIES :=  \
    libaudioclient_headers \
    libmedia_helper_headers \
    libexpectedutils_headers \
    libxsdc-utils \
    libaudioeffects \
    liberror_headers \
    libaudio_system_headers \
    libarpal_headers

LOCAL_SHARED_LIBRARIES := \
    libaudioaidlcommon \
    libbase \
    libbinder_ndk \
    libcutils \
    libfmq \
    liblog \
    libmedia_helper \
    libstagefright_foundation \
    libutils \
    libxml2 \
    android.hardware.common-V2-ndk \
    android.hardware.common.fmq-V1-ndk \
    $(LATEST_ANDROID_MEDIA_AUDIO_COMMON_TYPES) \
    $(LATEST_ANDROID_HARDWARE_AUDIO_CORE) \
    qti-audio-types-aidl-V1-ndk \
    libar-pal

include $(BUILD_STATIC_LIBRARY)

#-------------------------------------------
#              Build HFP LIB
#-------------------------------------------
include $(CLEAR_VARS)

LOCAL_MODULE := libhfp_pal
LOCAL_VENDOR_MODULE := true

ifeq ($(TARGET_BOARD_AUTO),true)
  LOCAL_CFLAGS += -DPLATFORM_AUTO
endif

LOCAL_SRC_FILES:= Hfp.cpp

LOCAL_CFLAGS += \
    -Wall \
    -Werror \
    -Wno-unused-function \
    -Wno-unused-variable

LOCAL_CPPFLAGS += -fexceptions

LOCAL_SHARED_LIBRARIES := \
    libaudioroute \
    libbase \
    liblog \
    libaudioutils \
    libcutils \
    libdl \
    libexpat \
    liblog \
    libar-pal

LOCAL_C_INCLUDES := \
    $(TOP)/vendor/qcom/opensource/pal \
    $(TOP)/vendor/qcom/opensource/audio-hal/primary-hal/hal \
    $(TOP)/external/expat/lib \
    $(TOP)/system/media/audio_utils/include \
    $(call include-path-for, audio-route) \

LOCAL_HEADER_LIBRARIES += libhardware_headers
LOCAL_HEADER_LIBRARIES += libsystem_headers
include $(BUILD_SHARED_LIBRARY)

#-------------------------------------------
#            Build FM LIB
#-------------------------------------------
include $(CLEAR_VARS)

LOCAL_MODULE := libfmpal
LOCAL_VENDOR_MODULE := true

LOCAL_SRC_FILES:= FM.cpp

LOCAL_CFLAGS += \
    -Wall \
    -Werror \
    -Wno-unused-function \
    -Wno-unused-variable

LOCAL_SHARED_LIBRARIES := \
    libaudioroute \
    libbase \
    liblog \
    libaudioutils \
    libcutils \
    libdl \
    libexpat \
    liblog \
    libar-pal

LOCAL_C_INCLUDES := \
    $(TOP)/vendor/qcom/opensource/pal \
    $(TOP)/vendor/qcom/opensource/audio-hal/primary-hal/hal \
    $(TOP)/vendor/qcom/opensource/audio-hal/primary-hal/hal/core/extensions/include \
    $(TOP)/external/expat/lib \
    $(TOP)/system/media/audio_utils/include \
    $(call include-path-for, audio-route) \

LOCAL_HEADER_LIBRARIES += libhardware_headers
LOCAL_HEADER_LIBRARIES += libsystem_headers
include $(BUILD_SHARED_LIBRARY)

#-------------------------------------------
#            Build Power_Policy_Client LIB
#-------------------------------------------
ifeq ($(strip $(AUDIO_FEATURE_ENABLED_POWER_POLICY)),true)

include $(CLEAR_VARS)

LOCAL_MODULE := libarpowerpolicy

LOCAL_VENDOR_MODULE := true

LOCAL_SRC_FILES:= \
        PowerPolicyClient.cpp \
        power_policy_launcher.cpp

LOCAL_C_INCLUDES := \
        $(LOCAL_PATH)/.. \
        system/media/audio/include \
        $(LOCAL_PATH)/../include

LOCAL_SHARED_LIBRARIES:= \
        android.frameworks.automotive.powerpolicy-V2-ndk \
        libbase \
        libbinder_ndk \
        libcutils \
        liblog \
        libpowerpolicyclient

LOCAL_C_INCLUDES += $(TOP)/packages/services/Car/cpp/powerpolicy/client/include
LOCAL_C_INCLUDES += $(TOP)/system/libbase/include
LOCAL_C_INCLUDES += $(TOP)/external/fmtlib/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)../../..
LOCAL_C_INCLUDES += $(TOP)/vendor/qcom/opensource/audio-hal/primary-hal/hal/core/include
LOCAL_SHARED_LIBRARIES += android.frameworks.automotive.powerpolicy-V1-ndk

include $(BUILD_SHARED_LIBRARY)
endif

#-------------------------------------------
#            Build AUTO HAL LIB
#-------------------------------------------
include $(CLEAR_VARS)

LOCAL_MODULE := libautohal_pal
LOCAL_VENDOR_MODULE := true

ifeq ($(TARGET_BOARD_AUTO),true)
   LOCAL_CFLAGS += -DPLATFORM_AUTO
endif

LOCAL_SRC_FILES:= auto_hal.cpp

LOCAL_CFLAGS += \
    -Wall \
    -Werror \
    -Wno-unused-function \
    -Wno-unused-variable

LOCAL_SHARED_LIBRARIES := \
    libaudioaidlcommon \
    libaudioroute \
    libaudioutils \
    libcutils \
    libdl \
    libexpat \
    liblog \
    libar-pal \
    qti-audio-types-aidl-V1-ndk \
    $(LATEST_ANDROID_MEDIA_AUDIO_COMMON_TYPES) \
    libbase \
    libbinder_ndk \
    libfmq \
    libmedia_helper \
    libutils \
    libxml2 \
    $(LATEST_ANDROID_HARDWARE_AUDIO_COMMON) \
    $(LATEST_ANDROID_HARDWARE_COMMON_FMQ) \
    $(LATEST_ANDROID_HARDWARE_AUDIO_CORE) \
    android.media.audio.common.types-V3-ndk \
    android.hardware.audio.core-V2-ndk

LOCAL_C_INCLUDES := \
    vendor/qcom/opensource/pal \
    $(LOCAL_PATH)/../include \
    $(LOCAL_PATH)/../../hal/core/extensions/ \
    external/expat/lib \
    system/media/audio_utils/include \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/../module_config/include \
    $(LOCAL_PATH)/../platform/include \
    $(LOCAL_PATH)/../utils/include \
    $(LOCAL_PATH)/.. \
    $(call include-path-for, audio-route) \

LOCAL_HEADER_LIBRARIES += libhardware_headers
LOCAL_HEADER_LIBRARIES += libsystem_headers
include $(BUILD_SHARED_LIBRARY)

# BATTERY_LISTENER not required for automotive
ifneq ($(TARGET_BOARD_AUTO),true)
#-------------------------------------------
#            Build BATTERY_LISTENER
#-------------------------------------------
include $(CLEAR_VARS)

LOCAL_MODULE := libbatterylistener
LOCAL_VENDOR_MODULE := true

LOCAL_SRC_FILES:= battery_listener.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include

LOCAL_CFLAGS := \
    -Wall \
    -Werror \
    -Wno-unused-function \
    -Wno-unused-variable

LOCAL_SHARED_LIBRARIES := \
    android.hardware.health@1.0 \
    android.hardware.health@2.0 \
    android.hardware.health@2.1 \
    android.hardware.power@1.2 \
    android.hardware.health-V1-ndk \
    libbinder_ndk \
    libaudioutils \
    libbase \
    libcutils \
    libdl \
    libhidlbase \
    liblog \
    libutils \

LOCAL_STATIC_LIBRARIES := libhealthhalutils

include $(BUILD_SHARED_LIBRARY)
endif
