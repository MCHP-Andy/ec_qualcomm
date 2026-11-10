#!/usr/bin/env bash
# Flash an SPI image to the external SPI flash through pyOCD.
#
# Flow (see readme.md "Flash"):
#   1. Load the RAM flasher helper (tools/app_flash/zephyr.bin) to SRAM and run it
#   2. fw_upgrade.py sends the image to the helper over the debug probe,
#      and the helper writes it to SPI flash
#
# Usage (from repo root, after tools/script/setup.sh):
#   bash tools/script/flash.sh [image.bin] [spi_addr]
#     image.bin : default build/ec_image.bin (output of release.sh)
#     spi_addr  : default 0x0

set -e

WORKSPACE=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
FLASH_TOOL_DIR=${WORKSPACE}/tools/app_flash

IMAGE=$(realpath "${1:-${WORKSPACE}/build/ec_image.bin}")
SPI_ADDR=${2:-0x0}

# RAM flasher helper: load address and entry (Thumb, bit0 = 1)
HELPER_BIN=${FLASH_TOOL_DIR}/zephyr.bin
HELPER_LOAD_ADDR=0x0c8000
HELPER_ENTRY=0x0cc72D

if [ ! -f "${IMAGE}" ]; then
    echo "Image not found: ${IMAGE}"
    echo "Run 'source tools/script/release.sh' first, or pass an image path."
    exit 1
fi

echo "Image    : ${IMAGE}"
echo "SPI addr : ${SPI_ADDR}"

# 1. Load and run the RAM flasher helper
echo "== Load flasher helper to ${HELPER_LOAD_ADDR} =="
pyocd cmd -t cortex_m -c "halt; load ${HELPER_BIN} ${HELPER_LOAD_ADDR}; wreg pc ${HELPER_ENTRY}; go"

# Give the helper time to boot
sleep 1

# 2. Transfer the image to SPI flash
echo "== Write image to SPI flash =="
cd "${FLASH_TOOL_DIR}"
python3 fw_upgrade.py "${IMAGE}" "${SPI_ADDR}"

# 3. Reset MCU so it boots from the new SPI image
echo "== Reset MCU =="
pyocd cmd -t cortex_m -c "reset"
