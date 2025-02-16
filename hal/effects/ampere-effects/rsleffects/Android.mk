LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE:= libampereeffects
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := soundfx
LOCAL_MODULE_OWNER := qti

LOCAL_C_FLAGS += -Werror -Wall -Wextr -O0

LOCAL_SRC_FILES:= \
        RslAidl.cpp \
        RslContext.cpp \
        AmbianceContext.cpp \
        SDVCContext.cpp \
        SteadyVolumeContext.cpp \
        BMTContext.cpp \
        BassBoostContext.cpp

LOCAL_STATIC_LIBRARIES := libaudioeffecthal_base_impl_static \
                          libaudiocore.extension

LOCAL_C_INCLUDES := \
    $(TOP)/vendor/qcom/opensource/audio-hal-ar/primary-hal/hal/core/extensions/include

LOCAL_SHARED_LIBRARIES:= \
    $(EFFECTS_DEFAULTS_SHARED_LIBRARIES) \
    libar-pal \
    liblog \
    libaudiocorehal.qti

LOCAL_HEADER_LIBRARIES:= \
    $(EFFECTS_DEFAULTS_HEADERS_LIBRARIES) \
    libacdb_headers \
    libarpal_headers \
    libaudioeffects \
    libcutils_headers

include $(BUILD_SHARED_LIBRARY)
