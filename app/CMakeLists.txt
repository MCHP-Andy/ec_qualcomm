# Copyright (c) 2025 Microchip Technology Inc.
# SPDX-License-Identifier: Apache-2.0
 
cmake_minimum_required(VERSION 3.20.0)

# list(APPEND EXTRA_ZEPHYR_MODULES
#   ${CMAKE_CURRENT_SOURCE_DIR}/../modules/app
# )

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(qualcomm)

# FILE(GLOB app_sources src/*.c)
# target_sources(app PRIVATE ${app_sources})

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_sources(app PRIVATE main.c)
add_subdirectory(linker)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../driver
                 ${CMAKE_CURRENT_BINARY_DIR}/shared_driver)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../service
                 ${CMAKE_CURRENT_BINARY_DIR}/shared_service)

# Release: pack zephyr.bin into app.bin (Qualcomm EC image header + CRC32)
set(APP_BIN ${ZEPHYR_BINARY_DIR}/app.bin)
set(APP_FW_VERSION
    "${CONFIG_APP_FW_VERSION_MAJOR}.${CONFIG_APP_FW_VERSION_MINOR}.${CONFIG_APP_FW_VERSION_PATCH}")
add_custom_command(
  OUTPUT  ${APP_BIN}
  COMMAND ${PYTHON_EXECUTABLE}
          ${CMAKE_CURRENT_SOURCE_DIR}/../tools/pack_ec_image.py
          -i ${ZEPHYR_BINARY_DIR}/${KERNEL_BIN_NAME}
          -o ${APP_BIN}
          -v ${APP_FW_VERSION}
  DEPENDS ${logical_target_for_zephyr_elf}
          ${CMAKE_CURRENT_SOURCE_DIR}/../tools/pack_ec_image.py
  COMMENT "Pack ${KERNEL_BIN_NAME} -> app.bin (v${APP_FW_VERSION})"
)
add_custom_target(app_bin ALL DEPENDS ${APP_BIN})
