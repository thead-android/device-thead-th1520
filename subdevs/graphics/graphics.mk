
# Graphics HAL
PRODUCT_PACKAGES += \
    android.hardware.graphics.allocator-service.th1520-legacy \
    android.hardware.graphics.allocator@4.0-service.th1520 \
    android.hardware.graphics.mapper@4.0-impl.th1520 \
    libth1520_ion_compat \
    android.hardware.composer.hwc3-service.th1520-g2d

PRODUCT_VENDOR_PROPERTIES += vendor.hwc.g2d.enabled=true
PRODUCT_SYSTEM_PROPERTIES += debug.sf.force_rgba_cursor=true

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
