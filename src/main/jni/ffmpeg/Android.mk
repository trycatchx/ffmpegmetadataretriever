LOCAL_PATH := $(call my-dir)


TARGET_ARCH_ABI := armv7-a


include $(CLEAR_VARS)
LOCAL_MODULE := libavutil
LOCAL_SRC_FILES := ffmpeg/$(TARGET_ARCH_ABI)/lib/libavutil.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/ffmpeg/$(TARGET_ARCH_ABI)/include
include $(PREBUILT_SHARED_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := libavcodec
LOCAL_SRC_FILES := ffmpeg/$(TARGET_ARCH_ABI)/lib/libavcodec.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/ffmpeg/$(TARGET_ARCH_ABI)/include
include $(PREBUILT_SHARED_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := libavformat
LOCAL_SRC_FILES := ffmpeg/$(TARGET_ARCH_ABI)/lib/libavformat.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/ffmpeg/$(TARGET_ARCH_ABI)/include
include $(PREBUILT_SHARED_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := libswscale
LOCAL_SRC_FILES := ffmpeg/$(TARGET_ARCH_ABI)/lib/libswscale.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/ffmpeg/$(TARGET_ARCH_ABI)/include
include $(PREBUILT_SHARED_LIBRARY)




LOCAL_PATH:= $(call my-dir)
