ifneq ($(AUDIO_USE_STUB_HAL), true)
LOCAL_PATH := $(call my-dir)
CURRENT_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_CFLAGS   += -fstack-protector-strong -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2

LOCAL_MODULE            := libaudiocorehal.qti
LOCAL_VENDOR_MODULE     := true
LOCAL_MODULE_RELATIVE_PATH := hw

LOCAL_C_INCLUDES    :=  $(LOCAL_PATH)/include\
                        $(LOCAL_PATH)/extensions/include

LOCAL_CFLAGS := \
    -DBACKEND_NDK \
    -Wall \
    -Wextra \
    -Werror \
    -Wthread-safety

LOCAL_CFLAGS += -Wno-writable-strings

LOCAL_VINTF_FRAGMENTS   := \
    ../../configs/common/manifest_non_qmaa.xml

ifneq (,$(filter 17 CinnamonBun,$(PLATFORM_VERSION)))
    LOCAL_VINTF_FRAGMENTS += manifest_audiocorehal_qti_17.xml
endif

LOCAL_SRC_FILES := \
    CoreService.cpp \
    Bluetooth.cpp \
    Module.cpp \
    ModulePrimary.cpp \
    ModuleStub.cpp \
    SoundDose.cpp \
    StreamWorker.cpp \
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

#Enable Hardware timestamp for Android U, V and in Android W only for Nord Gen5
ifneq (,$(filter U UpsideDownCake 14 V VanillaIceCream 15, $(PLATFORM_VERSION)))
    LOCAL_CPPFLAGS += -DHARDWARE_TIMESTAMP
else ifneq (,$(filter W Baklava 16 CinnamonBun 17, $(PLATFORM_VERSION)))
    ifneq (,$(filter $(TARGET_BOARD_PLATFORM)$(TARGET_BOARD_SUFFIX)$(TARGET_BOARD_DERIVATIVE_SUFFIX), gen4_gvm_gy gen4_gvm_gy_sgt gen5_gvm gen5_gvm_cmu gen5_gvm_gy))
    LOCAL_CPPFLAGS += -DHARDWARE_TIMESTAMP
    endif
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
    libbase \
    libbinder_ndk \
    libcutils \
    liblog \
    libdl \
    libhidlbase \
    libhardware \
    libhfp_pal \
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
    libaudioplatformconverter.qti \
    qti-audio-types-aidl-V1-ndk

include $(BUILD_SHARED_LIBRARY)

include $(CURRENT_PATH)/fuzzer/Android.mk
include $(CURRENT_PATH)/extensions/Android.mk
include $(CURRENT_PATH)/platform/Android.mk
include $(CURRENT_PATH)/utils/Android.mk
endif
