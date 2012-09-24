TF_BLOBIFIER := $(HOST_OUT_EXECUTABLES)/blobpack_tf
PRODUCT_COPY_FILES += $(LOCAL_PATH)/recovery/clear-misc.sh:root/sbin/clear-misc.sh


$(INSTALLED_RECOVERYIMAGE_TARGET): $(MKBOOTIMG) \
		$(recovery_ramdisk) \
		$(TF_BLOBIFIER) \
		$(recovery_kernel)
	@echo ----- Making recovery image ------
	$(MKBOOTIMG) $(INTERNAL_RECOVERYIMAGE_ARGS) --output $@.orig
	$(TF_BLOBIFIER) $@ SOS $@.orig
	@echo ----- Made recovery image -------- $@
	$(hide) $(call assert-max-image-size,$@,$(BOARD_RECOVERYIMAGE_PARTITION_SIZE),raw)


$(INSTALLED_BOOTIMAGE_TARGET): $(MKBOOTIMG) $(INTERNAL_BOOTIMAGE_FILES) $(TF_BLOBIFIER)
	$(call pretty,"Target boot image: $@")
	$(hide) $(MKBOOTIMG) $(INTERNAL_BOOTIMAGE_ARGS) --output $@.orig
	$(TF_BLOBIFIER) $@ LNX $@.orig
	$(hide) $(call assert-max-image-size,$@,$(BOARD_BOOTIMAGE_PARTITION_SIZE),raw)


