LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE            := aidl_fuzzer_audio_core_hal
LOCAL_VENDOR_MODULE     := true

LOCAL_C_INCLUDES    :=  $(LOCAL_PATH)/../include \
                        $(TOP)/system/media/audio/include \
                        $(TOP)/hardware/libhardware/include

LOCAL_CFLAGS := -DBACKEND_NDK

LOCAL_SRC_FILES := \
    fuzzer.cpp \

LOCAL_HEADER_LIBRARIES :=  \
    libxsdc-utils \
    libaudioeffects \
    liberror_headers \
    libaudioclient_headers \
    libaudio_system_headers \
    libmedia_helper_headers

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
    $(LATEST_ANDROID_HARDWARE_AUDIO_CORE_SOUNDDOSE) \
    libar-pal \
    libaudioserviceexampleimpl \
    libaudioplatformconverter.qti \
    qti-audio-types-aidl-V1-ndk \
    libbinder \
    libaudiocorehal.qti \
    libaudiocorehal.default \
    libclang_rt.ubsan_standalone \
    libAudioCalibOem

LOCAL_STATIC_LIBRARIES += libbinder_random_parcel

include $(BUILD_FUZZ_TEST)