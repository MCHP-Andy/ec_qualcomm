# EC Qualcomm

## Zephyr toolchain
Windows Minimal: [link](https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.8/zephyr-sdk-0.16.8_windows-x86_64_minimal.7z)

arm-zephyr-eabi: [link](https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.8/toolchain_windows-x86_64_arm-zephyr-eabi.7z)


## SDK install
```
tools\script\install.bat
``` 

## Setup
```
tools\script\setup.bat
```

## Build
The bootloader project enables `CONFIG_EC_BOOTLOADER` in
`bootloader/prj.conf`; the app build leaves this option disabled.

```
west build -p -b qual_module/mec172x_nsz app
west build -p -b qual_module/mec172x_nsz bootloader

west build -p -b qual_module/mec172x_nsz app -- -DOVERLAY_CONFIG=release.conf

west build -p -b qual_module/mec172x_nsz --sysbuild app -- -DOVERLAY_CONFIG=release.conf
source tools/script/release.sh
```

## QEMU test
```
set PATH=%PATH%;C:\Program Files\qemu
export PATH=$PATH:/c/Program\ Files/qemu

qemu-system-arm --version

west build -p -b qemu_cortex_m3 app
west build -t run

west build -p -b qemu_cortex_m3 app -t run

west build -p -b qemu_cortex_m3 --sysbuild app -- -DOVERLAY_CONFIG=release.conf
source tools/script/release.sh
```

## Flash
```
cd tools/KF_JLINK_Flash_Utility_L0100
python kf_flsh_util.py -w -f ../../build/zephyr/spi_image.bin
```

```shell
$ cd tools/app_flash

$ pyocd cmd -t cortex_m -c "halt; load zephyr.bin 0x0c8000; wreg pc 0x0cc72D; go"
# OR
$ pyocd cmd -t cortex_m
pyocd> halt
Successfully halted device
pyocd> 
pyocd> 
pyocd> load zephyr.bin 0x0c8000
[==================================================] 100%
[==================================================] 100%
pyocd> 
pyocd> wreg pc 0x0cc72D
pyocd> reg pc
pc = 0x000cc72c
pyocd> go
Successfully resumed device
pyocd> exit

$ python3 fw_upgrade.py ../../build/zephyr/spi_image.bin 0x0

```

## Release
```shell
(.venv) andy@ThinkPad-P1-Gen-7:~/Desktop/MCHP/ec_qualcomm$ cp build/zephyr/spi_image.bin ./image.bin
(.venv) andy@ThinkPad-P1-Gen-7:~/Desktop/MCHP/ec_qualcomm$ cp build/zephyr/zephyr.bin ./app.bin
(.venv) andy@ThinkPad-P1-Gen-7:~/Desktop/MCHP/ec_qualcomm$ python tools/pack_ec_image.py -i app.bin -o WoS_EC_MainFirmware_Release.bin
==================================================
 Qualcomm Glymur EC Firmware 打包完成
==================================================
 輸入檔案          : app.bin (95440 Bytes)
 輸出映像檔        : WoS_EC_MainFirmware_Release.bin (95472 Bytes)
------------------ Header 內容 -------------------
 Magic             : ECFW
 RAM Load Address  : 0x000D0000
 Firmware Size     : 95440 Bytes (0x000174D0)
 Firmware Version  : 0.1.13 (Hex: 00.01.0D)
 Lowest Version    : 0.1.0 (Hex: 00.01.00)
 Validation Mode   : CRC32 (Ctrl = 0x01)
 CRC32             : 0xB3A6A169
 Aux Data Size     : 0 Bytes
==================================================
(.venv) andy@ThinkPad-P1-Gen-7:~/Desktop/MCHP/ec_qualcomm$ python ./merge_bin.py --file1 image.bin --file2 WoS_EC_MainFirmware_Release.bin --output tt.bin
[警告] image.bin 大小 (0x80000) 超出 offset 0x10000 限制，將截斷多餘部分。
成功生成 tt.bin (大小: 0x80000 / 524288 Bytes)
```

## Framework



## Test

### Phase 1

#### Flash W/R
```bash
*** Booting Zephyr OS build v3.7.1 ***
Hello from bootloader
App address in: 0x000c0000
uart:~$ 
uart:~$ 
uart:~$ flash 
flash: wrong parameter count
flash - Flash shell commands
Subcommands:
  erase      : [<device>] <page address> [<size>]
  read       : [<device>] <address> [<Dword count>]
  test       : [<device>] <address> <size> <repeat count>
  write      : [<device>] <address> <dword> [<dword>...]
  load       : [<device>] <address> <size>
  page_info  : [<device>] <address>
uart:~$ 
uart:~$ flash read SST25PF040 0x1000 0x10
00001000: ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff |........ ........|

uart:~$ flash write SST25PF040 0x1000 0x01 0x02 0x03 0x04
Write OK.
Verified.
uart:~$ flash read SST25PF040 0x1000 0x10
00001000: 01 00 00 00 02 00 00 00  03 00 00 00 04 00 00 00 |........ ........|

uart:~$ flash erase SST25PF040 0x1000 0x1000
Erase success.
uart:~$ flash read SST25PF040 0x1000 0x10
00001000: ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff |........ ........|

uart:~$ 
```

#### I2C callback (loopback test)
```bash
uart:~$ i2c write i2c@40004800 0x76 0x25 0x01 0x04 0x55 0x00 0xff 0x00
[00:00:52.043,914] <inf> acpi_i2c: ACPI CMD 0x25 expected write length: mand=2, opt=5
[00:00:52.044,006] <inf> acpi_i2c: Received mandatory part of ACPI CMD: 0x25, length: 2
[00:00:52.044,036] <inf> acpi: Handled ACPI cmd 0x25 successfully
[00:00:52.044,555] <inf> acpi_i2c: Received complete ACPI CMD: 0x25, length: 7
[00:00:52.044,555] <inf> acpi: Handled ACPI cmd 0x25 successfully
...
uart:~$
uart:~$ i2c write i2c@40004800 0x76 0x25 0x01
[00:01:07.023,956] <inf> acpi_i2c: ACPI CMD 0x25 expected write length: mand=2, opt=5
[00:01:07.024,047] <inf> acpi_i2c: Received mandatory part of ACPI CMD: 0x25, length: 2
[00:01:07.024,078] <inf> acpi: Handled ACPI cmd 0x25 successfully
uart:~$ i2c direct_read i2c@40004800 0x76 10
00000000: 04 55 00 ff 00 00 00 00  00 00 00 00 00 00 00 00 |.U...... ........|
```

---
### Phase 2
