LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := device/qcom/common/power
LOCAL_CFLAGS := -DMPCTLV3
LOCAL_SRC_FILES := power-msm8998.c utils_ext.c
LOCAL_MODULE := libpower_msm8998
include $(BUILD_STATIC_LIBRARY) 