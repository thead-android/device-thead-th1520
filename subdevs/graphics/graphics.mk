
# Graphics HAL
PRODUCT_PACKAGES += \
    android.hardware.graphics.allocator@2.0-impl \
    android.hardware.graphics.allocator@2.0-service \
    android.hardware.graphics.composer@2.1-impl \
    android.hardware.graphics.composer@2.1-service \
    android.hardware.graphics.mapper@2.0-impl-2.1

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

