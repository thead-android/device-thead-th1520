# FIXME Use thead hardware resolution to replace  aosp default
# Keymaster
PRODUCT_PACKAGES += \
    android.hardware.keymaster@4.0-impl \
    android.hardware.keymaster@4.0-service

# Gatekeeper
PRODUCT_PACKAGES += \
    android.hardware.gatekeeper@1.0-service.software \
    android.hardware.gatekeeper@1.0-impl

# Enable AVB
BOARD_AVB_ENABLE := true
BOARD_BOOTIMAGE_PARTITION_SIZE := 0x2000000

# BOARD_RECOVERYIMAGE_PARTITION_SIZE := 0x2800000

# Enable chained vbmeta for boot images
BOARD_AVB_BOOT_KEY_PATH := external/avb/test/data/testkey_rsa2048.pem
BOARD_AVB_BOOT_ALGORITHM := SHA256_RSA2048
BOARD_AVB_BOOT_ROLLBACK_INDEX := $(PLATFORM_SECURITY_PATCH_TIMESTAMP)
BOARD_AVB_BOOT_ROLLBACK_INDEX_LOCATION := 2

