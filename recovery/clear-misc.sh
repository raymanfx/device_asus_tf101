#!/sbin/sh
# This clears the boot flag from misc which forces
# this device to continuously boot into recovery
/sbin/dd if=/dev/zero of=/dev/block/mmcblk0p3 bs=1 count=13
