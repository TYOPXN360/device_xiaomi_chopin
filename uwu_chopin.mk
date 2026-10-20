#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

# Inherit from device makefile.
$(call inherit-product, device/xiaomi/chopin/device.mk)

# Inherit the AOSP base telephony product.
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)

# Inherit the uwuAOSP configuration. The "uwu_" product-name prefix is what
# makes build/make/core/config.mk pull in vendor/uwu/config/BoardConfigUwU.mk,
# which defines the KERNEL_* variables the uwu Soong generators depend on.
UWU_DEVICE_TYPE := phone
UWU_SUPPORTS_TELEPHONY := true
$(call inherit-product, vendor/uwu/config/common.mk)

PRODUCT_NAME := uwu_chopin
PRODUCT_DEVICE := chopin
PRODUCT_MANUFACTURER := Xiaomi
PRODUCT_BRAND := POCO
PRODUCT_MODEL := 21061110AG

PRODUCT_CHARACTERISTICS := nosdcard

PRODUCT_BUILD_PROP_OVERRIDES += \
    BuildFingerprint= POCO/chopin_global/chopin:12/SP1A.210812.016/V14.0.6.0.TKPMIXM:user/release-keys \
    DeviceProduct=chopin \
    SystemName=chopin
