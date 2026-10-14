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
