PRODUCT_PACKAGES += \
	libimgcustom \
	libnnaruntime \
	neuralnetworks_img_nna_sl_driver_hw_prebuilt \
	android.hardware.neuralnetworks-shim-service-img-nn-hw \
	basic-debug \

PRODUCT_COPY_FILES += \
	vendor/thead/prebuilt/nna_um/etc/android.hardware.neuralnetworks-shim-service-img-nn.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/android.hardware.neuralnetworks-shim-service-img-nn.rc \

DEVICE_MANIFEST_FILE += \
 	vendor/thead/prebuilt/nna_um/etc/android.hardware.neuralnetworks-shim-service-img-nn.xml
