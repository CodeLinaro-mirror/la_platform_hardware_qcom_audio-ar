LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE            := libaudiocorehal.default
LOCAL_VENDOR_MODULE     := true
LOCAL_MODULE_RELATIVE_PATH := hw

LOCAL_CFLAGS := \
    -DBACKEND_NDK \
    -Wall \
    -Wextra \
    -Werror \
    -Wthread-safety

ifeq ($(PLATFORM_VERSION),16)
    # Android 16 (Baklava) requires updated manifest with audio core HAL v3
    LOCAL_VINTF_FRAGMENTS += manifest_audiocorehal_default_16.xml
else ifneq (,$(filter 17 CinnamonBun,$(PLATFORM_VERSION)))
    # Android 17 (CinnamonBun) requires updated manifest with audio core HAL v4
    ifeq ($(AUDIO_USE_STUB_HAL), true)
        # Stub-only mode: default.so registers IModule/default → V4
        LOCAL_VINTF_FRAGMENTS += manifest_audiocorehal_default_17.xml
    else
        # QTI mode: qti.so registers IModule/default → exclude it here
        LOCAL_VINTF_FRAGMENTS += manifest_audiocorehal_default_17_qti.xml
    endif
else
    LOCAL_VINTF_FRAGMENTS += manifest_audiocorehal_default.xml
endif

LOCAL_SRC_FILES := \
    DefaultServices.cpp

LOCAL_HEADER_LIBRARIES :=  \
    libxsdc-utils \
    liberror_headers

LOCAL_SHARED_LIBRARIES := \
    libaudioaidlcommon \
    libaudioserviceexampleimpl \
    libbase \
    libbinder_ndk \
    libcutils \
    liblog \
    libdl \
    libxml2 \
    libaudioutils \
    libutils \
    android.hardware.common-V2-ndk \
    libmedia_helper \
    libstagefright_foundation \
    libhidlbase \
    libhardware \
    libfmq

ifneq (,$(filter 17 CinnamonBun,$(PLATFORM_VERSION)))
LOCAL_SHARED_LIBRARIES += \
    android.media.audio.common.types-V5-ndk \
    android.hardware.audio.core-V4-ndk
else
LOCAL_SHARED_LIBRARIES += \
    $(LATEST_ANDROID_MEDIA_AUDIO_COMMON_TYPES) \
    $(LATEST_ANDROID_HARDWARE_AUDIO_CORE)
endif

include $(BUILD_SHARED_LIBRARY)

