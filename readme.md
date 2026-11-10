# EC Qualcomm

中文 | [English](readme_en.md)

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
App 與 bootloader 共用根目錄下的 `interface/`、`driver/`、`service/`。
`bootloader/prj.conf` 設定 `CONFIG_APP_BOOTLOADER=y` 以啟用 bootloader 專屬功能
(flash service、image 驗證、FW update 指令)，app 預設不啟用。

```
west build -p -b qc_module app
west build -p -b qc_module bootloader

west build -p -b qc_module app -- -DOVERLAY_CONFIG=release.conf
```

build 完成後會在 `zephyr/` 目錄額外產生:

| Image | 輸出檔 | 說明 |
|---|---|---|
| bootloader | `zephyr/bootloader.bin` | `spi_image.bin` 的複本 |
| app | `zephyr/app.bin` | `zephyr.bin` 經 `tools/pack_ec_image.py` 加上 32-Byte ECFW header + CRC32 |

### FW version
版本定義於 `app/Kconfig`，數值設定於 `app/prj.conf`:
```
CONFIG_APP_FW_VERSION_MAJOR=0
CONFIG_APP_FW_VERSION_MINOR=1
CONFIG_APP_FW_VERSION_PATCH=13
```
同一組版本會用於: 開機 log (`FW version : 0.1.13`)、ACPI 0x0E / 0x0F 回覆、`app.bin` header。

### release.sh
一次 build bootloader + app 並合併成完整 SPI image:
```
source tools/script/setup.sh
source tools/script/release.sh
```

| 步驟 | 指令 | 產出 |
|---|---|---|
| 1. build bootloader | `west build -p -b qc_module bootloader -d build/bootloader` | `build/bootloader/zephyr/bootloader.bin` |
| 2. build app | `west build -p -b qc_module app -d build/app` | `build/app/zephyr/app.bin` |
| 3. merge | `python tools/merge_bin.py --file1 bootloader.bin --file2 app.bin` | `build/ec_image.bin` (512 KB) |

`build/ec_image.bin` 的 layout:

| Offset | 內容 |
|---|---|
| `0x00000` | bootloader.bin |
| `0x11000` | app.bin (ECFW header) |

## QEMU test
```
set PATH=%PATH%;C:\Program Files\qemu
export PATH=$PATH:/c/Program\ Files/qemu

qemu-system-arm --version

# App 單獨執行時需關閉 DT code partition，否則 image 被放在 app_partition，QEMU 無法開機
west build -p -b qemu_cortex_m3 app -- -DCONFIG_USE_DT_CODE_PARTITION=n
west build -t run

west build -p -b qemu_cortex_m3 app -t run -- -DCONFIG_USE_DT_CODE_PARTITION=n
```

## Flash

### flash.sh (pyocd)
```
source tools/script/setup.sh
bash tools/script/flash.sh                    # 預設燒錄 build/ec_image.bin 到 SPI 0x0
bash tools/script/flash.sh <image.bin> <spi_addr>
```

流程:
1. 用 pyocd 將 RAM flasher (`tools/app_flash/zephyr.bin`) 載入 `0x0c8000`，設定 PC = `0x0cc72D` 後執行
2. `tools/app_flash/fw_upgrade.py` 透過 debug probe 把 image 分段 (2KB) 傳給 RAM flasher 寫入 SPI flash
3. `pyocd cmd -t cortex_m -c "reset"` 重置 MCU，從新的 SPI image 開機

> `HELPER_ENTRY` (`0x0cc72D`) 對應目前的 `tools/app_flash/zephyr.bin`，若重新編譯 RAM flasher 需更新此值。

Log (步驟 1 手動執行):
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

Log (`source tools/script/release.sh`，節錄):
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

> `[警告]` 為預期訊息: `bootloader.bin` 是完整 512KB SPI image，實際資料只到 `0xFEFF`，截斷 `0x11000` 之後的 0xFF 不影響內容。

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

