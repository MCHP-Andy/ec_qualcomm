# EC firmware update test

對應檔案:[bootloader/service/acpi/ec_fw_update.c](../bootloader/service/acpi/ec_fw_update.c) /
[bootloader/service/flash/service.c](../bootloader/service/flash/service.c)

規格參考:80-35782-87 Rev. AC (Windows Mobile Compute EC Interface Specification)

## 測試結果摘要

| 項目 | 結果 |
|------|------|
| 整體結果 | **PASS**(有 1 項已知問題,見[已知問題](#已知問題)) |
| 更新前 EC FW 版本 | 1.1.1(CRC `0xFAA57716`) |
| 更新後 EC FW 版本 | 0.1.13(CRC `0x8F8D4819`) |
| Main Block 抹除 | 50 × 4KB,成功 |
| Main Block 寫入 | 400 × 512B = 204,800 bytes(`0x011000`–`0x042FFF`),成功 |
| CRC 驗證 | EC 計算 `0x8F8D4819` = 預期 `0x8F8D4819`,Host 端 CRC Passed |
| NV 區抹除 | **FAIL**:EC 回 `-22`,Host 端未偵測到錯誤 |
| 更新後回到 Main Block | 成功,並正常處理 ACPI cmd `0xEC` |
| 燒錄耗時 | 約 17.0 秒(93,061 ms → 110,109 ms,ACPI FW Update Start → End) |

## 測試環境

| 項目 | 內容 |
|------|------|
| Board | `qual_module/mec172x_nsz`(MEC172x 內部 eFlash) |
| RTOS | Zephyr OS v3.7.1 |
| Bootloader | BBK V(Boot Block Version `0x3700A5`) |
| Host 工具 | UEFI Shell `ECFlashApp.efi -F -U ec_image.bin` |
| 測試 image | `ec_image.bin`,524,288 bytes(Main Block header 位於 offset `0x11000`) |
| 測試 image 內容 | FW 0.1.13,Size 204,768 bytes,Lowest Supported Version 0.1.0,Build `Oct 15 2026 12:09:21` |
| I2C Slave Address | `0x76` |
| Write Protection | DT 中已關閉(GPIO 38 = 0) |

## 更新流程

對照 Host(UEFI)與 EC 的 log,整個流程如下:

| # | Host 動作 | EC 行為 | 結果 |
|---|-----------|---------|------|
| 1 | 讀取 bin 版本 (0.1.13) 與 IC 版本 (1.1.1) | Main Block 執行中 | 版本不同,需要更新 |
| 2 | 要求進入 Boot Block | 重開機進入 bootloader(`mode=0xDC`),`Stay in bootloader mode` | 成功 ※1 |
| 3 | `EcEnableFlashing`,檢查 Control Path | SPI Service Sub-Command `0xDC`,Control Path `0x4F`(內部 eFlash) | 成功 |
| 4 | 抹除 Main Block(`0xAE`,Host addr `0x8000`) | Erase `0x011000`,204,800 bytes | 成功 |
| 5 | 抹除 NV 區(`0xAE`) | Erase `0x0F9000`,4,096 bytes → `-22` | **失敗** ※2 |
| 6 | 寫入 400 × 512B | 每次 Stage 到 RAM buffer 後 Commit 至 Flash | 成功 |
| 7 | `EcRunCRC` / `WaitForEcCRCResult` | Sub-Command `0xF0`,CRC 比對一致,Read CRC Status `0xAC` | 成功 |
| 8 | 等待回到 Main Block | Sub-Command `0x33`,驗證 Main Block 後跳到 `0x000D0000` | 成功 |
| 9 | `TriggerECReset`:再次進 BBK | Sub-Command `0xCC`,Software EC Reset | 成功 |
| 10 | 等待回到 Main Block,結束 | 載入 0.1.13,`Handled ACPI cmd 0xec successfully` | `EC Firmware Updated Successfully` |

※1 Host 在步驟 2 有 `WaitForEcSpiService() failed Device Error, ignore and continue`(SPI service status `0xFF`,retry 5 次後逾時)。這段時間 EC 正在重開機,Host 讀到的狀態都是 `0xFF`(Unknown Block),之後 Host 繼續流程,EC 也已在 Boot Block,不影響結果。若希望 log 乾淨,可以考慮讓 Host 拉長 retry 時間,或讓 EC 更早回報狀態。

※2 見下方已知問題。

## 已知問題

### 1. NV 區抹除失敗 (`0xAE` 回 `-22`)

EC log:

```shell
I: Erase Region: Path=0x4F, StartAddr=0x0F9000, Blocks=4096
E: Flash command 2 failed at 0x000f9000, size 4096: -22
E: Failed to erase flash region: -22
E: Failed to handle ACPI cmd 0xae: -22
```

Host log 卻顯示成功:

```shell
[WOS-EC I] [EcEraseNVFlashRegion 1463] EcI2cBlockWrite() for EC_ERASE_DESIGNATED_FLASH_AREA_CMD returned Success
[WOS-EC W] [EcEraseNVFlashRegion 1477] NV blk(4KB) 1 / 1 erased
```

- 推測原因:[acpi_ec_erase_designated_region()](../bootloader/service/acpi/ec_fw_update.c#L177) 一律把 Host 位址加上 `0x9000`。Host 送來的 NV 位址是 `0x0F0000`,換算後變成 `0x0F9000`,超出 MEC172x 內部 eFlash 範圍,`flash_erase()` 因此回 `-EINVAL`。
- 第二個問題:EC 處理失敗,但 Host 只確認 I2C 寫入成功,之後 EC 狀態仍回到 Idle,所以 Host 判斷為已抹除。EC 應該在狀態或 response 中回報錯誤。
- 影響:這次測試 NV 區沒有被抹除,但 Main Block 更新與開機都正常。

---

## UEFI log

<details>
<summary>點擊展開</summary>

```shell
FS5:\> ls
Directory of: FS5:\
01/06/1980  01:04             163,840  ECFlashApp.efi
01/06/1980  01:04             163,840  ECTestApp.efi
10/15/2026  04:45             524,288  ec_image.bin
01/24/2024  06:53 <DIR>           512  EFI
01/24/2024  06:53                 829  ENROLL_THIS_KEY_IN_MOKMANAGER.cer
01/24/2024  06:53 <DIR>         1,536  grub
01/24/2024  06:53 <DIR>           512  tool
01/24/2024  06:53 <DIR>         3,072  ventoy
          4 File(s)     852,797 bytes
          4 Dir(s)
FS5:\> 
FS5:\> 
FS5:\> 
FS5:\> 
FS5:\> 
FS5:\> 
FS5:\> 
FS5:\> 
FS5:\> ECFlashApp.efi -F -U ec_image.bin
[92059] - Loading driver at 0x000B66EF000 EntryPoint=0x000B66FE1C0 ECFlashApp.efi
[WOS-EC E] [ECFlashAppMain 1477] Bin File Name: ec_image.bin
[WOS-EC I] [ReadFileToBuffer 180] Bin File Size: 524288
[WOS-EC I] [ReadBinToBuff 893] FW binary is copied properly
[WOS-EC I] [SearchECVersionTag 225] File Size 0x80000
[WOS-EC W] [SearchECVersionTag 242] BootBlock VersionTag not Detected, Skipping to check MainBlock VersionTag
[WOS-EC I] [SearchECVersionTag 253] Main Block Matched (Offset : 0x11000) 
[WOS-EC I] [CheckBinVersion 927] ========On Bin EC FW Version=======
[WOS-EC I] [CheckBinVersion 928] MainVersion = 0 
[WOS-EC I] [CheckBinVersion 929] SubVersion  = 1 
[WOS-EC I] [CheckBinVersion 930] TestVersion = 13 
[WOS-EC I] [CheckBinVersion 931] ===============================

[WOS-EC I] [GetOnBinaryEcFwSizeFromHeader 485] EC Firmware is valid!
[WOS-EC E] [FlashWriteProtectHandler 2316] EC Write-Protection Feature is Disabled in DT
[WOS-EC I] [EcGetTLMMGpio 299] Current value of GPIO: 38 is 0x0
[WOS-EC I] [FlashWriteProtectHandler 2332] Skipped configuring as EC Write-Protection is already Disabled!
[WOS-EC I] [WaitForEcSpiService 680] EC is running in Main Block!
[WOS-EC I] [ECFlashAppMain 1569] Slave Address: 0x76
[WOS-EC I] [GetECFirmwareVersion 597] ========On IC EC FW Version=======
[WOS-EC I] [GetECFirmwareVersion 598] MainVersion = 1 
[WOS-EC I] [GetECFirmwareVersion 599] SubVersion  = 1 
[WOS-EC I] [GetECFirmwareVersion 600] TestVersion = 1 
[WOS-EC I] [GetECFirmwareVersion 601] ===============================

[WOS-EC I] [IsFwVersionDiff 253] Both FW's are not same!!
[WOS-EC I] [ECFlashAppMain 1577] ========FW Version Mismatch=======
[WOS-EC I] [ECFlashAppMain 1582] FW MainVersion [IC=1,  BIN=0] 
[WOS-EC I] [ECFlashAppMain 1587] FW SubVersion  [IC=1,  BIN=1]
[WOS-EC I] [ECFlashAppMain 1592] FW TestVersion [IC=1,  BIN=13]
[WOS-EC I] [ECFlashAppMain 1593] ===================================
[WOS-EC E] [ECFlashAppMain 1594] Needs EC FW Upgrade!!
[WOS-EC W] [GetEcBootBlockVersion 2183] Not in BBK, attempting to enter into BBK
[WOS-EC I] [WaitForEnterIntoBootBlock 886] Waiting for EC Firmware to enter into Boot Block
[WOS-EC I] [WaitForEcSpiService 680] EC is running in Main Block!
[WOS-EC I] [WaitForEnterIntoBootBlock 886] Waiting for EC Firmware to enter into Boot Block
[WOS-EC I] [WaitForEcSpiService 697] EC is running in Unknown Block!
[WOS-EC W] [WaitForEcSpiService 705] EcI2cBlockRead status : Success retry : 1
[WOS-EC I] [WaitForEcSpiService 697] EC is running in Unknown Block!
[WOS-EC W] [WaitForEcSpiService 705] EcI2cBlockRead status : Success retry : 2
[WOS-EC I] [WaitForEcSpiService 697] EC is running in Unknown Block!
[WOS-EC W] [WaitForEcSpiService 705] EcI2cBlockRead status : Success retry : 3
[WOS-EC I] [WaitForEcSpiService 697] EC is running in Unknown Block!
[WOS-EC W] [WaitForEcSpiService 705] EcI2cBlockRead status : Success retry : 4
[WOS-EC I] [WaitForEcSpiService 697] EC is running in Unknown Block!
[WOS-EC W] [WaitForEcSpiService 705] EcI2cBlockRead status : Success retry : 5
[WOS-EC W] [WaitForEcSpiService 711] EC SPI service is busy and timed out,       ec_spi_service_status = 0xFF
[WOS-EC E] [WaitForEnterIntoBootBlock 894] WaitForEcSpiService() failed Device Error, ignore and continue
[WOS-EC I] [GetEcBootBlockVersion 2235] EC Boot Block Version: 0x3700A5
[WOS-EC I] [ECFlashAppMain 1611] Programming FLASH.....
[WOS-EC I] [ECReflash 1601] ACPI FW Update Start. Time (ms): 93061
ScmArmV8SipSysCall:  SmcId 30000001  ExecTime:   386 ms
Successfully synced all UEFI tables
[WOS-EC W] [EcReflashFirmware 1133] Active Protocol Status: 1 
[WOS-EC I] [EcParseFwImage 283] Check: bbk[0x10], mbk[0x45], fw[0]:0x10, fw[0x2000]:0xC3
[WOS-EC E] [FlashWriteProtectHandler 2316] EC Write-Protection Feature is Disabled in DT
[WOS-EC I] [EcGetTLMMGpio 299] Current value of GPIO: 38 is 0x0
[WOS-EC I] [FlashWriteProtectHandler 2332] Skipped configuring as EC Write-Protection is already Disabled!
[WOS-EC W] [WaitForEcSpiService 676] EC is running in Boot Block!
[WOS-EC W] [WaitForEcSpiService 676] EC is running in Boot Block!
[WOS-EC W] [EcEnableFlashing 1063] EC is running in Boot Block!
[WOS-EC W] [WaitForEcSpiService 676] EC is running in Boot Block!
[WOS-EC W] [EcEnableFlashing 1094] status = Success
[WOS-EC I] [EcSelectSpiFlashEraseSize 319] Select Internal SPI eFlash
[WOS-EC I] [EcEraseSpiFlash 527] EC is in Boot Block
[WOS-EC I] [EcEraseSpiFlash 583] Erasing sector [0x4] from start[0x8000] --- end[0x39FFF] with erase_sector_size:4096
[WOS-EC I] [WaitForEnterIntoBootBlock 886] Waiting for EC Firmware to enter into Boot Block
[WOS-EC W] [WaitForEcSpiService 676] EC is running in Boot Block!
[WOS-EC W] [WaitForEcSpiService 676] EC is running in Boot Block!
[WOS-EC W] [EcCheckControlPath 461] EC response [B,4F,0]
[WOS-EC I] [EcCheckControlPath 464] Control path is in Internal eFlash!
[WOS-EC I] [EcEraseSpiFlash 595] ERASE_INT_FLASH_MBK_CMD
[WOS-EC I] [EcEraseDesignatedFlashRegion 1409] EcI2cBlockWrite() for EC_ERASE_DESIGNATED_FLASH_AREA_CMD returned Success
[WOS-EC W] [WaitForEcSpiService 676] EC is running in Boot Block!
[WOS-EC W] [EcEraseSpiFlash 610] blk(4KB) 50 / 50 erased
[WOS-EC I] [EcEraseNVFlashRegion 1463] EcI2cBlockWrite() for EC_ERASE_DESIGNATED_FLASH_AREA_CMD returned Success
[WOS-EC W] [WaitForEcSpiService 676] EC is running in Boot Block!
[WOS-EC W] [EcEraseNVFlashRegion 1477] NV blk(4KB) 1 / 1 erased
[WOS-EC I] [EcWriteSpiFlash 660] Total block count 400, each of 512 bytes to be flashed
[WOS-EC W] [WaitForEcSpiService 676] EC is running in Boot Block!
[WOS-EC W] [EcWriteSpiFlash 672] deviceContext->flash_type: 4
 blk(512B) 384 / 400 written[WOS-EC W] [EcFinalizeReflash 881] Partition Type: 4
[WOS-EC W] [EcFinalizeReflash 887] WaitForEcSpiService()...
[WOS-EC W] [WaitForEcSpiService 676] EC is running in Boot Block!
[WOS-EC W] [EcFinalizeReflash 908] EcRunCRC()...
[WOS-EC W] [EcFinalizeReflash 912] WaitForEcSpiService()...
[WOS-EC I] [WaitForEcSpiService 697] EC is running in Unknown Block!
[WOS-EC W] [WaitForEcSpiService 705] EcI2cBlockRead status : Success retry : 1
[WOS-EC I] [WaitForEcSpiService 697] EC is running in Unknown Block!
[WOS-EC W] [WaitForEcSpiService 705] EcI2cBlockRead status : Success retry : 2
[WOS-EC W] [WaitForEcSpiService 676] EC is running in Boot Block!
[WOS-EC I] [WaitForEcCRCResult 352]  Wait for EC CRC Read
[WOS-EC W] [WaitForEcCRCResult 382] EC CRC Passed, 32bit CRC[0,1,2,3]:[0x19, 0x48, 0x8D, 0x8F]
[WOS-EC I] [WaitForEnterIntoMainBlock 842] Waiting for EC Firmware to enter into Main Block
[WOS-EC W] [WaitForEcSpiService 676] EC is running in Boot Block!
[WOS-EC I] [WaitForEnterIntoMainBlock 842] Waiting for EC Firmware to enter into Main Block
[WOS-EC I] [WaitForEcSpiService 697] EC is running in Unknown Block!
[WOS-EC W] [WaitForEcSpiService 705] EcI2cBlockRead status : Success retry : 1
[WOS-EC I] [WaitForEcSpiService 697] EC is running in Unknown Block!
[WOS-EC W] [WaitForEcSpiService 705] EcI2cBlockRead status : Success retry : 2
[WOS-EC I] [WaitForEcSpiService 697] EC is running in Unknown Block!
[WOS-EC W] [WaitForEcSpiService 705] EcI2cBlockRead status : Success retry : 3
[WOS-EC I] [WaitForEcSpiService 697] EC is running in Unknown Block!
[WOS-EC W] [WaitForEcSpiService 705] EcI2cBlockRead status : Success retry : 4
[WOS-EC I] [WaitForEcSpiService 680] EC is running in Main Block!
[WOS-EC W] [EcReflashFirmware 1152] status = Success
[WOS-EC I] [ECReflash 1610] ACPI FW Update End. Time (ms): 110109
[WOS-EC W] [TriggerECReset 1027] Entering BBK...
[WOS-EC I] [WaitForEnterIntoBootBlock 886] Waiting for EC Firmware to enter into Boot Block
[WOS-EC I] [WaitForEcSpiService 680] EC is running in Main Block!
[WOS-EC I] [WaitForEnterIntoBootBlock 886] Waiting for EC Firmware to enter into Boot Block
[WOS-EC I] [WaitForEcSpiService 697] EC is running in Unknown Block!
[WOS-EC W] [WaitForEcSpiService 705] EcI2cBlockRead status : Success retry : 1
[WOS-EC I] [WaitForEcSpiService 697] EC is running in Unknown Block!
[WOS-EC W] [WaitForEcSpiService 705] EcI2cBlockRead status : Success retry : 2
[WOS-EC I] [WaitForEcSpiService 697] EC is running in Unknown Block!
[WOS-EC W] [WaitForEcSpiService 705] EcI2cBlockRead status : Success retry : 3
[WOS-EC I] [WaitForEcSpiService 697] EC is running in Unknown Block!
[WOS-EC W] [WaitForEcSpiService 705] EcI2cBlockRead status : Success retry : 4
[WOS-EC W] [WaitForEcSpiService 676] EC is running in Boot Block!
[WOS-EC I] [WaitForEcSpiService 697] EC is running in Unknown Block!
[WOS-EC W] [WaitForEcSpiService 705] EcI2cBlockRead status : Success retry : 1
[WOS-EC I] [WaitForEcSpiService 697] EC is running in Unknown Block!
[WOS-EC W] [WaitForEcSpiService 705] EcI2cBlockRead status : Success retry : 2
[WOS-EC I] [WaitForEcSpiService 697] EC is running in Unknown Block!
[WOS-EC W] [WaitForEcSpiService 705] EcI2cBlockRead status : Success retry : 3
[WOS-EC I] [WaitForEcSpiService 697] EC is running in Unknown Block!
[WOS-EC W] [WaitForEcSpiService 705] EcI2cBlockRead status : Success retry : 4
[WOS-EC I] [WaitForEcSpiService 680] EC is running in Main Block!
 ################################################################
 #          EC Firmware Updated Successfully                    #
 ################################################################
[WOS-EC E] [FlashWriteProtectHandler 2316] EC Write-Protection Feature is Disabled in DT
[WOS-EC I] [EcGetTLMMGpio 299] Current value of GPIO: 38 is 0x0
[WOS-EC I] [FlashWriteProtectHandler 2332] Skipped configuring as EC Write-Protection is already Disabled!
[WOS-EC I] [EcDeInitializeDevice 148] EC Device DeInitialized!
FS5:\> 
```

</details>

## EC log

<details>
<summary>點擊展開</summary>

```shell
*** Booting Zephyr OS build v3.7.1 ***
W: Retention data is invalid, initializing...
I: Hello from bootloader: BBK V
I: Header address in: 0x00011000
I: App address in: 0x000d0000
I: Flash Address: 0x00011020
I: EC Load Address: 0x0D0000
I: EC Firmware Size: 204768 bytes
I: EC Firmware Version: 0.1.13
I: EC Validation Control: 0x01
I: EC Lowest Supported Version: 0.1.0
I: Load image: Flash 0x00011020 -> RAM 0x000D0000, size 204768 bytes
Prepare jump to  0x000d0000...
New MSP: 0x0011e440, Entry: 0x000d82ad


*** Booting Zephyr OS build v3.7.1 ***
[00:00:00.001,159] <inf> btn: Set up button at gpio@40081180 pin 1, level: 1
[00:00:00.001,159] <inf> fan_dev: Initializing fan...
[00:00:00.001,220] <inf> fan_dev: Fan constraint: FAN(s) can only stay OFF
[00:00:00.001,220] <err> fan_dev: fan0 not ready
[00:00:00.001,251] <err> fan_dev: tach0 not ready
[00:00:00.001,281] <inf> therm_dev: Initializing thermal sensor...
[00:00:00.001,281] <err> therm_dev: ADC device for channel 0 not ready
[00:00:00.003,692] <inf> storage: Boot info: mode=0x00 reset=0x01 fail=0 corrupt=0x85 crc=0xAC/0xFAA57716
[00:00:00.004,211] <inf> main: Build time : Oct 15 2026 15:21:10
[00:00:00.004,241] <inf> main: Hello: qual_module/mec172x_nsz
uart:~$ 
uart:~$ 
uart:~$ 
uart:~$ *** Booting Zephyr OS build v3.7.1 ***
I: Boot info: mode=0xDC reset=0x05 fail=0 corrupt=0x85 crc=0xAC/0xFAA57716
I: Hello from bootloader: BBK V
I: Header address in: 0x00011000
I: App address in: 0x000d0000
I: Flash Address: 0x00011020
I: EC State: 0x0B, Control Path: 0x4F
I: EC State: 0xFF, Control Path: 0x4F
I: EC State: 0xFF, Control Path: 0x4F
I: EC State: 0xFF, Control Path: 0x4F
I: EC State: 0xFF, Control Path: 0x4F
I: EC Load Address: 0x0D0000
I: EC Firmware Size: 204768 bytes
I: EC Firmware Version: 0.1.13
I: EC Validation Control: 0x01
I: EC Lowest Supported Version: 0.1.0
I: Stay in bootloader mode, skipping jump to app


uart:~$ I: EC State: 0x0B, Control Path: 0x4F
I: EC State: 0x0B, Control Path: 0x4F
I: EC State: 0x0B, Control Path: 0x4F
I: SPI Service Request, Sub-Command: 0xDC
I: EC entered Bootloader execution mode
I: EC State: 0x0B, Control Path: 0x4F
I: EC State: 0x0B, Control Path: 0x4F
I: EC State: 0x0B, Control Path: 0x4F
I: Erase Region: Path=0x4F, StartAddr=0x011000, Blocks=204800
I: EC State: 0x0B, Control Path: 0x4F
I: Erase Region: Path=0x4F, StartAddr=0x0F9000, Blocks=4096
E: Flash command 2 failed at 0x000f9000, size 4096: -22
E: Failed to erase flash region: -22
E: Failed to handle ACPI cmd 0xae: -22
I: EC State: 0x0B, Control Path: 0x4F
I: EC State: 0x0B, Control Path: 0x4F
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x011000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x011200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x011400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x011600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x011800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x011A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x011C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x011E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x012000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x012200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x012400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x012600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x012800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x012A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x012C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x012E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x013000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x013200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x013400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x013600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x013800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x013A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x013C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x013E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x014000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x014200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x014400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x014600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x014800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x014A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x014C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x014E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x015000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x015200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x015400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x015600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x015800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x015A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x015C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x015E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x016000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x016200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x016400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x016600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x016800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x016A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x016C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x016E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x017000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x017200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x017400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x017600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x017800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x017A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x017C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x017E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x018000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x018200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x018400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x018600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x018800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x018A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x018C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x018E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x019000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x019200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x019400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x019600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x019800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x019A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x019C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x019E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01A000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01A200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01A400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01A600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01A800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01AA00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01AC00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01AE00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01B000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01B200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01B400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01B600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01B800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01BA00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01BC00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01BE00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01C000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01C200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01C400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01C600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01C800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01CA00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01CC00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01CE00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01D000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01D200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01D400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01D600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01D800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01DA00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01DC00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01DE00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01E000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01E200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01E400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01E600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01E800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01EA00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01EC00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01EE00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01F000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01F200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01F400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01F600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01F800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01FA00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01FC00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x01FE00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x020000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x020200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x020400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x020600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x020800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x020A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x020C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x020E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x021000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x021200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x021400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x021600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x021800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x021A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x021C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x021E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x022000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x022200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x022400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x022600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x022800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x022A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x022C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x022E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x023000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x023200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x023400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x023600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x023800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x023A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x023C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x023E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x024000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x024200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x024400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x024600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x024800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x024A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x024C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x024E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x025000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x025200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x025400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x025600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x025800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x025A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x025C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x025E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x026000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x026200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x026400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x026600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x026800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x026A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x026C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x026E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x027000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x027200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x027400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x027600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x027800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x027A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x027C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x027E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x028000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x028200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x028400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x028600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x028800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x028A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x028C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x028E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x029000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x029200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x029400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x029600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x029800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x029A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x029C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x029E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02A000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02A200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02A400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02A600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02A800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02AA00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02AC00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02AE00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02B000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02B200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02B400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02B600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02B800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02BA00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02BC00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02BE00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02C000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02C200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02C400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02C600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02C800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02CA00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02CC00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02CE00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02D000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02D200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02D400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02D600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02D800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02DA00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02DC00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02DE00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02E000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02E200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02E400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02E600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02E800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02EA00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02EC00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02EE00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02F000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02F200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02F400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02F600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02F800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02FA00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02FC00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x02FE00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x030000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x030200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x030400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x030600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x030800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x030A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x030C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x030E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x031000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x031200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x031400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x031600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x031800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x031A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x031C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x031E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x032000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x032200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x032400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x032600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x032800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x032A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x032C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x032E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x033000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x033200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x033400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x033600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x033800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x033A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x033C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x033E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x034000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x034200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x034400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x034600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x034800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x034A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x034C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x034E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x035000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x035200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x035400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x035600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x035800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x035A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x035C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x035E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x036000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x036200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x036400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x036600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x036800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x036A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x036C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x036E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x037000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x037200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x037400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x037600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x037800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x037A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x037C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x037E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x038000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x038200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x038400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x038600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x038800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x038A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x038C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x038E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x039000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x039200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x039400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x039600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x039800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x039A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x039C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x039E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03A000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03A200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03A400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03A600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03A800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03AA00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03AC00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03AE00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03B000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03B200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03B400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03B600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03B800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03BA00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03BC00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03BE00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03C000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03C200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03C400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03C600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03C800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03CA00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03CC00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03CE00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03D000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03D200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03D400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03D600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03D800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03DA00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03DC00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03DE00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03E000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03E200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03E400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03E600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03E800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03EA00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03EC00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03EE00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03F000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03F200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03F400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03F600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03F800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03FA00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03FC00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x03FE00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x040000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x040200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x040400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x040600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x040800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x040A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x040C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x040E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x041000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x041200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x041400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x041600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x041800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x041A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x041C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x041E00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x042000, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x042200, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x042400, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x042600, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x042800, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x042A00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x042C00, Len=512 bytes
I: Staged 512 bytes into RAM buffer
I: Commit RAM buffer to Flash: Path=0x4F, Addr=0x042E00, Len=512 bytes
I: EC State: 0x0B, Control Path: 0x4F
I: SPI Service Request, Sub-Command: 0xF0
I: Triggering Partition CRC Calculation: 0xF0
I: Run CRC: Size=204768, Calculated 0x8F8D4819, Expected 0x8F8D4819
I: EC State: 0x0B, Control Path: 0x4F
I: EC State: 0x0B, Control Path: 0x4F
I: Read CRC: Status=0xAC, CRC=0x8F8D4819
I: SPI Service Request, Sub-Command: 0x33
I: EC exiting Bootloader, returning to Main Block
I: EC State: 0x0B, Control Path: 0x4F
I: Validating Main Block before exiting Bootloader...
I: Flash Address: 0x00011020
I: EC State: 0xFF, Control Path: 0x4F
I: EC State: 0xFF, Control Path: 0x4F
I: EC Load Address: 0x0D0000
I: EC Firmware Size: 204768 bytes
I: EC Firmware Version: 0.1.13
I: EC Validation Control: 0x01
I: EC Lowest Supported Version: 0.1.0
I: Load image: Flash 0x00011020 -> RAM 0x000D0000, size 204768 bytes
I: EC State: 0xFF, Control Path: 0x4F
I: EC State: 0xFF, Control Path: 0x4F
Prepare jump to  0x000d0000...
New MSP: 0x0011e440, Entry: 0x000d82ad


*** Booting Zephyr OS build v3.7.1 ***
[00:00:00.001,159] <inf> btn: Set up button at gpio@40081180 pin 1, level: 1
[00:00:00.001,159] <inf> fan_dev: Initializing fan...
[00:00:00.001,220] <inf> fan_dev: Fan constraint: FAN(s) can only stay OFF
[00:00:00.001,220] <err> fan_dev: fan0 not ready
[00:00:00.001,251] <err> fan_dev: tach0 not ready
[00:00:00.001,251] <inf> therm_dev: Initializing thermal sensor...
[00:00:00.001,281] <err> therm_dev: ADC device for channel 0 not ready
[00:00:00.003,662] <inf> storage: Boot info: mode=0x00 reset=0x05 fail=0 corrupt=0x85 crc=0xAC/0x8F8D4819
[00:00:00.004,211] <inf> main: Build time : Oct 15 2026 12:09:21
[00:00:00.004,241] <inf> main: Hello: qual_module/mec172x_nsz
uart:~$ *** Booting Zephyr OS build v3.7.1 ***
I: Boot info: mode=0xDC reset=0x05 fail=0 corrupt=0x85 crc=0xAC/0x8F8D4819
I: Hello from bootloader: BBK V
I: Header address in: 0x00011000
I: App address in: 0x000d0000
I: Flash Address: 0x00011020
I: EC State: 0xFF, Control Path: 0x4F
I: EC State: 0xFF, Control Path: 0x4F
I: EC State: 0xFF, Control Path: 0x4F
I: EC State: 0xFF, Control Path: 0x4F
I: EC Load Address: 0x0D0000
I: EC Firmware Size: 204768 bytes
I: EC Firmware Version: 0.1.13
I: EC Validation Control: 0x01
I: EC Lowest Supported Version: 0.1.0
I: Stay in bootloader mode, skipping jump to app


uart:~$ I: EC State: 0x0B, Control Path: 0x4F
I: SPI Service Request, Sub-Command: 0xCC
W: Triggering Software EC Reset...
*** Booting Zephyr OS build v3.7.1 ***
W: Retention data is invalid, initializing...
I: Hello from bootloader: BBK V
I: Header address in: 0x00011000
I: App address in: 0x000d0000
I: Flash Address: 0x00011020
I: EC State: 0xFF, Control Path: 0x4F
I: EC State: 0xFF, Control Path: 0x4F
I: EC State: 0xFF, Control Path: 0x4F
I: EC Load Address: 0x0D0000
I: EC Firmware Size: 204768 bytes
I: EC Firmware Version: 0.1.13
I: EC Validation Control: 0x01
I: EC Lowest Supported Version: 0.1.0
I: Load image: Flash 0x00011020 -> RAM 0x000D0000, size 204768 bytes
I: EC State: 0xFF, Control Path: 0x4F
Prepare jump to  0x000d0000...
New MSP: 0x0011e440, Entry: 0x000d82ad


*** Booting Zephyr OS build v3.7.1 ***
[00:00:00.001,159] <inf> btn: Set up button at gpio@40081180 pin 1, level: 1
[00:00:00.001,159] <inf> fan_dev: Initializing fan...
[00:00:00.001,220] <inf> fan_dev: Fan constraint: FAN(s) can only stay OFF
[00:00:00.001,220] <err> fan_dev: fan0 not ready
[00:00:00.001,251] <err> fan_dev: tach0 not ready
[00:00:00.001,281] <inf> therm_dev: Initializing thermal sensor...
[00:00:00.001,281] <err> therm_dev: ADC device for channel 0 not ready
[00:00:00.003,692] <inf> storage: Boot info: mode=0x00 reset=0x01 fail=0 corrupt=0x85 crc=0xAC/0x8F8D4819
[00:00:00.004,211] <inf> main: Build time : Oct 15 2026 12:09:21
[00:00:00.004,241] <inf> main: Hello: qual_module/mec172x_nsz
[00:00:00.010,650] <inf> acpi: Handled ACPI cmd 0xec successfully
uart:~$ 
```

</details>
