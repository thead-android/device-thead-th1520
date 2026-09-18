
# Graphics HAL
PRODUCT_PACKAGES += \
    android.hardware.graphics.allocator-service.minigbm \
    android.hardware.composer.hwc3-service.drm \
    mapper.minigbm \
    gralloc.minigbm

# The HDMI bridge supplies fallback modes but no EDID preferred flag on LPi4A.
# Keep Android in the tested 16:9 layout instead of picking 1024x768 solely
# because it happens to be the first connector mode.
PRODUCT_VENDOR_PROPERTIES += \
    vendor.hwc.drm.preferred_mode=1280x720

# FIXME: these modules no longer included in API 30+,
# however needed by vendor code, such as display hidl.
PRODUCT_PACKAGES += \
    android.hardware.configstore@1.1-service \
    vndservice \
    vndservicemanager \

# FIXME use harware memtrack
PRODUCT_PACKAGES += \
    memtrack.th1520 \
    android.hardware.memtrack@1.0-service \
    android.hardware.memtrack@1.0-impl
