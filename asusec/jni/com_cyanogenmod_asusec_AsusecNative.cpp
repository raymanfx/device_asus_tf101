#include <JNIHelp.h>
#include <android_runtime/AndroidRuntime.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define ASUSEC_DEV "/dev/asusec"

// copied from drivers/input/asusec/asusec.h
#define ASUSEC_TP_ON   1
#define ASUSEC_TP_OFF  0
#define ASUSEC_IOC_MAGIC   0xf4
#define ASUSEC_TP_CONTROL      _IOR(ASUSEC_IOC_MAGIC, 5,  int)



// ----------------------------------------------------------------------------

/*
 * Class:     com_cyanogenmod_asusec_KeyHandler
 * Method:    nativeToggleTouchpad
 * Signature: (Z)Z
 */
JNIEXPORT jboolean JNICALL Java_com_cyanogenmod_asusec_KeyHandler_nativeToggleTouchpad
  (JNIEnv *env, jclass cls, jboolean status) {
    ALOGD("Switching touchpad %d\n", status);

    int fd = open(ASUSEC_DEV, O_RDONLY | O_NONBLOCK);

    if (fd < 0) {
        ALOGE("Could  open device %s\n", ASUSEC_DEV);
        return -1;
    }

    int on = (status == 0) ? ASUSEC_TP_OFF : ASUSEC_TP_ON;
    int success = ioctl(fd, ASUSEC_TP_CONTROL, on);

    if (success != 0) {
        ALOGE("Error calling ioctl, %d\n", success);
    }

    close(fd);

    ALOGD("Touchpad is %d\n", on);
    return (jboolean) ((on == 1) ? true : false);
}


// ----------------------------------------------------------------------------

static const JNINativeMethod gMethods[] = {
        { "nativeToggleTouchpad", "(Z)Z", (void *) Java_com_cyanogenmod_asusec_KeyHandler_nativeToggleTouchpad }
};


// ----------------------------------------------------------------------------

/*
 * This is called by the VM when the shared library is first loaded.
 */
jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    JNIEnv* env = NULL;
    jint result = -1;

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        ALOGE("ERROR: GetEnv failed\n");
        goto bail;
    }
    assert(env != NULL);

    if(android::AndroidRuntime::registerNativeMethods(
            env, "com/cyanogenmod/asusec/KeyHandler", gMethods,
            sizeof(gMethods) / sizeof(gMethods[0])) != JNI_OK) {
        ALOGE("Failed to register native methods");
        goto bail;
    }

    /* success -- return valid version number */
    result = JNI_VERSION_1_4;

bail:
    return result;
}
