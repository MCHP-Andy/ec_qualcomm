# Copyright (c) 2025 Microchip Technology Inc.
# SPDX-License-Identifier: Apache-2.0
 
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bootloader)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_sources(app PRIVATE main.c)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../driver
                 ${CMAKE_CURRENT_BINARY_DIR}/shared_driver)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../service
                 ${CMAKE_CURRENT_BINARY_DIR}/shared_service)

# Release: copy spi_image.bin to bootloader.bin in the same directory
# Only on boards that generate an SPI image (SPI_IMAGE_NAME set in board.cmake)
if(SPI_IMAGE_NAME)
  set(BOOTLOADER_BIN ${ZEPHYR_BINARY_DIR}/bootloader.bin)
  add_custom_command(
    OUTPUT  ${BOOTLOADER_BIN}
    COMMAND ${CMAKE_COMMAND} -E copy
            ${ZEPHYR_BINARY_DIR}/${SPI_IMAGE_NAME} ${BOOTLOADER_BIN}
    DEPENDS ${logical_target_for_zephyr_elf}
    COMMENT "Copy ${SPI_IMAGE_NAME} -> bootloader.bin"
  )
  add_custom_target(bootloader_bin ALL DEPENDS ${BOOTLOADER_BIN})
endif()
