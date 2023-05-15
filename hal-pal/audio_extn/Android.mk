LOCAL_PATH := $(call my-dir)

#-------------------------------------------
#            Build HFP LIB
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

LOCAL_SHARED_LIBRARIES := \
    libaudioroute \
    libaudioutils \
    libcutils \
    libdl \
    libexpat \
    liblog \
    libar-pal

LOCAL_C_INCLUDES := \
    vendor/qcom/opensource/pal \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../../hal/audio_extn/ \
    external/expat/lib \
    system/media/audio_utils/include \
    $(call include-path-for, audio-route) \

LOCAL_HEADER_LIBRARIES += libhardware_headers
LOCAL_HEADER_LIBRARIES += libsystem_headers
include $(BUILD_SHARED_LIBRARY)

#-------------------------------------------
#            Build HFP AG LIB
#-------------------------------------------
include $(CLEAR_VARS)

LOCAL_MODULE := libhfp_ag_pal
LOCAL_VENDOR_MODULE := true

ifeq ($(TARGET_BOARD_AUTO),true)
  LOCAL_CFLAGS += -DPLATFORM_AUTO
endif

LOCAL_SRC_FILES:= HfpAG.cpp

LOCAL_CFLAGS += \
    -Wall \
    -Werror \
    -Wno-unused-function \
    -Wno-unused-variable

LOCAL_SHARED_LIBRARIES := \
    libaudioroute \
    libaudioutils \
    libcutils \
    libdl \
    libexpat \
    liblog \
    libar-pal

LOCAL_C_INCLUDES := \
    vendor/qcom/opensource/pal \
    $(LOCAL_PATH)/.. \
    #$(LOCAL_PATH)/../../hal/audio_extn/ \
    external/expat/lib \
    system/media/audio_utils/include \
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
    libaudioutils \
    libcutils \
    libdl \
    libexpat \
    liblog \
    libar-pal

LOCAL_C_INCLUDES := \
    vendor/qcom/opensource/pal \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../../hal/audio_extn/ \
    external/expat/lib \
    system/media/audio_utils/include \
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

LOCAL_CFLAGS := \
    -Wall \
    -Werror \
    -Wno-unused-function \
    -Wno-unused-variable

LOCAL_SHARED_LIBRARIES := \
    android.hardware.health@1.0 \
    android.hardware.health@2.0 \
    android.hardware.power@1.2 \
    libaudioutils \
    libbase \
    libcutils \
    libdl \
    libhidlbase \
    liblog \
    libutils \

LOCAL_STATIC_LIBRARIES := libhealthhalutils
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
    libaudioroute \
    libaudioutils \
    libcutils \
    libdl \
    libexpat \
    liblog \
    libar-pal

LOCAL_C_INCLUDES := \
    vendor/qcom/opensource/pal \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../../hal/audio_extn/ \
    external/expat/lib \
    system/media/audio_utils/include \
    $(call include-path-for, audio-route) \

LOCAL_HEADER_LIBRARIES += libhardware_headers
LOCAL_HEADER_LIBRARIES += libsystem_headers

include $(BUILD_SHARED_LIBRARY)

#-------------------------------------------
#            Build ICC LIB
#-------------------------------------------
include $(CLEAR_VARS)

LOCAL_MODULE := libicc_pal
LOCAL_VENDOR_MODULE := true

ifeq ($(TARGET_BOARD_AUTO),true)
  LOCAL_CFLAGS += -DPLATFORM_AUTO
endif

LOCAL_SRC_FILES:= Icc.cpp

LOCAL_CFLAGS += \
    -Wall \
    -Werror \
    -Wno-unused-function \
    -Wno-unused-variable

LOCAL_SHARED_LIBRARIES := \
    libaudioroute \
    libaudioutils \
    libcutils \
    libdl \
    libexpat \
    liblog \
    libar-pal

LOCAL_C_INCLUDES := \
    vendor/qcom/opensource/pal \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../../hal/audio_extn/ \
    external/expat/lib \
    system/media/audio_utils/include \
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

LOCAL_C_INCLUDES:= \
        $(LOCAL_PATH)/.. \
        system/media/audio/include

LOCAL_SHARED_LIBRARIES:= \
        libbase \
        libbinder_ndk \
        libcutils \
        liblog \
        libpowerpolicyclient

# *-ndk_platform migrated to *-ndk from Android T onwards
ifeq ($(call math_gt_or_eq, $(PLATFORM_SDK_VERSION), 33), true)
    LOCAL_SHARED_LIBRARIES += android.frameworks.automotive.powerpolicy-V1-ndk
else
    LOCAL_SHARED_LIBRARIES += android.frameworks.automotive.powerpolicy-V1-ndk_platform
endif

include $(BUILD_SHARED_LIBRARY)
endif
