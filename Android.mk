
ifeq ($(TARGET_BOARD_PLATFORM), monaco)
MY_LOCAL_PATH := $(call my-dir)

include $(MY_LOCAL_PATH)/hal/Android.mk

endif
