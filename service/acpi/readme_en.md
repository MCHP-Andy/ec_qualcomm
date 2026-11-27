# ACPI service

[中文](readme.md) | English

Receives ACPI commands sent by the host (SoC) through the I2C target (address `0x76`), dispatches them
to the handlers registered by each service, and writes the response back to the interface buffer for
the host to read. The service is shared by the app and the bootloader, but each registers a different
set of commands (see [Command table](#command-table)).

## Kconfig
| Option | Default | Description |
| --- | --- | --- |
| `CONFIG_ACPI_SERVICE` | y | Enable the ACPI service |
| `CONFIG_ACPI_LOG_LEVEL` | - | Log level |
| `CONFIG_ACPI_SHELL` | y (requires `SHELL`) | `acpi` shell commands |

Buffer sizes are defined in [interface/acpi.h](../../interface/acpi.h):

| | `ACPI_RECE_LEN` | `ACPI_RESP_LEN` |
| --- | --- | --- |
| App | 512 | 64 |
| Bootloader (`CONFIG_APP_BOOTLOADER`) | 1024 | 1024 (0xA0 / 0xA1 carry 512 B payloads) |

## Architecture
```mermaid
graph LR
    subgraph "Driver (driver/acpi.c)"
        I2C[I2C target 0x76]
        WR[(wr_buf)]
        RESP[(resp_buf)]
    end

    subgraph "ACPI service"
        BUF[acpi_cmd_submit]
        EVT((acpi_cmd_queue<br/>k_msgq + k_mem_slab))
        HDL[acpi_cmd_hdl<br/>table lookup]
    end

    subgraph "acpi_cmd_t iterable section"
        TBL[ACPI_CMD_SUBSCRIBE<br/>fan / thermal / power / sci / board / fw]
    end

    I2C -->|write_received| WR
    WR -->|buf_read_requested / stop<br/>whole command| BUF
    BUF -->|k_msgq_put| EVT
    EVT --> HDL
    HDL --> TBL
    TBL -->|handler fills resp| HDL
    HDL -->|acpi_resp_set| RESP
    RESP -.->|I2C read| I2C
```

### Receive and dispatch flow
1. The driver (`driver/acpi.c`) stores the bytes the host writes to 0x76 in `wr_buf`. The service is not
   notified yet.
2. When the host reads with a repeated start (`buf_read_requested`) or sends a stop, the driver calls
   `acpi_cmd_submit(wr_buf, len)` to hand the whole command (opcode + arguments) to the service, once
   per write.
3. `acpi_cmd_submit` takes a block from a `k_mem_slab`, copies the command into it and puts the pointer
   on a `k_msgq` (depth `ACPI_CMD_QUEUE_LEN`). It is ISR safe; when the queue is full the command is
   dropped and `-ENOMEM` is returned.
4. The service thread (`APP_PRIO_H`) takes the command from the queue, looks up the opcode in the
   `acpi_cmd_t` section and calls the handler when `cmd_len >= mand`. If the command is too short, the
   handler is not called. On a read the I2C driver stretches SCL for
   `CONFIG_I2C_XEC_V2_CUSTOM_STRETCH_DELAY_MS` (3 ms by default) so the handler can write the response.
5. The handler decides read or write from the input length, and its return value sets how many
   bytes are written back to the ACPI buffer:

   | Input length `cmd_len` | Action | Return |
   |---|---|---|
   | `== mand` | Read: fill `resp` | response length (>0) |
   | `== mand + opt` (`ACPI_IS_WRITE` / `ACPI_CHECK_WRITE`) | Write: apply the data only | `0` |
   | error | — | negative errno |

   The service calls `acpi_resp_set(resp_buf, ret)` only when the return value is > 0; on 0 or an
   error the ACPI buffer is left unchanged. Commands with `opt == 0` are either read only
   (`resp_len > 0`) or write only (`resp_len == 0`). `resp_len` in the table is the maximum response
   length (checked by `ACPI_CHECK_OUT`).

### Adding a command
Use the macro from [interface/acpi.h](../../interface/acpi.h) in any service:

```c
// ACPI_CMD_SUBSCRIBE(name, opcode, handler, mand, opt, resp_len)
ACPI_CMD_SUBSCRIBE(fan, EC_FAN_RPM, acpi_ec_fan_rpm, 2, 0, 3);
```

| Parameter | Description |
| --- | --- |
| `mand` | Number of mandatory bytes, **including the opcode** |
| `opt` | Number of optional (write) bytes after `mand`. `cmd_len == mand` is a read (get), `cmd_len == mand + opt` is a write (set) |
| `resp_len` | Maximum response length (including the byte count, if any) |

Inside a handler, use the macros from [acpi_tbl.h](acpi_tbl.h):
- `ACPI_CHECK_IN` / `ACPI_CHECK_OUT`: input length >= `mand`, `resp` buffer >= `resp_len`.
- `ACPI_IS_WRITE`: the input is longer than `mand` (and `opt > 0`), so it is a write.
- `ACPI_CHECK_WRITE`: a write must be exactly `mand + opt` bytes, otherwise `-EINVAL`.

```c
static int acpi_xxx(const acpi_cmd_t *cmd_info, uint8_t *cmd, uint16_t cmd_len,
                    uint8_t *resp, uint16_t resp_len) {
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);

    if (ACPI_IS_WRITE(cmd_info, cmd_len)) {
        ACPI_CHECK_WRITE(cmd_info, cmd_len);
        // apply the data in cmd[mand..]
        return 0;
    }

    ACPI_CHECK_OUT(cmd_info, resp, resp_len);
    resp[0] = ...;
    return 1; // response length
}
``` Command opcodes are also defined in `acpi_tbl.h`.

## Command table

The `cmd` column is the full byte sequence written by the host (`[]` is optional; sending it makes the
command a write), and `resp` is what the host reads back for a read. A write does not update the
response.

### App
| Opcode | Name | File | cmd | resp |
| --- | --- | --- | --- | --- |
| 0x05 | Active cooling SCI event | [sci](../sci/service.c) | `05` | `sci` (cleared on read) |
| 0x06 | Device ID | [ec_ver_and_cap.c](ec_ver_and_cap.c) | `06` | `02 34 12` |
| 0x0E | FW version | ec_ver_and_cap.c | `0E` | `03 test sub major` |
| 0x0F | FW version + lowest supported | ec_ver_and_cap.c | `0F` | `07 ver(3) lowest(3) 'A'` |
| 0x42 | Thermal capabilities | ec_ver_and_cap.c | `42 02` | `02 82 FC` |
| 0x43 | ACPI who-am-i | ec_ver_and_cap.c | `43` | `01` |
| 0x44 | Active cooling IF ver & caps | ec_ver_and_cap.c | `44` | `05 FF 3F 00 00 11` |
| 0x20 ~ 0x30 | Fan commands | [fan](../fan/readme_en.md) | | |
| 0x23 | Modern standby notify | [power](../power/readme_en.md) | `23 status` | - |
| 0x29 ~ 0x34 | Thermistor commands | [thermal](../thermal/readme_en.md) | | |
| 0x35 | EC function flags | [ec_act_cool.c](ec_act_cool.c) | `35 [08 flags(8)]` | `08 flags(8)`; bit0 = SCI enable |
| 0xBB | SPI services | [ec_fw_update.c](ec_fw_update.c) | `BB 01 sub` | - |
| 0xEC | EC state & WP status | ec_fw_update.c | `EC` | `02 state path` |

On the app side, 0xBB supports only:
- `0xDC` Enter bootloader: sets boot info to `EC_BOOT_MODE_STAY_IN_BBK`, saves it, and jumps to the
  bootloader after 100 ms.
- `0x33` Exit bootloader: already in the main block, does nothing.

`0xB0` (flashing capabilities) is not supported yet.

### Bootloader ([ec_fw_update.c](ec_fw_update.c), `CONFIG_APP_BOOTLOADER`)
| Opcode | Name | cmd | resp |
| --- | --- | --- | --- |
| 0xEB | FW corruption status | `EB` | `01 status` |
| 0xEC | EC state & WP status | `EC` | `02 state path` |
| 0xA2 | Read CRC result | `A2` | `05 status(AC/FA) crc(4B LE, CRC8 in LSB)` |
| 0xAE | Erase region | `AE 05 path blk addr(3)` | - |
| 0xAF | Quick erase partition | `AF 01 sub` | - |
| 0xA0 | Write RAM buffer | `A0 data(512)` | - |
| 0xAA | Commit RAM buffer to flash | `AA 05 path size/32 addr(3)` | - |
| 0xA7 | Read region into RAM buffer | `A7 04 path addr(3)` | - |
| 0xA1 | Read RAM buffer | `A1` | `data(512)` |
| 0xBB | SPI services | `BB 01 sub` | - |

Notes:
- Flash control path: `0x4F` internal, `0x0F` external (not supported), `0xEA` auto
  (0xAA uses the write address set by 0xAF, which advances after each commit).
- `0xAE` and `0xAA` (path `0x4F`) add `0x9000` to the host address to match the actual flash address;
  `0xA7` uses the host address as is.
- The `blk` field of `0xAE` is in 4 KB units; the `size` field of `0xAA` is in 32-byte units and is
  capped at the length written by the last 0xA0.
- `0xA0` has `mand` = 513, so it only runs once the full 512 bytes are received.
- `0xAF` currently supports only `0x21` (erase backup main, `CONFIG_BOARD_QC_MODULE`); other
  sub-commands only log.
- All flash operations run asynchronously in the [flash service](../flash/readme_en.md): `0xAE` / `0xAF` /
  `0xAA` / `0xA7` / `0xBB F0` (Run CRC) return as soon as they are queued, and the host polls `0xEC`:
  `0xFF` (busy), then `0x0B` (idle). A failed flash operation is only logged and the status still
  returns to idle (`0xFA` is not reported for now).
- While the flash is busy, these flash commands and `0xA0` / `0xA1` (which share the RAM buffer with
  0xAA / 0xA7) return `-EBUSY` and do not update the response.
- Read with `0xA1` only after `0xA7` has finished (0xEC idle); read the CRC with `0xA2` only after
  `0xBB F0` has finished.

0xBB sub-commands:
| Sub | Function |
| --- | --- |
| `0xDC` / `0x33` | Enter / exit bootloader (exit validates the image header first and stays in the bootloader on failure) |
| `0xCC` | Cold reboot |
| `0xC0` / `0xC1` / `0xC2` | Forced crash: kernel oops / assert / watchdog hang (for testing) |
| `0x11` ~ `0x13` | Code mirror (log only) |
| `0x15` ~ `0x1C` | FW sync (log only) |
| `0xF0` | Compute CRC of internal active main, store the result in boot info, read it back with 0xA2 |
| `0xF1` ~ `0xF9` | CRC of other partitions (not supported) |

Typical FW update sequence:
```
BB 01 DC                 # (App) enter bootloader
EC                       # check state
AF 01 21                 # erase main partition, set auto write address
A0 <512 bytes>           # repeat: write RAM buffer
AA 05 EA 10 00 00 00     #         commit 512 bytes (0x10 * 32) to the auto address
BB 01 F0                 # compute CRC
A2                       # read CRC result (0xAC = passed)
BB 01 33                 # exit bootloader
```

## Shell
| Command | Description |
| --- | --- |
| `acpi write <op> [data ...]` | Simulate a host write in hex; submits the whole write with `acpi_cmd_submit` (same as an I2C stop) |

The shell has no command to read the response; check the log or read it back from the host.
The `fw_update` shell in `ec_fw_update.c` (bootloader) is guarded by `CONFIG_FW_UPDATE_SHELL`, which is
not defined in any Kconfig yet, so it is never compiled in.

## Test
```shell
uart:~$ acpi write 0x0E
ACPI CMD written successfully
# Host reads back: 03 01 01 01

uart:~$ acpi write 0x42 0x02
ACPI CMD written successfully
# Host reads back: 02 82 FC

uart:~$ acpi write 0x35 0x08 0x01 0 0 0 0 0 0 0
ACPI CMD written successfully
# SCI enabled, host reads back: 08 01 00 00 00 00 00 00 00
```

Set `CONFIG_ACPI_LOG_LEVEL_DBG=y` to see `Handled ACPI cmd 0x.. successfully`.
