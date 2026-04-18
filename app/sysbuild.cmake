# Copyright (c) 2024 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

ExternalZephyrProject_Add(
  APPLICATION bootloader
  SOURCE_DIR ${APP_DIR}/../bootloader
  # BOARD ${SB_CONFIG_REMOTE_BOARD}
)

add_dependencies(${DEFAULT_IMAGE} bootloader)
sysbuild_add_dependencies(FLASH ${DEFAULT_IMAGE} bootloader)
