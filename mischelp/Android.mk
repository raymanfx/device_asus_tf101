LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES := \
    libutils

LOCAL_MODULE_TAGS := eng
LOCAL_SRC_FILES := mischelp.c
LOCAL_CFLAGS += -I. -Ishared -Wall -ggdb
LOCAL_MODULE := mischelp
LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES += frameworks/base/include
LOCAL_C_INCLUDES += system/core/include
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SHARED_LIBRARIES := \
    libutils

LOCAL_SRC_FILES := mischelp.c
LOCAL_MODULE := mischelp
LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES += frameworks/base/include
LOCAL_C_INCLUDES += system/core/include
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := mischelp.c
LOCAL_MODULE := mischelp
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := UTILITY_EXECUTABLES
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/recovery/root/sbin
LOCAL_UNSTRIPPED_PATH := $(PRODUCT_OUT)/symbols/utilities
LOCAL_MODULE_STEM := mischelp
LOCAL_STATIC_LIBRARIES := libc libcutils
LOCAL_FORCE_STATIC_EXECUTABLE := true
include $(BUILD_EXECUTABLE)



