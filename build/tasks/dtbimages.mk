# Use this file to generate dtb.img and dtbo.img instead of using
# BOARD_PREBUILT_DTBIMAGE_DIR. We need to keep dtb and dtbo files at the fixed
# positions in images, so that bootloader can rely on their indexes in the
# image. As dtbo.img must be signed with AVB tool, we generate intermediate
# dtbo.img, and the resulting $(PRODUCT_OUT)/dtbo.img will be created with
# Android build system, by exploiting BOARD_PREBUILT_DTBOIMAGE variable.

ifneq ($(filter beaglev_ahead evb_light lichee_pi_4a light_b , $(TARGET_DEVICE)),)

PRODUCT_OUT ?= out/target/product/evb_light
KERNEL_OUT ?= $(PRODUCT_OUT)/obj/KERNEL_OBJ
KERNEL_DEVICE_TREE ?= light-a-val-android
LOCAL_DTB ?= $(KERNEL_OUT)/arch/riscv/boot/dts/thead/$(KERNEL_DEVICE_TREE).dtb

MKDTIMG := prebuilts/misc/linux-x86/libufdt/mkdtimg
DTBIMAGE := $(PRODUCT_OUT)/dtb.img
DTBOIMAGE := $(PRODUCT_OUT)/$(DTBO_UNSIGNED)

DTB_FILES := $(LOCAL_DTB)
DTBO_FILES := $(LOCAL_DTBO)

$(warning DTB_FILES:$(DTB_FILES) DTBO_FILES:$(DTBO_FILES))

$(DTBIMAGE): $(DTB_FILES)
	cat $^ > $@

$(DTBOIMAGE): $(DTBO_FILES)
	$(MKDTIMG) create $@ $^

include $(CLEAR_VARS)
LOCAL_MODULE := dtbimage
LOCAL_LICENSE_KINDS := legacy_notice
LOCAL_LICENSE_CONDITIONS := notice
LOCAL_ADDITIONAL_DEPENDENCIES := $(DTBIMAGE)
include $(BUILD_PHONY_PACKAGE)

include $(CLEAR_VARS)
LOCAL_MODULE := dtboimage
LOCAL_LICENSE_KINDS := legacy_notice
LOCAL_LICENSE_CONDITIONS := notice
LOCAL_ADDITIONAL_DEPENDENCIES := $(DTBOIMAGE)
include $(BUILD_PHONY_PACKAGE)

droidcore: dtbimage dtboimage

endif
