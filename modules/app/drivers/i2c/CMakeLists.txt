# Copyright (c) 2025 Microchip Technology Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

if(CONFIG_I2C_XEC_V2_CUSTOM)
  zephyr_library()
  # i2c-priv.h lives in the Zephyr I2C driver directory
  zephyr_library_include_directories(${ZEPHYR_BASE}/drivers/i2c)
  zephyr_library_sources(i2c_mchp_xec_v2_custom.c)
endif()
