LOCAL_PATH := $(call my-dir)

# Set GSTREAMER_ROOT to the architecture-specific GStreamer directory
# GSTREAMER_ROOT_ANDROID is passed from gradle as the parent directory
ifndef GSTREAMER_ROOT_ANDROID
    $(error GSTREAMER_ROOT_ANDROID is not set!)
endif

GSTREAMER_ROOT := $(GSTREAMER_ROOT_ANDROID)/$(TARGET_ARCH_ABI)

# First include GStreamer ndk-build makefile (defines gstreamer_android module)
include $(GSTREAMER_ROOT)/../share/gst-android/ndk-build/gstreamer-1.0.mk

# Then build our smartplayer module
include $(CLEAR_VARS)

LOCAL_MODULE := smartplayer

LOCAL_SRC_FILES := \
    src/main/cpp/smart_player_jni.c \
    src/main/cpp/smart_player_core.c

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/src/main/cpp \
    $(GSTREAMER_ROOT)/include/gstreamer-1.0 \
    $(GSTREAMER_ROOT)/include/glib-2.0 \
    $(GSTREAMER_ROOT)/lib/glib-2.0/include

LOCAL_CFLAGS := -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable -O2 -Wno-shift-count-overflow

LOCAL_SHARED_LIBRARIES := gstreamer_android

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/src/main/cpp

include $(BUILD_SHARED_LIBRARY)
