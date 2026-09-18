TARGET_BOARD_PLATFORM := th1520

# The LPi4A mainline kernel cannot delegate FUSE backing-file registration to
# the unprivileged MediaProvider. Use normal FUSE until that ABI is supported.
# A system_ext init action also migrates the inherited persistent true value.
ifeq ($(TARGET_PRODUCT),lichee_pi_4a)
PRODUCT_COPY_FILES += \
    device/thead/th1520/init.lpi4a-fuse.rc:$(TARGET_COPY_OUT_SYSTEM_EXT)/etc/init/init.lpi4a-fuse.rc
endif

$(call inherit-product, vendor/thead/proprietary/config/chipset/th1520/th1520.mk)

include device/thead/th1520/subdevs/graphics/graphics.mk
include device/thead/th1520/subdevs/security/security.mk
include device/thead/th1520/subdevs/updater/updater.mk
include device/thead/th1520/subdevs/performance/performance.mk
include device/thead/th1520/subdevs/audio/audio.mk
include device/thead/th1520/subdevs/camera/camera.mk
include device/thead/th1520/subdevs/media/media.mk
include device/thead/th1520/subdevs/connectivity/connectivity.mk

PRODUCT_SHIPPING_API_LEVEL := 31
PRODUCT_CHARACTERISTICS := tablet

# FIXME API_LEVEL 32 required manifest fcm verison ge 6
# but now is 3
DEVICE_MANIFEST_FILE += \
    device/thead/th1520/manifest.xml

# Android 17 no longer installs the frozen Android 10 framework matrix, but
# this board's vendor manifest still correctly declares FCM level 4. Install
# the restored matrix on system_ext through the Android.bp module.
PRODUCT_PACKAGES += \
    th1520_framework_compatibility_matrix.4.xml

PRODUCT_OTA_ENFORCE_VINTF_KERNEL_REQUIREMENTS := false

# Enable userspace reboot
$(call inherit-product, $(SRC_TARGET_DIR)/product/userspace_reboot.mk)

# Enable project quotas and casefolding for emulated storage without sdcardfs
$(call inherit-product, $(SRC_TARGET_DIR)/product/emulated_storage.mk)

# setup dalvik vm configs
$(call inherit-product, frameworks/native/build/phone-xhdpi-4096-dalvik-heap.mk)

# Enable Virtual A/B
# $(call inherit-product, $(SRC_TARGET_DIR)/product/virtual_ab_ota/compression.mk)
# $(call inherit-product, $(SRC_TARGET_DIR)/product/virtual_ab_ota.mk)

# modetest installs into /system/bin. Modern generic_system artifact-path
# enforcement rejects device-specific additions there; keep it out of images.

# Android 17 on LPi4A uses the mainline/GKI boot chain. Falling back to the
# vendor 5.10 module directory produces a bootable image whose modules cannot
# load into that kernel, so reject an incomplete build environment immediately.
ifeq ($(TARGET_PREBUILT_KERNEL_MODULES), )
$(error TARGET_PREBUILT_KERNEL_MODULES must point to modules built with the selected LPi4A kernel)
endif

KERNEL_MODULE_DIR := $(TARGET_PREBUILT_KERNEL_MODULES)
# Do not silently build an image whose init requests an absent backlight module.
ifeq ($(wildcard $(KERNEL_MODULE_DIR)/th1520_pwm0_diag.ko),)
$(error This LPi4A DSI image requires the matching th1520_pwm0_diag.ko module)
endif
PRODUCT_PACKAGES += lpi4a_v4l2_layout_probe
# Keep the obsolete RTL8723BS driver out of the image.  The fitted
# FGN200AKSR-05 is handled by the AIC BSP/FDRV pair, and shipping both drivers
# makes an old init script capable of claiming the same SDIO functions first.
BOARD_VENDOR_KERNEL_MODULES := $(filter-out \
    $(KERNEL_MODULE_DIR)/r8723bs.ko, \
    $(wildcard $(KERNEL_MODULE_DIR)/*.ko))
