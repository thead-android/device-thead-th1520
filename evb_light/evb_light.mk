#
# Copyright 2022 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#
LOCAL_64ONLY := _64

# Aosp gsi and basic vendor
$(call inherit-product, device/thead/common/aosp_common.mk)

# Thead  system_ext
$(call inherit-product-if-exist, device/thead/light/light_system_ext.mk)

# Riscv patch for system
$(call inherit-product, device/thead/common/system_riscv_patch.mk)

# AOSP default vendor configs
$(call inherit-product, device/thead/common/aosp_vendor.mk)

# Thead evb_light vendor packages
$(call inherit-product, device/thead/light/evb_light/device-evb_light.mk)

PRODUCT_MANUFACTURER := Thead
PRODUCT_BRAND := Android
PRODUCT_NAME := evb_light
PRODUCT_DEVICE := evb_light
PRODUCT_MODEL := AOSP on C910(RISV64)
