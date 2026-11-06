# EC fan LUT test

對應檔案:[app/service/fan/service.c](../app/service/fan/service.c) /
[app/driver/fan.c](../app/driver/fan.c)

規格參考:

- 80-35782-88 Rev. AB (Windows Mobile Compute EC Feature Acceptance Test Specification) 2.12 FAN LUT test
- 80-35782-87 Rev. AC (Windows Mobile Compute EC Interface Specification) `0x24` / `0x26` / `0x27` / `0x28`

## 測試結果摘要

| 項目 | 結果 |
|------|------|
| 整體結果 | **PASS** |
| 支援的 Profile 數量 | 7(`0x26` 回 7) |
| Profile 1–3 | 未定義 LUT,Host 判定 `Not support any LUT`,跳過 |
| Profile 4–7 LUT1(SoC Tj) | Fan1 Step up **PASS**(max Level 5)、Step Down **PASS** |
| Profile 4–7 LUT2(Tskin) | Fan1 Step up **PASS**(max Level 5)、Step Down **PASS** |
| 測試項目數 | 8 組(4 個 Profile × 2 個 LUT),全部 PASS |

## 測試環境

| 項目 | 內容 |
|------|------|
| Board | `qual_module/mec172x_nsz` |
| RTOS | Zephyr OS v3.7.1 |
| Host 工具 | UEFI Shell `ECTA3013.efi -fst fan_lut_test`(ECTestApp) |
| 測試日期 | 2026-11-05(log 檔時間) |
| 測試參數 | 全部使用預設值(見下表) |
| 前置條件(規格) | Fan Power constraint 已解除、Thermal Telemetry 關閉、Tskin Test config 開啟 |

| 參數 | 說明 | 值 |
|------|------|----|
| `-ft` | Fan 反應時間 | 2000 ms |
| `-dt1` | 等待 Fan 到達目標 RPM | 1000 ms |
| `-dt2` | 讀取所有 pending SCI 的等待時間 | 1000 ms |
| `-dt3` | 重讀 RPM 的等待時間 | 2000 ms |
| `-rt` | RPM 容許誤差 | 100 RPM |

## 測試流程

| # | Host 動作 | EC 行為 | 結果 |
|---|-----------|---------|------|
| 1 | 讀取支援的 Profile 數量(`0x26`) | [acpi_ec_fan_profile_num()](../app/service/fan/service.c#L418) 回 7 | Total Profile Numbers = 7 |
| 2 | 對 Profile 1–7 逐一讀取 LUT 數量(`0x27`) | [acpi_ec_fan_lut_num()](../app/service/fan/service.c#L488) | Profile 1–3 無 LUT,Profile 4–7 有 LUT1、LUT2 |
| 3 | 切換 Profile(`0x24`)並讀取 LUT(`0x28`) | 回傳 [fan_profiles](../app/driver/fan.c#L77) 中的表 | 成功 |
| 4 | Step up:依 LUT 逐級設定溫度,確認 RPM 跟著上升 | [check_fan_rpm()](../app/service/fan/service.c#L132) 依溫度查表,取 SoC Tj / Tskin 兩者中較高的 RPM | PASS(max Level 5) |
| 5 | Step down:逐級降溫,確認 RPM 跟著下降 | 同上 | PASS |

### EC 中的 LUT

Profile 4–7 的 Fan1 SoC Tj(LUT1)與 Tskin(LUT2)內容相同:

| Level | tRPM | RPM | TmpU (°C) | TmpD (°C) |
|-------|------|-----|-----------|-----------|
| Lv00 | `0x19` | 2500 | `0x4A` (74) | `0x00` (0) |
| Lv01 | `0x23` | 3500 | `0x4D` (77) | `0x47` (71) |
| Lv02 | `0x2D` | 4500 | `0x50` (80) | `0x4A` (74) |
| Lv03 | `0x37` | 5500 | `0x53` (83) | `0x4D` (77) |
| Lv04 | `0x41` | 6500 | `0x55` (85) | `0x50` (80) |
| Lv05 | `0x4B` | 7500 | `0xFF` | `0x53` (83) |
| Lv06–Lv09 | `0x4B` | 7500 | `0xFF` | `0xFF` |

Lv05 之後的 entry 都是 `0xFF` 填充,有效的最高 Level 是 Lv05,與 Host 對 LUT1、LUT2 回報的 `max Level 5` 一致。

Fan2 的 LUT(SoC Tj 與 Tskin 相同)有效到 Lv07(`0x64` = 10000 RPM,TmpD 64 °C)。Host 只測 Fan1,所以這次沒有驗證 Fan2 的 LUT。

Profile 1–3(Battery saver、Better battery AC/DC)在 [app/driver/fan.c](../app/driver/fan.c#L77) 中沒有定義 LUT,屬於目前的設計,與規格 sample output 的行為相同。

## 備註

- `Step Down Test PASS` 後面的 `500` 是 console 殘留字元,不影響判定。
- Profile 5 LUT2 Step Down 那一行被 shell prompt 覆蓋成 `FS5:\> T2 Step Down Test PASS`,原本應是 `   - LUT2 Step Down Test PASS`,判定為 PASS。
- 這次只有 UEFI log,沒有 EC log。

---

## UEFI log

<details>
<summary>點擊展開</summary>

```shell
FS5:\> ECTA3013.efi -fst fan_lut_test
[152566] - Loading driver at 0x000B6B9F000 EntryPoint=0x000B6BB6570 ECTestApp.efi
[WOS-EC E] [InsufficientArgCheck 51] Insufficient Arguments!
[WOS-EC E] [NextArgStrcmp 146] Insufficient String value!
[WOS-EC E] [CmpStrAndGetDecimal 216] Please specify [-ft]
[WOS-EC W] [ECTestAppMain 478]  Setting -ft to Default Value 2000(DEF_FAN_RESP)! It must be >=1000(MIN_FAN_RESP) and < 5001(MAX_FAN_RESP)
[WOS-EC E] [InsufficientArgCheck 51] Insufficient Arguments!
[WOS-EC E] [NextArgStrcmp 146] Insufficient String value!
[WOS-EC E] [CmpStrAndGetDecimal 216] Please specify [-dt1]
[WOS-EC W] [ECTestAppMain 479]  Setting -dt1 to Default Value 1000(DEF_DELAY_T1)! It must be >=1000(MIN_DELAY_T1) and < 4001(MAX_DELAY_T1)
[WOS-EC E] [InsufficientArgCheck 51] Insufficient Arguments!
[WOS-EC E] [NextArgStrcmp 146] Insufficient String value!
[WOS-EC E] [CmpStrAndGetDecimal 216] Please specify [-dt2]
[WOS-EC W] [ECTestAppMain 480]  Setting -dt2 to Default Value 1000(DEF_DELAY_T2)! It must be >=1000(MIN_DELAY_T2) and < 5001(MAX_DELAY_T2)
[WOS-EC E] [InsufficientArgCheck 51] Insufficient Arguments!
[WOS-EC E] [NextArgStrcmp 146] Insufficient String value!
[WOS-EC E] [CmpStrAndGetDecimal 216] Please specify [-dt3]
[WOS-EC W] [ECTestAppMain 481]  Setting -dt3 to Default Value 2000(DEF_DELAY_T3)! It must be >=1000(MIN_DELAY_T3) and < 5001(MAX_DELAY_T3)
[WOS-EC E] [InsufficientArgCheck 51] Insufficient Arguments!
[WOS-EC E] [NextArgStrcmp 146] Insufficient String value!
[WOS-EC E] [CmpStrAndGetDecimal 216] Please specify [-rt]
[WOS-EC W] [ECTestAppMain 482]  Setting -rt to Default Value 100(RPM_TOLERANCE)! It must be >=100(RPM_100) and < 1001(MAX_RPM_TOLERANCE)
[WOS-EC I] [FanLutTest 1750] Testing Fan LUT Test...
[WOS-EC I] [FanLutTest 1760]  - Reading the numbers of the supported Profiles
[WOS-EC I] [FanLutTest 1770]  - Total Profile Numbers = 7
[WOS-EC I] [FanLutTest 1777]  - Testing Profile ID : 1
[WOS-EC I] [FanLutTest 1859]   - Not support any LUT in Profile 1
[WOS-EC I] [FanLutTest 1777]  - Testing Profile ID : 2
[WOS-EC I] [FanLutTest 1859]   - Not support any LUT in Profile 2
[WOS-EC I] [FanLutTest 1777]  - Testing Profile ID : 3
[WOS-EC I] [FanLutTest 1859]   - Not support any LUT in Profile 3
[WOS-EC I] [FanLutTest 1777]  - Testing Profile ID : 4
[WOS-EC I] [FanLutTest 1936]   - Profile 4 LUT1 support in Fan1
   - LUT1 Step up Test PASS(max Level 5)
   - LUT1 Step Down Test PASS                 500
[WOS-EC I] [FanLutTest 1936]   - Profile 4 LUT2 support in Fan1
   - LUT2 Step up Test PASS(max Level 5)
   - LUT2 Step Down Test PASS                 500
[WOS-EC I] [FanLutTest 1777]  - Testing Profile ID : 5
[WOS-EC I] [FanLutTest 1936]   - Profile 5 LUT1 support in Fan1
   - LUT1 Step up Test PASS(max Level 5)
   - LUT1 Step Down Test PASS                 500
[WOS-EC I] [FanLutTest 1936]   - Profile 5 LUT2 support in Fan1
   - LUT2 Step up Test PASS(max Level 5)
FS5:\> T2 Step Down Test PASS                 500
[WOS-EC I] [FanLutTest 1777]  - Testing Profile ID : 6
[WOS-EC I] [FanLutTest 1936]   - Profile 6 LUT1 support in Fan1
   - LUT1 Step up Test PASS(max Level 5)
   - LUT1 Step Down Test PASS                 500
[WOS-EC I] [FanLutTest 1936]   - Profile 6 LUT2 support in Fan1
   - LUT2 Step up Test PASS(max Level 5)
   - LUT2 Step Down Test PASS                 500
[WOS-EC I] [FanLutTest 1777]  - Testing Profile ID : 7
[WOS-EC I] [FanLutTest 1936]   - Profile 7 LUT1 support in Fan1
   - LUT1 Step up Test PASS(max Level 5)
   - LUT1 Step Down Test PASS                 500
[WOS-EC I] [FanLutTest 1936]   - Profile 7 LUT2 support in Fan1
   - LUT2 Step up Test PASS(max Level 5)
   - LUT2 Step Down Test PASS                 500
[WOS-EC I] [FanLutTest 2029] Done Testing Fan LUT Test!
```

</details>
