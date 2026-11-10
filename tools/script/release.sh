#!/usr/bin/env bash
# Build bootloader + app and merge them into one SPI flash image.
# Usage (from repo root, after tools/script/setup.sh):
#   source tools/script/release.sh

BOARD_NAME=qc_module
BOOT_PATH=build/bootloader/zephyr
APP_PATH=build/app/zephyr
OUTPUT_BIN=build/ec_image.bin

release_build() {
    echo "Board: ${BOARD_NAME}"

    # Build bootloader -> build/bootloader/zephyr/bootloader.bin
    west build -p -b ${BOARD_NAME} bootloader -d build/bootloader || return 1

    # Build app -> build/app/zephyr/app.bin
    west build -p -b ${BOARD_NAME} app -d build/app || return 1

    # Merge bootloader.bin + app.bin
    python tools/merge_bin.py \
        --file1 ${BOOT_PATH}/bootloader.bin \
        --file2 ${APP_PATH}/app.bin \
        --output ${OUTPUT_BIN} || return 1

    echo "Release image: ${OUTPUT_BIN}"
}

release_build
