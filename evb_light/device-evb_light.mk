# Kernel
ifeq ($(TARGET_PREBUILT_KERNEL),)
LOCAL_KERNEL := device/thead/light-kernel/kernel-5.10
LOCAL_DTB := device/thead/light-kernel/light-a-val-android.dtb
else
LOCAL_KERNEL := $(TARGET_PREBUILT_KERNEL)
LOCAL_DTB := $(TARGET_PREBUILT_DTB)
endif

PRODUCT_COPY_FILES := \
    $(LOCAL_KERNEL):kernel

# Install light platform common packages
$(call inherit-product, device/thead/light/light-common.mk)

# Overlay
DEVICE_PACKAGE_OVERLAYS := device/thead/light/evb_light/overlay

# evb_ligth init files
PRODUCT_COPY_FILES += \
    device/thead/light/evb_light/init.evb_light.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.evb_light.rc \
    device/thead/light/evb_light/init.common.usb.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.evb_light.usb.rc \
    device/thead/light/evb_light/init.connectivity.evb_light.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.connectivity.evb_light.rc \
    device/thead/light/evb_light/ueventd.evb_light.rc:$(TARGET_COPY_OUT_VENDOR)/etc/ueventd.rc \

# evb_light fstab files
PRODUCT_COPY_FILES += \
    device/thead/light/evb_light/fstab.evb_light:$(TARGET_COPY_OUT_VENDOR)/etc/fstab.evb_light \
    device/thead/light/evb_light/fstab.evb_light.ramdisk:$(TARGET_COPY_OUT_RAMDISK)/fstab.evb_light \
    device/thead/light/evb_light/fstab.enableswap:$(TARGET_COPY_OUT_VENDOR)/etc/fstab.enableswap \

# Recovery files
PRODUCT_COPY_FILES += \
    device/thead/light/evb_light/init.recovery.evb_light.rc:recovery/root/init.recovery.evb_light.rc \

# evb_light media configurations
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/media_codecs.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs.xml \
    frameworks/av/media/libstagefright/data/media_codecs_google_audio.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_google_audio.xml \
    frameworks/av/media/libstagefright/data/media_codecs_google_video.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_google_video.xml \

# evb_light permission configurations
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.usb.accessory.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.usb.accessory.xml \
    frameworks/native/data/etc/android.hardware.wifi.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.xml \
    frameworks/native/data/etc/android.hardware.wifi.direct.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.direct.xml \
    frameworks/native/data/etc/android.hardware.opengles.aep.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.opengles.aep.xml \

PRODUCT_COPY_FILES += $(LOCAL_PATH)/android.software.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.xml

# ethernet #
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.ethernet.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.ethernet.xml


# evb_light wifi configuration
PRODUCT_COPY_FILES += \
    vendor/thead/opensource/hal_default/wlan/supplicant_overlay_config/wpa_supplicant_overlay.conf:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/wpa_supplicant_overlay.conf \
    vendor/thead/opensource/hal_default/wlan/supplicant_overlay_config/p2p_supplicant_overlay.conf:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/p2p_supplicant_overlay.conf

# USB properties
PRODUCT_VENDOR_PROPERTIES += \
   ro.boot.usb_mode=1 \
   ro.boot.usb_speed=5

# Display lcd denstity
TARGET_SCREEN_DENSITY := 320

# Enable AVB
BOARD_AVB_ENABLE := true
BOARD_BOOTIMAGE_PARTITION_SIZE := 0x2000000

# BOARD_RECOVERYIMAGE_PARTITION_SIZE := 0x2800000

# Installs gsi keys into ramdisk, to boot a developer GSI with verified boot.
$(call inherit-product, $(SRC_TARGET_DIR)/product/developer_gsi_keys.mk)

# Set Vendor SPL to match platform
VENDOR_SECURITY_PATCH = $(PLATFORM_SECURITY_PATCH)

# Set boot SPL
BOOT_SECURITY_PATCH = $(PLATFORM_SECURITY_PATCH)

# product.img
BOARD_PRODUCTIMAGE_FILE_SYSTEM_TYPE := ext4
TARGET_COPY_OUT_PRODUCT := product

# system_ext.img
BOARD_SYSTEM_EXTIMAGE_FILE_SYSTEM_TYPE := ext4
TARGET_COPY_OUT_SYSTEM_EXT := system_ext

BOARD_AVB_VBMETA_SYSTEM := system system_ext product
BOARD_AVB_VBMETA_SYSTEM_KEY_PATH := external/avb/test/data/testkey_rsa2048.pem
BOARD_AVB_VBMETA_SYSTEM_ALGORITHM := SHA256_RSA2048
BOARD_AVB_VBMETA_SYSTEM_ROLLBACK_INDEX := $(PLATFORM_SECURITY_PATCH_TIMESTAMP)
BOARD_AVB_VBMETA_SYSTEM_ROLLBACK_INDEX_LOCATION := 1

# Enable chained vbmeta for boot images
BOARD_AVB_BOOT_KEY_PATH := external/avb/test/data/testkey_rsa2048.pem
BOARD_AVB_BOOT_ALGORITHM := SHA256_RSA2048
BOARD_AVB_BOOT_ROLLBACK_INDEX := $(PLATFORM_SECURITY_PATCH_TIMESTAMP)
BOARD_AVB_BOOT_ROLLBACK_INDEX_LOCATION := 2

# Fix cts bug CtsThemeHostTestCases about xhdpi assets missing
PRODUCT_AAPT_PREF_CONFIG := xhdpi

# Enable APK Verity, which depends on fs-verity support in kernel.
PRODUCT_PROPERTY_OVERRIDES += ro.apk_verity.mode=2

PRODUCT_COPY_FILES += \
    system/core/libprocessgroup/profiles/cgroups.json:$(TARGET_COPY_OUT_VENDOR)/etc/cgroups.json \
    system/core/libprocessgroup/profiles/task_profiles.json:$(TARGET_COPY_OUT_VENDOR)/etc/task_profiles.json

# fastbootd
PRODUCT_PACKAGES += \
    fastbootd \

# Install evb_light proprietary packages
$(call inherit-product-if-exist, vendor/thead/light/evb_light/evb_light-vendor.mk)
