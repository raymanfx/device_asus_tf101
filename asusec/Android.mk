LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
        $(call all-subdir-java-files)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= com.cyanogenmod.asusec
include $(BUILD_JAVA_LIBRARY)

include $(CLEAR_VARS)
include $(call all-makefiles-under,$(LOCAL_PATH))
