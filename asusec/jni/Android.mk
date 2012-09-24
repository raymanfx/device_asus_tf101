LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= libasusec_jni

# All of the source files that we will compile.
LOCAL_SRC_FILES:= \
	com_cyanogenmod_asusec_AsusecNative.cpp

# All of the shared libraries we link against.
LOCAL_SHARED_LIBRARIES := \
	libandroid_runtime \
	libnativehelper \
	libcutils \
	libutils

# Also need the JNI headers.
LOCAL_C_INCLUDES += \
	$(JNI_H_INCLUDE)

include $(BUILD_SHARED_LIBRARY)
