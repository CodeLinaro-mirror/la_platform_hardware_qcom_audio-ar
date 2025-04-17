LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE            := libaudiocore.extension
LOCAL_VENDOR_MODULE     := true

LOCAL_C_INCLUDES            := $(LOCAL_PATH)/.. \
                               $(LOCAL_PATH)/include \
                               $(LOCAL_PATH)/../platform/include \
                               $(LOCAL_PATH)/../include \
                               $(LOCAL_PATH)/../module_config/include \
                               $(LOCAL_PATH)/../utils/include

LOCAL_EXPORT_C_INCLUDE_DIRS   := $(LOCAL_PATH)/include

LOCAL_CFLAGS := -Wall -Wextra -Werror -Wthread-safety

LOCAL_SRC_FILES := \
    AudioExtension.cpp \
    auto_hal.cpp

ifeq ($(ENABLE_QCOM_AUDIO_DIAGNOSTICS),true)
LOCAL_CFLAGS += -DENABLE_AUDIO_DIAGNOSTICS
endif

ifeq ($(AUDIO_FEATURE_ENABLED_ECNR_HAL),true)
LOCAL_CFLAGS += -DECNR_HAL_ENABLE
LOCAL_SRC_FILES += hal_ecnr.cpp

ifneq (,$(filter userdebug eng,$(TARGET_BUILD_VARIANT)))
LOCAL_SRC_FILES += hal_ecnr_tune.cpp
LOCAL_CPPFLAGS += -DECNR_HAL_TUNE
LOCAL_CPPFLAGS += -DECNR_HAL_DUMP_ENABLE
endif

endif

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

ifeq ($(ENABLE_QCOM_HAL_AUDIO_FOCUS),true)
LOCAL_CFLAGS += -DENABLE_QCOM_HAL_AUDIO_FOCUS
LOCAL_SHARED_LIBRARIES += \
    android.hardware.automotive.audiocontrol-V4-ndk \
    alliance.hardware.automotive.audiocontrol.internal-V2-ndk \
    ampere.hardware.interfaces.automotive.audioparameterparser-V1-ndk \
    libexpat
endif

include $(BUILD_STATIC_LIBRARY)

ifneq ($(AUDIO_FEATURE_ENABLED_ECNR_HAL),true)
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

else
#-------------------------------------------
#            Build HFP Sse LIB
#-------------------------------------------

include $(CLEAR_VARS)

LOCAL_MODULE := libhfp_pal
LOCAL_VENDOR_MODULE := true

ifeq ($(TARGET_BOARD_AUTO),true)
  LOCAL_CFLAGS += -DPLATFORM_AUTO
endif

LOCAL_CFLAGS += -DECNR_HAL_ENABLE

LOCAL_SRC_FILES:= HfpECNR.cpp
LOCAL_SRC_FILES += hal_ecnr.cpp

ifneq (,$(filter userdebug eng,$(TARGET_BUILD_VARIANT)))
LOCAL_SRC_FILES += hal_ecnr_tune.cpp
LOCAL_CPPFLAGS += -DECNR_HAL_TUNE
LOCAL_CPPFLAGS += -DECNR_HAL_DUMP_ENABLE
endif

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
    $(LOCAL_PATH)/include \
    $(TOP)/vendor/qcom/opensource/pal \
    $(TOP)/external/expat/lib \
    $(TOP)/system/media/audio_utils/include \
    $(call include-path-for, audio-route) \

LOCAL_HEADER_LIBRARIES += libhardware_headers
LOCAL_HEADER_LIBRARIES += libsystem_headers
include $(BUILD_SHARED_LIBRARY)
endif

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
    $(LATEST_ANDROID_HARDWARE_AUDIO_CORE)


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



#-------------------------------------------
#            Build CONFIG LIB
#-------------------------------------------
include $(CLEAR_VARS)

LOCAL_MODULE := libAudioConfigOem
LOCAL_VENDOR_MODULE := true

LOCAL_SRC_FILES:= AudioConfig.cpp

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
#            Build Auto OEM extension
#-------------------------------------------
include $(CLEAR_VARS)

LOCAL_MODULE := libautooemextension
LOCAL_VENDOR_MODULE := true

LOCAL_SRC_FILES:= auto_oem_extension.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include

LOCAL_CFLAGS := \
    -Wall \
    -Werror \
    -Wno-unused-function \
    -Wno-unused-variable \
    -Wno-missing-field-initializers \
    -Wunused-parameter \
    -Wextra \


LOCAL_C_INCLUDES := \
    $(TOP)/vendor/qcom/opensource/pal \
    $(TOP)/vendor/qcom/opensource/audio-hal-ar/primary-hal/hal \
    $(TOP)/vendor/qcom/opensource/audio-hal-ar/primary-hal/hal/core/extensions/include \
    $(TOP)/external/expat/lib \
    $(TOP)/system/media/audio_utils/include \
    $(call include-path-for, audio-route) \

LOCAL_SHARED_LIBRARIES := \
    libbinder_ndk \
    libaudioutils \
    libbase \
    libcutils \
    libdl \
    libhidlbase \
    liblog \
    libutils \
    libar-pal \
    libvhalclient \
    libAudioConfigOem \
    libAWXPAL


LOCAL_HEADER_LIBRARIES :=  \
    libaudio_system_headers \
    libsystem_headers \
    libarpal_headers \

LOCAL_STATIC_LIBRARIES := \
    VehicleHalUtils \
    android-automotive-large-parcelable-lib \
    android.hardware.automotive.vehicle@2.0 \
    libmath \
    android.hardware.automotive.vehicle-V3-ndk \
    android.hardware.automotive.vehicle.property-V3-ndk \

include $(BUILD_SHARED_LIBRARY)
#-------------------------------------------

ifeq ($(ENABLE_QCOM_HAL_AUDIO_FOCUS),true)
#-------------------------------------------------
#            Build HAL Priority extension
#-------------------------------------------------
include $(CLEAR_VARS)

LOCAL_MODULE := libaudiohalpriorityextn
LOCAL_VENDOR_MODULE := true

LOCAL_SRC_FILES:= AudioHalFocusManager.cpp \
                  BusDuckConfig.cpp

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include

LOCAL_CFLAGS := \
    -Wall \
    -Werror \
    -Wno-unused-function \
    -Wno-unused-variable \
    -Wno-missing-field-initializers \
    -Wunused-parameter \
    -Wextra


LOCAL_C_INCLUDES := \
    $(TOP)/vendor/qcom/opensource/audio-hal/primary-hal/hal/core/extensions/include \
    $(TOP)/system/media/audio_utils/include

LOCAL_SHARED_LIBRARIES := \
    libbinder_ndk \
    libaudioutils \
    libbase \
    libcutils \
    liblog \
    libutils \
    libexpat \
    $(LATEST_ANDROID_MEDIA_AUDIO_COMMON_TYPES) \
    $(LATEST_ANDROID_HARDWARE_AUDIO_CORE) \
    alliance.hardware.automotive.audiocontrol.internal-V2-ndk \
    android.hardware.automotive.audiocontrol-V4-ndk \
    ampere.hardware.interfaces.automotive.audioparameterparser-V1-ndk \
    libAudioConfigOem \
    libAWXPAL


LOCAL_HEADER_LIBRARIES :=  \
    libaudio_system_headers \
    libsystem_headers \
    libarpal_headers

include $(BUILD_SHARED_LIBRARY)
#-------------------------------------------------
endif

#-------------------------------------------
#            Build CONFIG LIB
#-------------------------------------------
include $(CLEAR_VARS)

LOCAL_MODULE := libAWXPAL
LOCAL_VENDOR_MODULE := true

LOCAL_SRC_FILES:= PalParamDelegator.cpp

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
    liblog \
    libar-pal

LOCAL_C_INCLUDES := \
    $(TOP)/vendor/qcom/opensource/pal \
    $(TOP)/vendor/qcom/opensource/audio-hal-ar/primary-hal/hal/core/extensions/include/extensions \
    $(call include-path-for, audio-route) \

LOCAL_HEADER_LIBRARIES += libhardware_headers
LOCAL_HEADER_LIBRARIES += libsystem_headers

include $(BUILD_SHARED_LIBRARY)


ifeq ($(ENABLE_QCOM_AUDIO_DIAGNOSTICS),true)
#-------------------------------------------------
#            Build Audio Diagnostic extension
#-------------------------------------------------
include $(CLEAR_VARS)

LOCAL_MODULE := libaudiodiagnostics
LOCAL_VENDOR_MODULE := true

LOCAL_SRC_FILES:= AudioDiagnostics.cpp

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include

LOCAL_CFLAGS := \
    -Wall \
    -Werror \
    -Wno-unused-function \
    -Wno-unused-variable \
    -Wno-missing-field-initializers \
    -Wunused-parameter \
    -Wextra \


LOCAL_C_INCLUDES := \
    $(TOP)/vendor/qcom/opensource/audio-hal/primary-hal/hal/core/extensions/include \
    $(TOP)/system/media/audio_utils/include \


LOCAL_SHARED_LIBRARIES := \
    libbinder_ndk \
    libaudioutils \
    libbase \
    libcutils \
    liblog \
    libutils \
    libexpat \
    $(LATEST_ANDROID_MEDIA_AUDIO_COMMON_TYPES) \
    $(LATEST_ANDROID_HARDWARE_AUDIO_CORE) \
    libvhalclient

LOCAL_STATIC_LIBRARIES := \
    libmath \
    android.hardware.automotive.vehicle-V4-ndk \
    android.hardware.automotive.vehicle@2.0 \
    VehicleHalUtils \
    android-automotive-large-parcelable-lib \
    android.hardware.automotive.vehicle.property-V3-ndk


LOCAL_HEADER_LIBRARIES :=  \
    libaudio_system_headers \
    libsystem_headers \
    libarpal_headers

include $(BUILD_SHARED_LIBRARY)
#-------------------------------------------------
endif