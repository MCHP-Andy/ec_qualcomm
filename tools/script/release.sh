
west build -p -b qemu_cortex_m3 --sysbuild app -- -DOVERLAY_CONFIG=release.conf

${ZEPHYR_SDK_INSTALL_DIR}/arm-zephyr-eabi/bin/arm-zephyr-eabi-objcopy.exe -O ihex build/app/zephyr/zephyr.elf app.hex
${ZEPHYR_SDK_INSTALL_DIR}/arm-zephyr-eabi/bin/arm-zephyr-eabi-objcopy.exe -O ihex build/bootloader/zephyr/zephyr.elf boot.hex

python zephyr-rtos/scripts/build/mergehex.py --output merged.hex boot.hex app.hex

# qemu-system-arm -nographic -machine lm3s6965evb -device loader,file=merged.hex



# For MEC175x

# west build -p -b qual_module/mec172x_nsz --sysbuild app -- -DOVERLAY_CONFIG=release.conf

# export OBJ_COPY=${ZEPHYR_SDK_INSTALL_DIR}/arm-zephyr-eabi/bin/arm-zephyr-eabi-objcopy.exe

# # 處理 a.elf：從 0xb0000 變成 0x00000
# ${OBJ_COPY} -O binary --change-addresses -0xb0000 build/bootloader/zephyr/zephyr.elf a_zero.bin

# # 處理 b.elf：從 0xc0000 變成 0x00000
# ${OBJ_COPY} -O binary --change-addresses -0xc0000 build/app/zephyr/zephyr.elf b_zero.bin

# # 1. 先將 a_zero.bin 墊到 0x10000 長度 (不足處補 0xff)
# ${OBJ_COPY} -I binary -O binary --pad-to 0x10000 --gap-fill 0xff a_zero.bin a_padded.bin

# # 2. 合併檔案 (Windows 指令)
# # copy /b a_padded.bin + b_zero.bin c.bin
# cat a_padded.bin b_zero.bin > zephyr.bin

# # SPI gen
# ${MEC5_SPI_GEN} -i ${MEC5_SPI_CFG} -o merge.bin
