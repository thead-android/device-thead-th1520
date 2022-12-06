TARGET_BOARD_PLATFORM := light
PRODUCT_USE_DYNAMIC_PARTITIONS := true

include device/thead/light/subdevs/boot/boot.mk
include device/thead/light/subdevs/graphics/graphics.mk
include device/thead/light/subdevs/nna/nna.mk
include device/thead/light/subdevs/security/security.mk
include device/thead/light/subdevs/updater/updater.mk
include device/thead/light/subdevs/performance/performance.mk
include device/thead/light/subdevs/audio/audio.mk
include device/thead/light/subdevs/camera/camera.mk
include device/thead/light/subdevs/media/media.mk
include device/thead/light/subdevs/connectivity/connectivity.mk

PRODUCT_SHIPPING_API_LEVEL := 31

# FIXME API_LEVEL 32 required manifest fcm verison ge 6
# but now is 3
DEVICE_MANIFEST_FILE += \
    device/thead/light/manifest.xml

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

#debug tools included only for userdebug and eng build
ifneq (,$(filter userdebug eng, $(TARGET_BUILD_VARIANT)))
PRODUCT_PACKAGES += \
    modetest
endif

KERNEL_MODULE_DIR := device/thead/light-kernel
BOARD_VENDOR_KERNEL_MODULES := $(wildcard $(KERNEL_MODULE_DIR)/*.ko)


