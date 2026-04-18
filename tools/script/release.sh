
west build -p -b qemu_cortex_m3 --sysbuild app -- -DOVERLAY_CONFIG=release.conf

${ZEPHYR_SDK_INSTALL_DIR}/arm-zephyr-eabi/bin/arm-zephyr-eabi-objcopy.exe -O ihex build/app/zephyr/zephyr.elf app.hex
${ZEPHYR_SDK_INSTALL_DIR}/arm-zephyr-eabi/bin/arm-zephyr-eabi-objcopy.exe -O ihex build/bootloader/zephyr/zephyr.elf boot.hex

python zephyr-rtos/scripts/build/mergehex.py --output merged.hex boot.hex app.hex

${ZEPHYR_SDK_INSTALL_DIR}/arm-zephyr-eabi/bin/arm-zephyr-eabi-objcopy.exe -I ihex -O binary --gap-fill 0xff app.hex app_release.bin

# qemu-system-arm -nographic -machine lm3s6965evb -device loader,file=merged.hex
echo "qemu-system-arm -nographic -machine lm3s6965evb -device loader,file=merged.hex"
