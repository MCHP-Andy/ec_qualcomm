# SoCCP-EC 介面測試指令腳本

對應檔案:[app/service/soccp/service.c](../app/service/soccp/service.c) /
[app/service/soccp/soccp_handler.c](../app/service/soccp/soccp_handler.c)
規格參考:80-35782-87 Rev. AC (SoCCP-EC Interface Specification, Page 52-57)

## Shell 使用方式

| 指令 | 說明 |
|------|------|
| `soccp write <bytes...>` | 寫入 SoCCP command 與資料(所有參數皆以 **16 進位**解析,不需加 `0x`) |
| `soccp read` | 讀回最後一次 response buffer(固定 dump 64 bytes) |

> Payload 格式:`[cmd, reg_id, data_LSB, data_MSB]`
> `0x03` 是 Feature ID,真正功能由 **offset 0x01 的 Register ID**(`0x20`/`0x25`/`0x11`)決定。
> 16-bit 資料 **LSB 在前**(`cmd[2]=LSB, cmd[3]=MSB`)。
> Buffer 上限只有 **8 bytes**(`SOCCP_RECE_LEN` / `SOCCP_RESP_LEN`),勿超過。

---

## 一、EC SoCCP who-am-i (Cmd 0x43)

```bash
# mand=1, 回應 0x05
soccp write 43
soccp read                       # -> 第 1 byte = 0x05 (SoCCP who-am-i value)
```

---

## 二、EC FAN Constraints Message (Cmd 0x03, Reg 0x20)

```bash
# [0x03, 0x20, status_LSB, status_MSB] ; 0=OFF(default), 1=ON
soccp write 03 20 00 00          # Fan 維持 OFF
soccp write 03 20 01 00          # Fan 允許 ON
```

---

## 三、EC Off mode / OOB State Message (Cmd 0x03, Reg 0x25)

status bits:Bit0=Off-mode、Bit1=SoCCP active、Bit2=OOB init

```bash
soccp write 03 25 02 00          # 0x02  -> S0           (Bit1)
soccp write 03 25 03 00          # 0x03  -> S4/S5        (Bit0,1)
soccp write 03 25 06 00          # 0x06  -> S0 + OOB     (Bit1,2)
soccp write 03 25 07 00          # 0x07  -> S4/S5 + OOB  (Bit0,1,2)
```

---

## 四、EC SoC Power state message (Cmd 0x03, Reg 0x11)

```bash
# 1=S0, 2=ModernStandby, 3=S3, 4=S4, 5=S5, 6=G3
soccp write 03 11 01 00          # S0
soccp write 03 11 02 00          # Modern Standby (Entry)
soccp write 03 11 03 00          # S3
soccp write 03 11 04 00          # S4
soccp write 03 11 05 00          # S5
soccp write 03 11 06 00          # G3
```

---

## 五、驗證 / 邊界測試

```bash
# 未知的 Register ID -> handler 回 -EINVAL,log: "Unknown Power State Register ID"
soccp write 03 99 00 00

# 未知的 Feature/Cmd -> dispatcher log: "Unknown SoCCP command: 0x05"
soccp write 05

# mand 不足 (0x03 需要 4 bytes) -> SOCCP_CHECK_IN 回 -EINVAL
soccp write 03 11
```

---

## 重點提醒

- `0x03` 系列為 **WO**(write-only,`resp_len=0`),下完 `soccp write` 後 **不需** `soccp read`;只有 `0x43` who-am-i 有回應需要 `soccp read`。
- 目前 handler 只有 `LOG_INF` 印出結果(實際動作標 `TODO`),驗證時請看 log 輸出,例如:
  - `SoCCP: Fan Constraints set to ON`
  - `SoCCP: OOB Status updated: 0x0006`
  - `SoCCP: SoC Power State changed to: 2`

---

## SoCCP 指令對照表

| Cmd | Reg ID | 名稱 | mand | resp_len |
|-----|--------|------|------|----------|
| 0x43 | - | EC SoCCP who-am-i | 1 | 1 |
| 0x03 | 0x20 | EC FAN Constraints Message | 4 | 0 |
| 0x03 | 0x25 | EC Off mode / OOB State Message | 4 | 0 |
| 0x03 | 0x11 | EC SoC Power state message | 4 | 0 |
