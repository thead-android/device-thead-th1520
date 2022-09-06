
# Graphics HAL
PRODUCT_PACKAGES += \
    android.hardware.graphics.allocator@2.0-impl \
    android.hardware.graphics.allocator@2.0-service \
    android.hardware.graphics.composer@2.1-impl \
    android.hardware.graphics.composer@2.1-service \
    android.hardware.graphics.mapper@2.0-impl

# FIXME: these modules no longer included in API 30+,
# however needed by vendor code, such as display hidl.
PRODUCT_PACKAGES += \
    android.hardware.configstore@1.1-service \
    vndservice \
    vndservicemanager \

# FIXME use harware memtrack
PRODUCT_PACKAGES += \
    memtrack.light \
    android.hardware.memtrack@1.0-service \
    android.hardware.memtrack@1.0-impl

PREBUILT_GPU_BGM := true

# Add gpu bxm binary
PRODUCT_PACKAGES += \
    ocl_unit_test \
    pdump \
    pvr_memory_test \
    pvr_mutex_perf_test_mx \
    pvrdebug \
    pvrhtb2txt \
    pvrhtbd \
    pvrhwperf \
    pvrlogdump \
    pvrlogsplit \
    pvrsrvctl \
    pvrtld \
    rgx_blit_test \
    rgx_compute_test \
    rgx_kicksync_test \
    rgx_triangle_test \
    rgx_twiddling_test \
    testwrap \

# Add gpu bxm lib
PRODUCT_PACKAGES += \
    libEGL_powervr \
    libGLESv1_CM_powervr \
    libGLESv2_powervr \
    hwcomposer.light \
    gralloc.light \
    vulkan.powervr \
    libIMGegl \
    libPVROCL \
    libPVRScopeServices \
    libcreatesurface \
    libglslcompiler \
    libpvrANDROID_WSEGL \
    libsrv_um \
    libsutu_display \
    libufwriter \
    libusc \

# Opengles version
PRODUCT_VENDOR_PROPERTIES += \
    ro.hardware.egl=powervr  \
    ro.opengles.version=196610

# Add gpu bxm test apks
PRODUCT_PACKAGES += \
    eglinfo \
    gles1test1 \
    gles2test1 \
    gles3test1\
    hal_blit_test \
    launcher \
    tearing_test \
    vkbonjour

# GPU copy files
PRODUCT_COPY_FILES += \
    vendor/thead/prebuilt/gpu_bxm/etc/init/android.hardware.atrace@1.0-service.img.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/android.hardware.atrace@1.0-service.img.rc \
    vendor/thead/prebuilt/gpu_bxm/firmware/rgx.fw.36.52.104.182:$(TARGET_COPY_OUT_VENDOR)/firmware/rgx.fw.36.52.104.182 \
    vendor/thead/prebuilt/gpu_bxm/firmware/rgx.sh.36.52.104.182:$(TARGET_COPY_OUT_VENDOR)/firmware/rgx.sh.36.52.104.182 \
