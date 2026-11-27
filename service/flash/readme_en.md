# Flash service

[中文](readme.md) | English

Bootloader-only flash operation service. A single thread runs write / read / erase / CRC /
load-to-RAM in order, so flash accesses never overlap. Two ways to call it:
- `flash_service_submit()`: asynchronous, returns as soon as the request is queued and calls a
  callback when done. Used by the ACPI FW update commands.
- `flash_service_request()`: synchronous, returns when the operation is done. Used by the boot-time
  image load / validation (`driver/storage.c`).

Built only with `CONFIG_APP_BOOTLOADER=y`; there is no separate Kconfig or shell.

## Architecture
```mermaid
graph LR
    subgraph "Callers"
        FW[ACPI FW update<br/>ec_fw_update.c]
        STO[driver/storage.c]
    end

    subgraph "Flash service"
        SUB[flash_service_submit<br/>async]
        REQ[flash_service_request<br/>sync]
        Q[(flash_service_queue<br/>len 4)]
        THR[flash_service thread]
    end

    DRV[qc_flash_write / read / erase<br/>flash_service_cal_crc]
    INFO[(boot info status)]

    FW --> SUB
    STO --> REQ
    REQ -->|submit + k_sem_take| SUB
    SUB -->|BUSY| INFO
    SUB -->|k_msgq_put| Q
    Q --> THR
    THR --> DRV
    THR -->|done callback| FW
    THR -->|IDLE| INFO
```

### Flow
1. `flash_service_submit()` checks the `flash_service_msg_t` filled in by the caller, sets the boot info status to `EC_STATE_BUSY`,
   copies the msg into the queue (`K_NO_WAIT`, `-ENOMEM` when full) and returns; the caller's msg
   may live on the stack.
2. The thread (stack 1024, priority 5) runs the requests in order. After each one it calls the `done`
   callback first (e.g. to store a CRC result), then updates the status: once every queued request
   has finished it is `EC_STATE_BOOTLOADER_IDLE_WP_OFF`, also when one of them failed: a failure is
   only logged and passed to the `done` callback (`EC_STATE_SPI_OP_FAILED` is not reported for now).
3. The host polls the status with ACPI `0xEC` and waits for idle before the next step.
4. `flash_service_request()` is built on submit plus a semaphore on the caller's stack; it returns the
   result once the callback gives the semaphore.

Do not call `flash_service_request()` from the flash service thread itself (including a `done`
callback); it would deadlock.

## API
[interface/flash_service.h](../../interface/flash_service.h)

```c
typedef struct flash_service_msg_t {
    flash_service_command_t command;
    off_t flash_address;
    size_t size;
    void *ram_address;             // LOAD_TO_RAM destination
    void *data_pointer;            // WRITE source / READ destination
    flash_service_done_cb_t done;  // async completion callback, may be NULL
    void *user_data;               // passed to done
} flash_service_msg_t;

int flash_service_submit(const flash_service_msg_t *msg);
int flash_service_request(const flash_service_msg_t *msg, uint32_t *crc_result);
bool flash_service_busy(void);
```

| Command | Required argument | Description |
| --- | --- | --- |
| `FLASH_SERVICE_WRITE` | `data_pointer` | Write flash from `data_pointer` |
| `FLASH_SERVICE_READ` | `data_pointer` | Read into `data_pointer` |
| `FLASH_SERVICE_ERASE` | - | Erase `size` bytes starting at `flash_address` |
| `FLASH_SERVICE_CRC` | - (result: `crc_result` for sync, callback `crc` for async) | Compute CRC8 (stored in 4 bytes, upper 3 bytes are 0) |
| `FLASH_SERVICE_LOAD_TO_RAM` | `ram_address` | Read into `ram_address` |

Every command requires `flash_address >= 0` and `size > 0`, otherwise it returns `-EINVAL`. For an
asynchronous call, `data_pointer` / `ram_address` must stay valid until the callback runs (or the status
is no longer busy).

## Example
```c
// Async: returns at once, the result is handled in the callback (flash service thread)
static void crc_done(flash_service_command_t cmd, int result, uint32_t crc, void *user) {
    // store into boot info; the status leaves BUSY only after the callback
}
flash_service_msg_t msg = {
    .command = FLASH_SERVICE_CRC,
    .flash_address = IMG_ADDR,
    .size = hdr.fw_size,
    .done = crc_done,
};
flash_service_submit(&msg);   // msg is copied into the queue, it may live on the stack

// Sync: waits until done (done / user_data are ignored)
uint32_t crc;
int ret = flash_service_request(&msg, &crc);
```

## Test
The flash service has no shell. Verify it through the bootloader's ACPI FW update commands; see the
Bootloader command table in the [ACPI service](../acpi/readme_en.md):

```shell
uart:~$ acpi write 0xBB 0x01 0xF0
ACPI CMD written successfully
<inf> acpi: Run CRC: Size=..., Calculated 0x........, Expected 0x........

uart:~$ acpi write 0xA2
ACPI CMD written successfully
<inf> acpi: Read CRC: Status=0xAC, CRC=0x........
```
