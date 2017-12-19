# GPS
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/gps/apdr.conf:system/vendor/etc/apdr.conf \
    $(LOCAL_PATH)/configs/gps/flp.conf:system/vendor/etc/flp.conf \
    $(LOCAL_PATH)/configs/gps/gps.conf:system/vendor/etc/gps.conf \
    $(LOCAL_PATH)/configs/gps/izat.conf:system/vendor/etc/izat.conf \
    $(LOCAL_PATH)/configs/gps/lowi.conf:system/vendor/etc/lowi.conf \
    $(LOCAL_PATH)/configs/gps/sap.conf:system/vendor/etc/sap.conf \
    $(LOCAL_PATH)/configs/gps/xtwifi.conf:system/vendor/etc/xtwifi.conf

# GPS
PRODUCT_PACKAGES += \
    gps.msm8998 \
    libcurl \
    libgnsspps \
    libvehiclenetwork-native

# Permissions
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.location.gps.xml:system/etc/permissions/android.hardware.location.gps.xml