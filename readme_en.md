# EC Qualcomm

[中文](readme.md) | English

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
App and bootloader share `interface/`, `driver/` and `service/` in the repo root.
`bootloader/prj.conf` sets `CONFIG_APP_BOOTLOADER=y` to enable bootloader-only features
(flash service, image verification, FW update commands). They are disabled in the app by default.

```
west build -p -b qc_module app
west build -p -b qc_module bootloader

west build -p -b qc_module app -- -DOVERLAY_CONFIG=release.conf
```

After the build, these extra files are generated in the `zephyr/` directory:

| Image | Output | Description |
|---|---|---|
| bootloader | `zephyr/bootloader.bin` | Copy of `spi_image.bin` |
| app | `zephyr/app.bin` | `zephyr.bin` with a 32-byte ECFW header + CRC32 added by `tools/pack_ec_image.py` |

### FW version
The version is defined in `app/Kconfig` and set in `app/prj.conf`:
```
CONFIG_APP_FW_VERSION_MAJOR=0
CONFIG_APP_FW_VERSION_MINOR=1
CONFIG_APP_FW_VERSION_PATCH=13
```
The same version is used in: the boot log (`FW version : 0.1.13`), the ACPI 0x0E / 0x0F replies, and the `app.bin` header.

### release.sh
Builds the bootloader and app in one go and merges them into a complete SPI image:
```
source tools/script/setup.sh
source tools/script/release.sh
```

| Step | Command | Output |
|---|---|---|
| 1. build bootloader | `west build -p -b qc_module bootloader -d build/bootloader` | `build/bootloader/zephyr/bootloader.bin` |
| 2. build app | `west build -p -b qc_module app -d build/app` | `build/app/zephyr/app.bin` |
| 3. merge | `python tools/merge_bin.py --file1 bootloader.bin --file2 app.bin` | `build/ec_image.bin` (512 KB) |

Layout of `build/ec_image.bin`:

| Offset | Content |
|---|---|
| `0x00000` | bootloader.bin |
| `0x11000` | app.bin (ECFW header) |

## QEMU test
```
set PATH=%PATH%;C:\Program Files\qemu
export PATH=$PATH:/c/Program\ Files/qemu

qemu-system-arm --version

# When running the app alone, disable the DT code partition; otherwise the image is placed in app_partition and QEMU cannot boot
west build -p -b qemu_cortex_m3 app -- -DCONFIG_USE_DT_CODE_PARTITION=n
west build -t run

west build -p -b qemu_cortex_m3 app -t run -- -DCONFIG_USE_DT_CODE_PARTITION=n
```

## Flash

### flash.sh (pyocd)
```
source tools/script/setup.sh
bash tools/script/flash.sh                    # default: flash build/ec_image.bin to SPI 0x0
bash tools/script/flash.sh <image.bin> <spi_addr>
```

Flow:
1. pyocd loads the RAM flasher (`tools/app_flash/zephyr.bin`) to `0x0c8000`, sets PC = `0x0cc72D` and runs it
2. `tools/app_flash/fw_upgrade.py` sends the image in 2KB chunks through the debug probe to the RAM flasher, which writes it to SPI flash
3. `pyocd cmd -t cortex_m -c "reset"` resets the MCU so it boots from the new SPI image

> `HELPER_ENTRY` (`0x0cc72D`) matches the current `tools/app_flash/zephyr.bin`. Update it if the RAM flasher is rebuilt.

Log (step 1 run manually):
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

$ python3 fw_upgrade.py ../../build/ec_image.bin 0x0

```

### J-Link
```
cd tools/KF_JLINK_Flash_Utility_L0100
python kf_flsh_util.py -w -f ../../build/ec_image.bin
```

## Release
```
source tools/script/setup.sh
source tools/script/release.sh
bash tools/script/flash.sh
```

Log (`source tools/script/release.sh`, excerpt):
```shell
Board: qc_module
-- west build: making build dir /home/andy/project/ec_qualcomm/build/bootloader pristine
...
[166/167] Linking C executable zephyr/zephyr.elf
Memory region         Used Size  Region Size  %age Used
           FLASH:       60136 B        64 KB     91.76%
             RAM:       16072 B        60 KB     26.16%
        retained:          0 GB         2 KB      0.00%
        IDT_LIST:          0 GB        32 KB      0.00%
Generating files from /home/andy/project/ec_qualcomm/build/bootloader/zephyr/zephyr.elf for board: qc_module
 MEC175x_SPI_GEN Version 4.1 2024-12-18
...
  Output FileName        = /home/andy/project/ec_qualcomm/build/bootloader/zephyr/spi_image.bin
  Done with image Generation!
==============================================
[167/167] Copy spi_image.bin -> bootloader.bin
-- west build: making build dir /home/andy/project/ec_qualcomm/build/app pristine
...
[203/204] Linking C executable zephyr/zephyr.elf
Memory region         Used Size  Region Size  %age Used
           FLASH:      106104 B       200 KB     51.81%
             RAM:       30080 B        60 KB     48.96%
        retained:          0 GB         2 KB      0.00%
        IDT_LIST:          0 GB        32 KB      0.00%
Generating files from /home/andy/project/ec_qualcomm/build/app/zephyr/zephyr.elf for board: qc_module
 MEC175x_SPI_GEN Version 4.1 2024-12-18
...
  Output FileName        = /home/andy/project/ec_qualcomm/build/app/zephyr/spi_image.bin
  Done with image Generation!
==============================================
[204/204] Pack zephyr.bin -> app.bin (v0.1.13)
==================================================
 Qualcomm Glymur EC Firmware 打包完成
==================================================
 輸入檔案          : /home/andy/project/ec_qualcomm/build/app/zephyr/zephyr.bin (106104 Bytes)
 輸出映像檔        : /home/andy/project/ec_qualcomm/build/app/zephyr/app.bin (204800 Bytes)
------------------ Header 內容 -------------------
 Magic             : ECFW
 RAM Load Address  : 0x000D0000
 Firmware Size     : 204768 Bytes (0x00031FE0)
 Firmware Version  : 0.1.13 (Hex: 00.01.0D)
 Lowest Version    : 0.1.0 (Hex: 00.01.00)
 Validation Mode   : CRC32 (Ctrl = 0x01)
 CRC32             : 0x67CF4D24
 Aux Data Size     : 0 Bytes
==================================================
[警告] build/bootloader/zephyr/bootloader.bin 大小 (0x80000) 超出 offset 0x10000 限制，將截斷多餘部分。
成功生成 build/ec_image.bin (大小: 0x80000 / 524288 Bytes)
Release image: build/ec_image.bin
```

> The `[警告]` (warning) line is expected: `bootloader.bin` is a full 512KB SPI image whose real data ends at `0xFEFF`. Only 0xFF padding after `0x11000` is truncated, so no content is lost.

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

