# Copyright (C) 2011 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

$(call inherit-product, $(SRC_TARGET_DIR)/product/languages_full.mk)

# Prebuilt kernel location
ifeq ($(TARGET_PREBUILT_KERNEL),)
	LOCAL_KERNEL := $(LOCAL_PATH)/kernel
else
	LOCAL_KERNEL := $(TARGET_PREBUILT_KERNEL)
endif

# Files needed for boot image
PRODUCT_COPY_FILES += \
    $(LOCAL_KERNEL):kernel \
    $(LOCAL_PATH)/ramdisk/init.ventana.rc:root/init.ventana.rc\
    $(LOCAL_PATH)/ramdisk/init.ventana.usb.rc:root/init.ventana.usb.rc \
    $(LOCAL_PATH)/ramdisk/ueventd.ventana.rc:root/ueventd.ventana.rc \
    $(LOCAL_PATH)/ramdisk/init.ventana.keyboard.rc:root/init.ventana.keyboard.rc \
    $(LOCAL_PATH)/prebuilt/keyswap::root/sbin/keyswap \
    $(LOCAL_PATH)/ramdisk/fstab.ventana:root/fstab.ventana

# These are the hardware-specific features
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/tablet_core_hardware.xml:system/etc/permissions/tablet_core_hardware.xml \
    frameworks/native/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml \
    frameworks/native/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml

# Build characteristics setting 
PRODUCT_CHARACTERISTICS := tablet

# This device have enough room for precise dalvik
PRODUCT_TAGS += dalvik.gc.type-precise

# Extra packages to build for this device
PRODUCT_PACKAGES += \
    	librs_jni \
	com.android.future.usb.accessory \
	make_ext4fs \
	setup_fs \
	libinvensense_mpl \
        blobpack_tf \
	mischelp

# Propertys spacific for this device
PRODUCT_PROPERTY_OVERRIDES := \
    	wifi.interface=wlan0 \
    	wifi.supplicant_scan_interval=15 \
    	ro.opengles.version=131072 \
	persist.sys.usb.config=mtp,adb \
	dalvik.vm.dexopt-data-only=1 \
        ro.sf.lcd_density=160 \
        ro.wifi.country=EU \
        ro.carrier=wifi-only

# Device nameing
PRODUCT_BUILD_PROP_OVERRIDES += BUILD_UTC_DATE=0
PRODUCT_NAME := tf101
PRODUCT_DEVICE := tf101
PRODUCT_MODEL := tf101
PRODUCT_BRAND := asus
PRODUCT_MANUFACTURER := asus