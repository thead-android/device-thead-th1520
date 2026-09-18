# Wi-Fi framework-facing AIDL HAL and nl80211 daemons.
PRODUCT_PACKAGES += \
    android.hardware.wifi-service \
    wpa_supplicant \
    hostapd \
    Lpi4aWifiOverlay \
    thead_wpa_supplicant.conf \
    hostapd.conf \
    p2p_supplicant.conf

# AIC8800D firmware is initialized by the existing BSP/FDRV early-init sequence.
# UART4 then exposes the tested H4 transport at 1.5 Mbaud with RTS/CTS.
PRODUCT_PACKAGES += android.hardware.bluetooth-service.default
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.bluetooth.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.bluetooth.xml \
    frameworks/native/data/etc/android.hardware.bluetooth_le.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.bluetooth_le.xml
PRODUCT_VENDOR_PROPERTIES += \
    vendor.ser.bt-uart=/dev/ttyS4 \
    vendor.ser.bt-baud=1500000 \
    vendor.ser.bt-rtscts=true
PRODUCT_PRODUCT_PROPERTIES += \
    bluetooth.profile.gatt.enabled=true \
    bluetooth.profile.hid.host.enabled=true \
    bluetooth.profile.a2dp.source.enabled=true \
    bluetooth.profile.avrcp.target.enabled=true
