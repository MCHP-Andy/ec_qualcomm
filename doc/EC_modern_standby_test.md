# EC modern standby test

對應檔案:[app/service/power/service.c](../app/service/power/service.c) /
[app/driver/power.c](../app/driver/power.c) /
[app/service/fan/service.c](../app/service/fan/service.c) /
[app/service/sci/service.c](../app/service/sci/service.c)

規格參考:

- 80-35782-88 Rev. AB (Windows Mobile Compute EC Feature Acceptance Test Specification) 2.8 Validate FAN Post modern Standby (MSBY)
- 80-35782-87 Rev. AC (Windows Mobile Compute EC Interface Specification) `0x23` SoC to EC Modern Standby notification、`0x05` SCI `0x30` / `0x31`

## 測試結果摘要

| 項目 | 結果 |
|------|------|
| 整體結果 | **PASS** |
| 驗證的 Fan | Fan1、Fan2(`-fnum 2`) |
| MS Entry 前 Fan 狀態 | Fan1 / Fan2 都 spinning |
| MS Entry 後 SCI | 偵測到 `EC_SCI_FAN1_STATUS_CHANGE`、`EC_SCI_FAN2_STATUS_CHANGE` |
| MS Entry 後 Fan 狀態 | Fan1 / Fan2 都 stop |
| MS 期間 SCI | 無 SCI 發出 |
| MS Exit 後 SCI | 偵測到 `EC_SCI_FAN2_STATUS_CHANGE` |
| MS Exit 後 Fan 狀態 | Fan1 / Fan2 都 spinning |

## 測試環境

| 項目 | 內容 |
|------|------|
| Board | `qual_module/mec172x_nsz` |
| RTOS | Zephyr OS v3.7.1 |
| Host 工具 | UEFI Shell `ECTA3013.efi -fst ms_test -fnum 2`(ECTestApp) |
| 測試日期 | 2026-11-05(UEFI log 檔時間) |
| `-fnum` | 2(檢查 Fan1、Fan2) |
| `-scir` | 預設 4000 ms(SCI 等待時間) |
| Fan Profile | Profile 7(Best performance,DC) |
| 前置條件(規格) | Thermal Telemetry 關閉 |

## 測試流程

| # | Host 動作 | EC 行為 | 結果 |
|---|-----------|---------|------|
| 1 | 確認 Fan 為自主模式,清除 pending SCI | 回應 SCI 讀取 | 成功 |
| 2 | 讀 Profile7 Fan1 LUT_T1,設定 T1 = 74 °C,目標 RPM = 3500 | 依 LUT 驅動 Fan1 | 成功 |
| 3 | 讀 Profile7 Fan2 LUT_T2,設定 T2 = 54 °C,目標 RPM = 5000 | 依 LUT 驅動 Fan2 | 成功 |
| 4 | 讀 Fan1 / Fan2 Status / Speed | Fan 轉動中 | spinning,成功 |
| 5 | 送 MS Entry(`0x23`, status=1) | [acpi_soc_to_ec_modern_standby_notifi()](../app/service/power/service.c#L109) → S0 → Modern Standby;[pwr_state_chg()](../app/driver/power.c#L261) 開啟 heavy sleep;Fan constraint 設為 OFF | 成功 |
| 6 | 讀 pending SCI | Fan RPM 變 0 時送出 SCI `0x30` / `0x31` | 偵測到 Fan1、Fan2 SCI |
| 7 | 讀 Fan1 / Fan2 Status / Speed | Fan 已停止 | stop,成功 |
| 8 | 確認 MS 期間沒有 SCI | 沒有 SCI 發出 | 成功 |
| 9 | 送 MS Exit(`0x23`, status=0) | Modern Standby → S0,關閉 heavy sleep;Fan constraint 設為 ON | 成功 |
| 10 | 讀 pending SCI | Fan RPM 由 0 變非 0 時送出 SCI `0x30` / `0x31` | 只偵測到 Fan2 SCI |
| 11 | 讀 Fan1 / Fan2 Status / Speed,清除 pending SCI | Fan 轉動中 | spinning,`Done Testing ModernStandby Notification!` |

Host 設定的溫度與 EC 的 LUT 一致:

| Fan | LUT | 設定溫度 | 命中的 Level | 目標 RPM |
|-----|-----|----------|--------------|----------|
| Fan1 | SoC Tj | 74 °C | Lv01(71 < T ≤ 77) | `0x23` = 3500 |
| Fan2 | Tskin | 54 °C | Lv01(52 < T ≤ 57) | `0x32` = 5000 |

### EC 處理方式

- `0x23` 由 [acpi_soc_to_ec_modern_standby_notifi()](../app/service/power/service.c#L121) 轉成 `pwr_state_set(PWR_STA_MS / PWR_STA_S0)`,power service thread 處理狀態切換,再送出 `SYS_PWR_STA_CHG`。
- Fan service 收到事件後,在 [service.c:225](../app/service/fan/service.c#L225) 依電源狀態設定 Fan constraint:非 S0 時關閉 Fan 電源(`fan_en` GPIO),回到 S0 時再打開。
- Fan service 每 1 秒(`rpm_update`)讀一次 RPM,RPM 在 0 與非 0 之間切換時送出 SCI `0x30` / `0x31`([service.c:198](../app/service/fan/service.c#L198))。

---

## UEFI log

<details>
<summary>點擊展開</summary>

```shell
FS5:\> ECTA3013.efi -fst ms_test  -fnum 2
[1192186] - Loading driver at 0x000B6B9F000 EntryPoint=0x000B6BB6570 ECTestApp.efi
[WOS-EC E] [InsufficientArgCheck 51] Insufficient Arguments!
[WOS-EC E] [NextArgStrcmp 146] Insufficient String value!
[WOS-EC E] [CmpStrAndGetDecimal 216] Please specify [-scir]
[WOS-EC W] [ECTestAppMain 469]  Setting -scir to Default Value 4000(DEF_SCI_TIME_MS)! It must be >=4000(MIN_SCI_TIME_MS) and < 60001(MAX_SCI_TIME_MS)
[WOS-EC I] [ModernStandbyTest 1415] Testing ModernStandby Notification...
[WOS-EC I] [ModernStandbyTest 1417]  - Ensuring Fan is in Autonumous Mode...
[WOS-EC I] [ModernStandbyTest 1434]  - Performing MS Entry test...
[WOS-EC I] [ModernStandbyTest 1437]   - Cleaning Pending SCI Events before testing
[WOS-EC I] [GetCurrentLUTx 748] Get Fan1, Profile7, LUT_T1
[WOS-EC I] [ModernStandbyTest 1484]   - Setting T1 = 74 to reach to target RPM = 3500...
[WOS-EC I] [GetCurrentLUTx 748] Get Fan2, Profile7, LUT_T2
[WOS-EC I] [ModernStandbyTest 1484]   - Setting T2 = 54 to reach to target RPM = 5000...
[WOS-EC I] [ModernStandbyTest 1500]   - Reading Pending SCI Events
[WOS-EC I] [IsAllFanSpinning 797]   - Reading Fan1 Status & Speed
[WOS-EC I] [IsAllFanSpinning 797]   - Reading Fan2 Status & Speed
[WOS-EC I] [ModernStandbyTest 1518]   - All the Fan Status & Speed are reflected as spinning state prior MS Entry
[WOS-EC I] [ModernStandbyTest 1522]   - Sending MS Entry to EC...
[WOS-EC I] [ModernStandbyTest 1526]   - Reading Pending SCI Events
[WOS-EC I] [ModernStandbyTest 1544]   - Detected EC_SCI_FAN1_STATUS_CHANGE
[WOS-EC I] [ModernStandbyTest 1548]   - Detected EC_SCI_FAN2_STATUS_CHANGE
[WOS-EC I] [IsAllFanStopped 870]   - Reading Fan1 Status & Speed
FS5:\>  I] [IsAllFanStopped 870]   - Reading Fan2 Status & Speed
[WOS-EC I] [ModernStandbyTest 1569]   - All the Fan Status & Speed are reflected as stop state in MS
[WOS-EC I] [ModernStandbyTest 1572]   - Checking no SCI event been issued in MS.....
[WOS-EC I] [ModernStandbyTest 1611]  - Performing MS Exit test...
[WOS-EC I] [ModernStandbyTest 1613]   - Sending MS Exit to EC...
[WOS-EC I] [ModernStandbyTest 1618]   - Reading Pending SCI Events
[WOS-EC I] [ModernStandbyTest 1638]   - Detected EC_SCI_FAN2_STATUS_CHANGE
[WOS-EC I] [IsAllFanSpinning 797]   - Reading Fan1 Status & Speed
[WOS-EC I] [IsAllFanSpinning 797]   - Reading Fan2 Status & Speed
[WOS-EC I] [ModernStandbyTest 1658]   - All the Fan Status & Speed are reflected as spinning post MS Exit
[WOS-EC I] [ModernStandbyTest 1661] Done Testing ModernStandby Notification!
[WOS-EC I] [ModernStandbyTest 1690]   - Clearing Pending SCI Events
```

</details>

## EC log

此 EC log 為 2026-11-04 的測試(`ECTA3013.efi -fst ms_test`,`-fnum 1`,Profile 6)所收,不是上方 UEFI log 那次測試。`...` 為原始 log 中省略的重複 `Host read SCI while none pending`。

<details>
<summary>點擊展開</summary>

```shell
[00:00:41.724,914] <wrn> sci: Host read SCI while none pending
...
[00:00:53.041,198] <wrn> sci: Host read SCI while none pending
[00:00:53.214,019] <inf> power: Prepare to change power state: S0 -> Modern Standby
[00:00:53.214,050] <inf> app_pm: Power transition request: 1 -> 2
[00:00:53.214,050] <inf> app_pm: Try to enter heavy sleep
[00:00:53.214,080] <inf> power: Power state: Modern Standby
[00:00:53.214,172] <inf> fan_dev: Fan constraint: FAN(s) can only stay OFF
[00:00:53.231,536] <wrn> sci: Host read SCI while none pending
...
[00:00:54.178,894] <wrn> sci: Host read SCI while none pending
[00:00:54.214,294] <inf> fan: Fan: 2, rpm changed from 3102 to 0
[00:00:54.214,355] <inf> sci: Notify SCI 0x31 (0 queued)
[00:00:54.389,526] <wrn> sci: Host read SCI while none pending
...
[00:00:55.126,403] <wrn> sci: Host read SCI while none pending
[00:00:55.214,385] <inf> fan: Fan: 1, rpm changed from 1755 to 0
[00:00:55.214,477] <inf> sci: Notify SCI 0x30 (0 queued)
[00:00:55.336,944] <wrn> sci: Host read SCI while none pending
...
[00:01:03.445,587] <wrn> sci: Host read SCI while none pending
[00:01:04.438,018] <inf> power: Prepare to change power state: Modern Standby -> S0
[00:01:04.438,049] <inf> app_pm: Power transition request: 2 -> 1
[00:01:04.438,079] <inf> power: Power state: S0
[00:01:04.438,171] <inf> fan_dev: Fan constraint: FAN(s) can turn ON
[00:01:04.455,505] <wrn> sci: Host read SCI while none pending
...
[00:01:05.402,862] <wrn> sci: Host read SCI while none pending
[00:01:05.438,323] <inf> fan: Fan: 2, rpm changed from 0 to 2466
[00:01:05.438,385] <inf> sci: Notify SCI 0x31 (0 queued)
[00:01:05.613,372] <wrn> sci: Host read SCI while none pending
...
[00:01:06.350,219] <wrn> sci: Host read SCI while none pending
[00:01:06.438,415] <inf> fan: Fan: 1, rpm changed from 0 to 797
[00:01:06.438,507] <inf> sci: Notify SCI 0x30 (0 queued)
[00:01:06.560,760] <wrn> sci: Host read SCI while none pending
...
[00:01:17.070,648] <wrn> sci: Host read SCI while none pending
```

</details>
