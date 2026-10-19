#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

DEVICE_PATH := device/xiaomi/chopin

# Inherit from mt6893-common
include device/xiaomi/mt6893-common/BoardConfigCommon.mk

# Display
TARGET_SCREEN_DENSITY := 440

# Properties
TARGET_PRODUCT_PROP += $(DEVICE_PATH)/product.prop
TARGET_VENDOR_PROP += $(DEVICE_PATH)/vendor.prop

# Security Patch Level
VENDOR_SECURITY_PATCH := 2023-10-01

# Inherit the proprietary files
include vendor/xiaomi/chopin/BoardConfigVendor.mk

# SELinux policy for this device.
#
# MTK's graphics HAL and SurfaceFlinger allocate through /dev/ion, but the
# shipped policy never granted them access, so every buffer import failed
# with EBADF and video mode stayed black.
BOARD_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy
