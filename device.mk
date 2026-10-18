#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#


# NFC
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.nfc.hcef.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(PRODUCT_DEVICE)/android.hardware.nfc.hcef.xml \
    frameworks/native/data/etc/android.hardware.nfc.hce.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(PRODUCT_DEVICE)/android.hardware.nfc.hce.xml \
    frameworks/native/data/etc/android.hardware.nfc.uicc.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(PRODUCT_DEVICE)/android.hardware.nfc.uicc.xml \
    frameworks/native/data/etc/android.hardware.nfc.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(PRODUCT_DEVICE)/android.hardware.nfc.xml \
    frameworks/native/data/etc/com.android.nfc_extras.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(PRODUCT_DEVICE)/com.android.nfc_extras.xml

#
# libdmabufheap: make BufferAllocator::CheckIonSupport() report ION support.
#
# The camera HAL here is an Android 13 prebuilt and still uses ION. It picks
# its physical-address path with
#
#     helpMapPhyAddr() { return CheckIonSupport() ? Ion() : Imem(); }
#
# and its Imem() branch was built without MTK's IMEM driver, so that branch
# is nothing but __android_log_assert("no define IMEM DRV"). Reaching it
# aborts camerahalserver during the first 3A allocation, so no capture
# session is ever configured and the camera app fails to open.
#
# AOSP hardcodes CheckIonSupport() to false now that ION is gone upstream,
# but our kernel still provides /dev/ion and its ION ioctls work, so the
# Ion() branch is the one that can succeed. This prebuilt copy of the vendor
# library has that single function patched from "mov w0, wzr" to
# "mov w0, #1" (0x9610 and 0x9f20), which is the whole change: two
# instructions in an otherwise untouched binary. AOSP sources stay as-is.
#
# Overlay
PRODUCT_PACKAGES += \
    FrameworksResOverlayChopin \
    SystemUIOverlayChopin \

PRODUCT_ENFORCE_RRO_TARGETS := *

# Soong
PRODUCT_SOONG_NAMESPACES += \
    $(LOCAL_PATH)

# Sku properties
PRODUCT_COPY_FILES += \
    $(call find-copy-subdir-files,*,$(LOCAL_PATH)/sku/,$(TARGET_COPY_OUT_VENDOR)/)

# Rootdir
PRODUCT_PACKAGES += \
    init.project.rc

# ION front-end the camera blobs resolve their ion_* calls against.
# See shims/Android.bp: AOSP libion.so rejects them before the kernel, and
# libmtkcam_imgbuf.so has its DT_NEEDED repointed here by extract-files.py.
PRODUCT_PACKAGES += \
    libio2

# Inherit from mt6893-common
$(call inherit-product, device/xiaomi/mt6893-common/mt6893.mk)

# Inherit the proprietary files
$(call inherit-product, vendor/xiaomi/chopin/chopin-vendor.mk)
