# thead base camera modules
PRODUCT_PACKAGES += \
    android.hardware.camera.provider@2.5-service-thead \
    libthead_camera_core \
    libyaml

# odm must make their media_profiles_V1_0.xml to odm/etc
PRODUCT_COPY_FILES += \
    device/thead/light/subdevs/camera/media_profiles.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_profiles_V1_0.xml \
    frameworks/native/data/etc/android.hardware.camera.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.xml \
    frameworks/native/data/etc/android.hardware.camera.front.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.front.xml
