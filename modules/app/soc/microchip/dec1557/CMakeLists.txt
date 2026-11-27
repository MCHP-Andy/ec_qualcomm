# Copyright (c) 2025 Microchip Technology Inc.
# SPDX-License-Identifier: Apache-2.0

# Reuse the in-tree Microchip MEC family/series sources (mec172x + common).
add_subdirectory(${ZEPHYR_BASE}/soc/microchip/mec ${CMAKE_CURRENT_BINARY_DIR}/mec)
