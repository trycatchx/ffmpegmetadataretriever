ROOT_PATH:= $(call my-dir)


include $(ROOT_PATH)/ffmpeg/Android.mk

include $(ROOT_PATH)/metadata/Android.mk

LOCAL_CFLAGS := -D__STDC_CONSTANT_MACROS -Wl,-Map=test.map -g
