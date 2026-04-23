

export OBJ_COPY=${ZEPHYR_SDK_INSTALL_DIR}/arm-zephyr-eabi/bin/arm-zephyr-eabi-objcopy.exe
export BOOT_PATH=build/bootloader/zephyr
export APP_PATH=build/app/zephyr
export MERGE_PATH=build/zephyr

# export BOARD_NAME=qemu_cortex_m3
export BOARD_NAME=qual_module/mec172x_nsz

# Build project
echo "Board: ${BOARD_NAME}"
west build -p -b ${BOARD_NAME} --sysbuild app -- -DOVERLAY_CONFIG=release.conf

# Merge hex
${ZEPHYR_SDK_INSTALL_DIR}/arm-zephyr-eabi/bin/arm-zephyr-eabi-objcopy.exe -O ihex ${BOOT_PATH}/zephyr.elf ${MERGE_PATH}/boot.hex
${ZEPHYR_SDK_INSTALL_DIR}/arm-zephyr-eabi/bin/arm-zephyr-eabi-objcopy.exe -O ihex ${APP_PATH}/zephyr.elf ${MERGE_PATH}/app.hex

python zephyr-rtos/scripts/build/mergehex.py --output ${MERGE_PATH}/merged.hex ${MERGE_PATH}/boot.hex ${MERGE_PATH}/app.hex

if [ "$BOARD_NAME" = "qemu_cortex_m3" ]; then
    echo "qemu-system-arm -nographic -machine lm3s6965evb -device loader,file=${MERGE_PATH}/merged.hex"
else
    echo "Board: ${BOARD_NAME}"
    # Gen zephyr.bin and spi_image.bin
    ${OBJ_COPY} -I ihex -O binary --gap-fill 0xff ${MERGE_PATH}/merged.hex ${MERGE_PATH}/zephyr.bin
    ${OBJ_COPY} -I ihex -O binary --gap-fill 0xff ${MERGE_PATH}/merged.hex zephyr.bin
    ${MEC5_SPI_GEN} -i ${MEC5_SPI_CFG} -o ${MERGE_PATH}/spi_image.bin
fi
