LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS   += -fstack-protector-strong -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2

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
else ifeq ($(PLATFORM_VERSION),CinnamonBun)
    # Android 17 (CinnamonBun) requires updated manifest with audio core HAL v4
    LOCAL_VINTF_FRAGMENTS += manifest_audiocorehal_default_17.xml
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

ifeq ($(PLATFORM_VERSION),CinnamonBun)
LOCAL_SHARED_LIBRARIES += \
    android.media.audio.common.types-V5-ndk \
    android.hardware.audio.core-V4-ndk
else
LOCAL_SHARED_LIBRARIES += \
    $(LATEST_ANDROID_MEDIA_AUDIO_COMMON_TYPES) \
    $(LATEST_ANDROID_HARDWARE_AUDIO_CORE)
endif

include $(BUILD_SHARED_LIBRARY)

