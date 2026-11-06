# EC fan LUT test

Related files: [app/service/fan/service.c](../app/service/fan/service.c) /
[app/driver/fan.c](../app/driver/fan.c)

Specification:

- 80-35782-88 Rev. AB (Windows Mobile Compute EC Feature Acceptance Test Specification) 2.12 FAN LUT test
- 80-35782-87 Rev. AC (Windows Mobile Compute EC Interface Specification) `0x24` / `0x26` / `0x27` / `0x28`

## Test Result Summary

| Item | Result |
|------|--------|
| Overall result | **PASS** |
| Supported profiles | 7 (`0x26` returned 7) |
| Profiles 1–3 | No LUT defined; host reports `Not support any LUT` and skips them |
| Profiles 4–7 LUT1 (SoC Tj) | Fan1 Step up **PASS** (max Level 5), Step Down **PASS** |
| Profiles 4–7 LUT2 (Tskin) | Fan1 Step up **PASS** (max Level 5), Step Down **PASS** |
| Test cases | 8 (4 profiles × 2 LUTs), all PASS |

## Test Environment

| Item | Details |
|------|---------|
| Board | `qual_module/mec172x_nsz` |
| RTOS | Zephyr OS v3.7.1 |
| Host tool | UEFI Shell `ECTA3013.efi -fst fan_lut_test` (ECTestApp) |
| Test date | 2026-11-05 (log file timestamp) |
| Test parameters | All defaults (see table below) |
| Preconditions (spec) | Fan power constraint cleared, Thermal Telemetry disabled, Tskin Test config enabled |

| Parameter | Description | Value |
|-----------|-------------|-------|
| `-ft` | Fan response time | 2000 ms |
| `-dt1` | Wait for the fan to reach the target RPM | 1000 ms |
| `-dt2` | Wait time to read all pending SCI events | 1000 ms |
| `-dt3` | Wait time before retrying the RPM read | 2000 ms |
| `-rt` | RPM tolerance | 100 RPM |

## Test Flow

| # | Host action | EC behavior | Result |
|---|-------------|-------------|--------|
| 1 | Read number of supported profiles (`0x26`) | [acpi_ec_fan_profile_num()](../app/service/fan/service.c#L418) returns 7 | Total Profile Numbers = 7 |
| 2 | Read LUT count for profiles 1–7 (`0x27`) | [acpi_ec_fan_lut_num()](../app/service/fan/service.c#L488) | Profiles 1–3 have no LUT; profiles 4–7 have LUT1 and LUT2 |
| 3 | Switch profile (`0x24`) and read the LUT (`0x28`) | Returns the table from [fan_profiles](../app/driver/fan.c#L77) | Succeeded |
| 4 | Step up: set the temperature level by level, check RPM rises | [check_fan_rpm()](../app/service/fan/service.c#L132) looks up RPM by temperature and uses the higher of SoC Tj / Tskin | PASS (max Level 5) |
| 5 | Step down: lower the temperature level by level, check RPM falls | Same as above | PASS |

### LUT in the EC

The Fan1 SoC Tj (LUT1) and Tskin (LUT2) tables are identical in profiles 4–7:

| Level | tRPM | RPM | TmpU (°C) | TmpD (°C) |
|-------|------|-----|-----------|-----------|
| Lv00 | `0x19` | 2500 | `0x4A` (74) | `0x00` (0) |
| Lv01 | `0x23` | 3500 | `0x4D` (77) | `0x47` (71) |
| Lv02 | `0x2D` | 4500 | `0x50` (80) | `0x4A` (74) |
| Lv03 | `0x37` | 5500 | `0x53` (83) | `0x4D` (77) |
| Lv04 | `0x41` | 6500 | `0x55` (85) | `0x50` (80) |
| Lv05 | `0x4B` | 7500 | `0xFF` | `0x53` (83) |
| Lv06–Lv09 | `0x4B` | 7500 | `0xFF` | `0xFF` |

Entries after Lv05 are `0xFF` padding, so the highest valid level is Lv05. This matches the `max Level 5` the host reports for both LUT1 and LUT2.

The Fan2 LUTs (SoC Tj and Tskin are identical) are valid up to Lv07 (`0x64` = 10000 RPM, TmpD 64 °C). The host only tests Fan1, so the Fan2 LUTs were not verified in this run.

Profiles 1–3 (Battery saver, Better battery AC/DC) have no LUT defined in [app/driver/fan.c](../app/driver/fan.c#L77). This is the current design and matches the behavior in the spec's sample output.

## Notes

- The `500` after `Step Down Test PASS` is leftover console output. It does not affect the result.
- The Profile 5 LUT2 Step Down line was overwritten by the shell prompt and shows as `FS5:\> T2 Step Down Test PASS`. It should read `   - LUT2 Step Down Test PASS` and is counted as PASS.
- Only the UEFI log was captured for this run; there is no EC log.

---

## UEFI log

<details>
<summary>Click to expand</summary>

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
