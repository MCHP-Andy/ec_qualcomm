# EC modern standby test

Related files: [service/power/service.c](../service/power/service.c) /
[driver/power.c](../driver/power.c) /
[service/fan/service.c](../service/fan/service.c) /
[service/sci/service.c](../service/sci/service.c)

Specification:

- 80-35782-88 Rev. AB (Windows Mobile Compute EC Feature Acceptance Test Specification) 2.8 Validate FAN Post modern Standby (MSBY)
- 80-35782-87 Rev. AC (Windows Mobile Compute EC Interface Specification) `0x23` SoC to EC Modern Standby notification, `0x05` SCI `0x30` / `0x31`

## Test Result Summary

| Item | Result |
|------|--------|
| Overall result | **PASS** |
| Fans verified | Fan1, Fan2 (`-fnum 2`) |
| Fan state before MS Entry | Fan1 and Fan2 both spinning |
| SCI after MS Entry | Detected `EC_SCI_FAN1_STATUS_CHANGE` and `EC_SCI_FAN2_STATUS_CHANGE` |
| Fan state after MS Entry | Fan1 and Fan2 both stopped |
| SCI during MS | No SCI issued |
| SCI after MS Exit | Detected `EC_SCI_FAN2_STATUS_CHANGE` |
| Fan state after MS Exit | Fan1 and Fan2 both spinning |

## Test Environment

| Item | Details |
|------|---------|
| Board | `qual_module/mec172x_nsz` |
| RTOS | Zephyr OS v3.7.1 |
| Host tool | UEFI Shell `ECTA3013.efi -fst ms_test -fnum 2` (ECTestApp) |
| Test date | 2026-11-05 (UEFI log file timestamp) |
| `-fnum` | 2 (checks Fan1 and Fan2) |
| `-scir` | Default 4000 ms (SCI wait time) |
| Fan profile | Profile 7 (Best performance, DC) |
| Preconditions (spec) | Thermal Telemetry disabled |

## Test Flow

| # | Host action | EC behavior | Result |
|---|-------------|-------------|--------|
| 1 | Make sure the fans are in autonomous mode, clear pending SCIs | Answers SCI reads | Succeeded |
| 2 | Read Profile7 Fan1 LUT_T1, set T1 = 74 °C, target RPM = 3500 | Drives Fan1 from the LUT | Succeeded |
| 3 | Read Profile7 Fan2 LUT_T2, set T2 = 54 °C, target RPM = 5000 | Drives Fan2 from the LUT | Succeeded |
| 4 | Read Fan1 / Fan2 status / speed | Fans spinning | Spinning, succeeded |
| 5 | Send MS Entry (`0x23`, status=1) | [acpi_soc_to_ec_modern_standby_notifi()](../service/power/service.c#L109) → S0 → Modern Standby; [pwr_state_chg()](../driver/power.c#L261) enables heavy sleep; fan constraint set to OFF | Succeeded |
| 6 | Read pending SCI | Sends SCI `0x30` / `0x31` when a fan's RPM drops to 0 | Detected Fan1 and Fan2 SCIs |
| 7 | Read Fan1 / Fan2 status / speed | Fans stopped | Stopped, succeeded |
| 8 | Check that no SCI is issued during MS | No SCI issued | Succeeded |
| 9 | Send MS Exit (`0x23`, status=0) | Modern Standby → S0, heavy sleep disabled; fan constraint set to ON | Succeeded |
| 10 | Read pending SCI | Sends SCI `0x30` / `0x31` when a fan's RPM goes from 0 to non-zero | Only Fan2 SCI detected |
| 11 | Read Fan1 / Fan2 status / speed, clear pending SCIs | Fans spinning | Spinning, `Done Testing ModernStandby Notification!` |

The temperatures the host set match the EC LUTs:

| Fan | LUT | Temperature set | Level hit | Target RPM |
|-----|-----|-----------------|-----------|------------|
| Fan1 | SoC Tj | 74 °C | Lv01 (71 < T ≤ 77) | `0x23` = 3500 |
| Fan2 | Tskin | 54 °C | Lv01 (52 < T ≤ 57) | `0x32` = 5000 |

### How the EC handles it

- [acpi_soc_to_ec_modern_standby_notifi()](../service/power/service.c#L121) turns `0x23` into `pwr_state_set(PWR_STA_MS / PWR_STA_S0)`. The power service thread performs the state change and then posts `SYS_PWR_STA_CHG`.
- When the fan service gets the event, it sets the fan constraint from the power state at [service.c:225](../service/fan/service.c#L225): outside S0 it cuts fan power (`fan_en` GPIO), and back in S0 it turns it on again.
- The fan service reads RPM every 1 s (`rpm_update`). When RPM switches between 0 and non-zero, it sends SCI `0x30` / `0x31` ([service.c:198](../service/fan/service.c#L198)).

---

## UEFI log

<details>
<summary>Click to expand</summary>

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

This EC log was captured during the 2026-11-04 run (`ECTA3013.efi -fst ms_test`, `-fnum 1`, Profile 6), not the run in the UEFI log above. `...` marks repeated `Host read SCI while none pending` lines that were left out of the original log.

<details>
<summary>Click to expand</summary>

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
