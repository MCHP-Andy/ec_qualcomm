# ACPI 介面測試指令腳本

對應檔案:[app/service/acpi/service.c](../app/service/acpi/service.c)
規格參考:80-35782-87 Rev. AC (Windows Mobile Compute EC Interface Specification)

## Shell 使用方式

| 指令 | 說明 |
|------|------|
| `acpi write <bytes...>` | 寫入 ACPI command 與資料(所有參數皆以 **16 進位**解析,不需加 `0x`) |
| `acpi read` | 讀回最後一次 response buffer(固定 dump 64 bytes) |
| `sci en <0\|1>` | 啟用/關閉 SCI 通知 |
| `sci put <hex>` | 模擬塞入一筆 SCI event |
| `sci get` | 讀取並清除目前 pending 的 SCI |

> 注意:**寫入指令後需再下 `acpi read`** 才能看到回應。
> `0x24 / 0x25 / 0x28 / 0x30 / 0x32 / 0x34 / 0x35` 為讀寫共用同一 cmd,EC 以 payload 長度(是否帶 data)區分讀/寫。
> 16-bit 資料一律 **LSB 在前**。

---

## 一、EC Version and Capabilities

```bash
# 1. EC Device FW Version (0x0E)
uart:~$ acpi write 0E
ACPI CMD written successfully
[207:31:43.060,000] <inf> acpi: Handled ACPI cmd 0x0e successfully
uart:~$ acpi read
ACPI Read Response:
00000000: 03 01 01 01 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
uart:~$

# 2. EC Device FW Version + Lowest Supported FW Version (0x0F)
uart:~$ acpi write 0F
ACPI CMD written successfully
[339:49:18.460,000] <inf> acpi: Handled ACPI cmd 0x0f successfully
uart:~$ acpi read
ACPI Read Response:
00000000: 07 01 01 01 01 01 01 41  00 00 00 00 00 00 00 00 |.......A ........|
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
uart:~$

# 3. EC Device Thermal Capabilities (0x42, SubCmd=0x02)
uart:~$ acpi write 42 02
ACPI CMD written successfully
[428:21:09.010,000] <inf> acpi: Handled ACPI cmd 0x42 successfully
uart:~$ acpi read
ACPI Read Response:
00000000: 02 82 fc 01 01 01 01 41  00 00 00 00 00 00 00 00 |.......A ........|
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
uart:~$ 

# 4. EC Device Active Cooling IF Version & Capabilities (0x44)
uart:~$ acpi write 44
ACPI CMD written successfully
[795:11:32.490,000] <inf> acpi: Handled ACPI cmd 0x44 successfully
uart:~$ acpi read
ACPI Read Response:
00000000: 05 ff 3f 00 00 11 01 41  00 00 00 00 00 00 00 00 |..?....A ........|
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
uart:~$

# 5. EC ACPI who-am-i (0x43)  -> 回應 0x01
uart:~$ acpi write 43
ACPI CMD written successfully
[949:40:28.790,000] <inf> acpi: Handled ACPI cmd 0x43 successfully
uart:~$ acpi read
ACPI Read Response:
00000000: 01 ff 3f 00 00 11 01 41  00 00 00 00 00 00 00 00 |..?....A ........|
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
uart:~$ 

# 6. EC Device ID (0x06)
uart:~$ acpi write 06
ACPI CMD written successfully
[1056:18:10.400,000] <inf> acpi: Handled ACPI cmd 0x06 successfully
uart:~$ acpi read
ACPI Read Response:
00000000: 02 34 12 00 00 11 01 41  00 00 00 00 00 00 00 00 |.4.....A ........|
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
uart:~$ 
```

> `EC_DEV_FLASHING_CAP (0xB0)` 在指令表中被註解掉(Not Supported),故不列入。

---

## 二、EC Active Cooling Commands

```bash
# --- SoC to EC Temperature (0x20) ; Src=01(SoC Tj) ByteCount=02 Temp=651(0.1°C → 65.1°C)=0x028B
uart:~$ acpi write 20 01 02 8B 02
ACPI CMD written successfully
[662:27:46.674,000] <inf> acpi: Handled ACPI cmd 0x20 successfully
[662:27:46.674,000] <inf> sim_dev: [SIM] Fan 1 PWM set to: 0
[662:27:46.674,000] <inf> sim_dev: [SIM] Fan 2 PWM set to: 0

# --- EC Fan Status query (0x21) ; Fan1
uart:~$ acpi write 21 01
ACPI CMD written successfully
[778:58:16.444,000] <inf> acpi: Handled ACPI cmd 0x21 successfully
uart:~$ acpi read
ACPI Read Response:
00000000: 01 34 12 00 00 11 01 41  00 00 00 00 00 00 00 00 |.4.....A ........|
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
uart:~$ 

# --- EC Fan RPM query (0x22) ; Fan1
uart:~$ acpi write 22 01
ACPI CMD written successfully
[918:54:58.584,000] <inf> acpi: Handled ACPI cmd 0x22 successfully
uart:~$ acpi read
ACPI Read Response:
00000000: 02 00 00 28 32 5a 3c 34  ff 4d 34 ff ff 00 00 00 |...(2Z<4 .M4.....|
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|

# --- SoC to EC Modern Standby notify (0x23) ; 01=Enter / 00=Exit
acpi write 23 01

# --- Fan Profile (0x24)
uart:~$ acpi write 24                       # 讀取目前 profile
ACPI CMD written successfully
[1170:37:31.394,000] <inf> acpi: Handled ACPI cmd 0x24 successfully
uart:~$ acpi read
ACPI Read Response:
00000000: 16 00 12 00 00 11 01 41  00 00 00 00 00 00 00 00 |.......A ........|
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
uart:~$ acpi write 24 16                    # 設定: Bit4-7 FanID=1, Bit0-3 Profile=6(Best Perf w/ charger) -> 0x16
ACPI CMD written successfully
[32:46:26.448,000] <inf> acpi: Handled ACPI cmd 0x24 successfully
[32:46:26.448,000] <inf> sim_dev: [SIM] Fan 1 PWM set to: 0
[32:46:26.448,000] <inf> sim_dev: [SIM] Fan 2 PWM set to: 0

# --- Fan Trip Point (0x25) ; FanID=01
uart:~$ acpi write 25 01                    # 讀取 trip point
ACPI CMD written successfully
[542:07:48.718,000] <inf> acpi: Handled ACPI cmd 0x25 successfully
uart:~$ acpi read
ACPI Read Response:
00000000: 04 00 00 00 00 11 01 41  00 00 00 00 00 00 00 00 |.......A ........|
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
uart:~$ acpi write 25 01 04 D0 07 88 13     # 設定: ByteCount=4, Low=2000rpm(0x07D0), High=5000rpm(0x1388)
ACPI CMD written successfully
[636:27:21.828,000] <inf> acpi: Handled ACPI cmd 0x25 successfully
[636:27:21.828,000] <inf> sim_dev: [SIM] Fan 1 PWM set to: 0
[636:27:21.828,000] <inf> sim_dev: [SIM] Fan 2 PWM set to: 0
uart:~$ acpi read
ACPI Read Response:
00000000: 04 d0 07 88 13 11 01 41  00 00 00 00 00 00 00 00 |.......A ........|
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
uart:~$ 

# --- Number of Fan Profiles (0x26) ; Fan1
uart:~$ acpi write 26 01
ACPI CMD written successfully
[941:15:26.028,000] <inf> acpi: Handled ACPI cmd 0x26 successfully
uart:~$ acpi read
ACPI Read Response:
00000000: 06 d0 07 88 13 11 01 41  00 00 00 00 00 00 00 00 |.......A ........|
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
uart:~$

# --- Number of Fan LUTs (0x27) ; Bit0-3 FanID=1, Bit4-7 Profile=1 -> 0x11
uart:~$ acpi write 27 11
ACPI CMD written successfully
[1091:01:46.448,000] <inf> acpi: Handled ACPI cmd 0x27 successfully
uart:~$ acpi read
ACPI Read Response:
00000000: 04 d0 07 88 13 11 01 41  00 00 00 00 00 00 00 00 |.......A ........|
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
uart:~$

# --- Fan LUT (0x28) ; Fan&Profile=0x11, TempSrc=01(SoC Tj)
uart:~$ acpi write 28 11 01                 # 讀取 LUT
ACPI CMD written successfully
[226:48:15.512,000] <inf> acpi: Handled ACPI cmd 0x28 successfully
uart:~$ acpi read
ACPI Read Response:
00000000: 0c 00 46 00 22 50 43 34  ff 4d 34 ff ff 00 00 00
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
uart:~$ acpi write 28 11 01 06 1E 50 28 32 5A 3C   # 設定: ByteCount=6(2筆), 每筆=RPM(x100),TempHigh,TempLow
ACPI CMD written successfully
[347:04:15.242,000] <inf> acpi: Handled ACPI cmd 0x28 successfully
uart:~$ acpi write 28 11 01                 # 讀取 LUT
uart:~$ acpi read
ACPI Read Response:
00000000: 0c 1e 50 28 32 5a 3c 34  ff 4d 34 ff ff 00 00 00
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00



# --- EC Thermistors (0x29/0x2A/0x2B) ; Thermistor 1/2/3
uart:~$ acpi write 29
ACPI CMD written successfully
[815:44:56.602,000] <inf> acpi: Handled ACPI cmd 0x29 successfully
uart:~$ acpi read
ACPI Read Response:
00000000: 02 5e 01 50 28 32 5a 34  ff 4d 34 ff ff 00 00 00 |.^.P(2Z4 .M4.....|
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
uart:~$ 
uart:~$ acpi write 2A
ACPI CMD written successfully
[870:01:32.212,000] <inf> acpi: Handled ACPI cmd 0x2a successfully
uart:~$ acpi read
ACPI Read Response:
00000000: 02 90 01 50 28 32 5a 34  ff 4d 34 ff ff 00 00 00 |...P(2Z4 .M4.....|
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
uart:~$ 
uart:~$ acpi write 2B
ACPI CMD written successfully
[908:02:27.212,000] <inf> acpi: Handled ACPI cmd 0x2b successfully
uart:~$ acpi read
ACPI Read Response:
00000000: 02 c2 01 50 28 32 5a 34  ff 4d 34 ff ff 00 00 00 |...P(2Z4 .M4.....|
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
uart:~$

# --- Fan Debug Control (0x30) ; FanID=01
uart:~$ acpi write 30 01                    # 讀取 debug 設定
ACPI CMD written successfully
[298:48:42.246,000] <inf> acpi: Handled ACPI cmd 0x30 successfully
uart:~$ acpi read
ACPI Read Response:
00000000: 04 00 00 00 00 32 5a 34  ff 4d 34 ff ff 00 00 00 |.....2Z4 .M4.....|
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
uart:~$ acpi write 30 01 04 03 B8 0B 00     # 設定: Mode=0x03(DebugON+FanON+RPM), RPM=3000(0x0BB8), PWM=0
ACPI CMD written successfully
[369:44:07.216,000] <inf> acpi: Handled ACPI cmd 0x30 successfully
[369:44:07.216,000] <inf> sim_dev: [SIM] Fan 1 Target RPM set to: 3000
[369:44:07.216,000] <inf> sim_dev: [SIM] Fan 2 PWM set to: 0
uart:~$ acpi read
ACPI Read Response:
00000000: 04 03 b8 0b 00 32 5a 34  ff 4d 34 ff ff 00 00 00 |.....2Z4 .M4.....|
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
uart:~$


# --- EC Thermistor Temp Threshold (0x32) ; Bit0-3 ThermID=1, Bit4-7 DeviceID=1 -> 0x11
uart:~$ acpi write 32 11                    # 讀取 threshold
ACPI CMD written successfully
[39:56:56.930,000] <inf> acpi: Handled ACPI cmd 0x32 successfully
uart:~$ acpi read
ACPI Read Response:
00000000: 04 50 50 50 50 00 00 00  00 00 00 00 00 00 00 00 |.PPPP... ........|
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
uart:~$ acpi write 32 11 04 46 50 5A 64     # 設定: PSV=70, CR3=80, HOT=90, CRT=100 (°C)
ACPI CMD written successfully
[137:05:48.620,000] <inf> acpi: Handled ACPI cmd 0x32 successfully
uart:~$ acpi write 32 11                    # 讀取 threshold
ACPI CMD written successfully
[175:48:34.480,000] <inf> acpi: Handled ACPI cmd 0x32 successfully
uart:~$ acpi read
ACPI Read Response:
00000000: 04 46 50 5a 64 00 00 00  00 00 00 00 00 00 00 00 |.FPZd... ........|
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
uart:~$


# --- EC Thermistor Sampling Rate (0x34)
uart:~$ acpi write 34                       # 讀取 sampling rate
ACPI CMD written successfully
[561:31:11.570,000] <inf> acpi: Handled ACPI cmd 0x34 successfully
uart:~$ acpi read
ACPI Read Response:
00000000: e8 03 50 5a 64 00 00 00  00 00 00 00 00 00 00 00 |..PZd... ........|
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
uart:~$ acpi write 34 64 00                 # 設定: 100ms(0x0064), 最小值 100ms
ACPI CMD written successfully
[624:59:50.130,000] <inf> acpi: Handled ACPI cmd 0x34 successfully
uart:~$ acpi write 34                       # 讀取 sampling rate
ACPI CMD written successfully
[646:52:25.890,000] <inf> acpi: Handled ACPI cmd 0x34 successfully
uart:~$ acpi read
ACPI Read Response:
00000000: 64 00 50 5a 64 00 00 00  00 00 00 00 00 00 00 00 |d.PZd... ........|
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
uart:~$


# --- EC Functionality Flags (0x35)
acpi write 35                       # 讀取 flags
acpi read
acpi write 35 08 01 00 00 00 00 00 00 00   # 設定: ByteCount=8, Bit0=1 (SCI Event Enable)

uart:~$ acpi write 35                       # 讀取 flags
ACPI CMD written successfully
[945:48:26.950,000] <inf> acpi: Handled ACPI cmd 0x35 successfully
uart:~$ acpi read
ACPI Read Response:
00000000: 08 01 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
uart:~$ acpi write 35 08 00 00 00 00 00 00 00 00   # 設定: ByteCount=8, Bit0=0 (SCI Event Disable)
ACPI CMD written successfully
[1186:25:47.590,000] <inf> acpi: Handled ACPI cmd 0x35 successfully
uart:~$ acpi write 35                       # 讀取 flags
ACPI CMD written successfully
[18:44:16.014,000] <inf> acpi: Handled ACPI cmd 0x35 successfully
uart:~$ acpi read
ACPI Read Response:
00000000: 08 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
uart:~$


# --- EC Active Cooling SCI Event (0x05)
acpi write 05
```

---

## 三、SCI 事件測試

對照 [app/interface/acpi.h](../app/interface/acpi.h) 的 `sci_t` 定義:

| SCI 值 | 事件 |
|--------|------|
| `0x30` / `0x31` | Fan1 / Fan2 Status Change |
| `0x32` / `0x33` | Fan1 / Fan2 RPM Cross |
| `0x34` | LUT Set |
| `0x35` | Fan Profile Switch |
| `0x36` / `0x37` / `0x38` | EC Therm1 / 2 / 3 Cross |
| `0x3D` | EC Reset |

```bash
sci en 1          # 啟用 SCI 通知
sci put 30        # 模擬: Fan1 Status Change
sci put 32        # 模擬: Fan1 RPM Cross
sci put 36        # 模擬: EC Therm1 Cross
sci put 3D        # 模擬: EC Reset
sci get           # 讀取並清除目前 pending 的 SCI
sci en 0          # 關閉 SCI 通知
```

---

## ACPI 指令對照表

| Cmd | 名稱 | mand | opt | resp_len |
|-----|------|------|-----|----------|
| 0x0E | EC Device FW Version | 1 | 0 | 4 |
| 0x0F | FW Version + Lowest Supported | 1 | 0 | 8 |
| 0x42 | EC Device Thermal Capabilities | 2 | 0 | 3 |
| 0x44 | Active Cooling IF Version & Cap | 1 | 0 | 6 |
| 0x43 | EC ACPI who-am-i | 1 | 0 | 1 |
| 0x06 | EC Device ID | 1 | 0 | 3 |
| 0x20 | SoC to EC Temperature | 5 | 0 | 0 |
| 0x21 | EC Fan Status query | 2 | 0 | 1 |
| 0x22 | EC Fan RPM query | 2 | 0 | 3 |
| 0x23 | Modern Standby notify | 2 | 0 | 0 |
| 0x24 | Fan Profile (R/W) | 1 | 1 | 1 |
| 0x25 | Fan Trip Point (R/W) | 2 | 5 | 5 |
| 0x26 | Number of Fan Profiles | 2 | 0 | 1 |
| 0x27 | Number of Fan LUTs | 2 | 0 | 1 |
| 0x28 | Fan LUT (R/W) | 3 | - | - |
| 0x29 / 0x2A / 0x2B | EC Thermistor 1 / 2 / 3 | 1 | 0 | 3 |
| 0x30 | Fan Debug Control (R/W) | 2 | 5 | 5 |
| 0x32 | Thermistor Temp Threshold (R/W) | 2 | 5 | 5 |
| 0x34 | Thermistor Sampling Rate (R/W) | 1 | 2 | 2 |
| 0x35 | EC Functionality Flags (R/W) | 1 | 9 | 9 |
| 0x05 | Active Cooling SCI Event | 1 | 0 | 1 |
