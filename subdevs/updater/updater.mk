
TARGET_NO_RECOVERY := true

AB_OTA_UPDATER := true

AB_OTA_PARTITIONS += \
    boot \
    system \
    vendor \
    vbmeta

# FIXME use real bootctrl
PRODUCT_PACKAGES += \
    android.hardware.boot@1.2-service \
    android.hardware.boot@1.2-impl
