TARGET_BOARD_PLATFORM := th1520

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

ifeq ($(TARGET_PREBUILT_KERNEL_MODULES), )
KERNEL_MODULE_DIR := device/thead/th1520-kernel
else
KERNEL_MODULE_DIR := $(TARGET_PREBUILT_KERNEL_MODULES)
endif

BOARD_VENDOR_KERNEL_MODULES := $(wildcard $(KERNEL_MODULE_DIR)/*.ko)
