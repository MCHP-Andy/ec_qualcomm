
#pragma once

#if defined(CONFIG_APP_BOOTLOADER)
// Firmware update commands move up to 512B payloads (0xA0 / 0xA1)
#define ACPI_RECE_LEN 1024
#define ACPI_RESP_LEN 1024
#else
#define ACPI_RECE_LEN 512
#define ACPI_RESP_LEN 64
#endif

#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#include <zephyr/logging/log.h>

/* -------------------------------------------------------------------------
 * ACPI command opcodes (all ACPI commands are defined here)
 * ------------------------------------------------------------------------- */
// EC Version and Capabilities
#define EC_DEV_FW_VER                               0x0E
#define EC_DEV_FW_VER_AND_LOWEST_SUPPORTED_FW_VER   0x0F
#define EC_DEV_FLASHING_CAP                         0xB0
#define EC_DEV_THERMAL_CAP                          0x42
#define EC_DEV_ACTIVE_COOLING_IF_VER_AND_CAP        0x44
#define EC_ACPI_WHOAMI_IF                           0x43
#define EC_DEV_ID                                   0x06

// EC Active Cooling Commands
#define EC_ACTIVE_COOLING_SCI_EVENT                 0x05
#define SOC_TO_EC_TEMP                              0x20
#define EC_FAN_STATUS                               0x21
#define EC_FAN_RPM                                  0x22
#define SOC_TO_EC_MODERN_STANDBY_NOTIFI             0x23
#define EC_FAN_PROFILE                              0x24
#define EC_FAN_TRIP_POINT                           0x25
#define EC_FAN_PROFILE_NUM                          0x26
#define EC_FAN_LUT_NUM                              0x27
#define EC_FAN_LUT                                  0x28
#define EC_THERMISTOR1                              0x29
#define EC_THERMISTOR2                              0x2A
#define EC_THERMISTOR3                              0x2B
#define EC_FAN_DEBUG_CTRL                           0x30
#define EC_THERMISTOR_TEMP_THRE                     0x32
#define EC_THERMISTOR_SAMPLING_RATE                 0x34
#define EC_FUNC_FLAG                                0x35

// EC Firmware Update Commands (80-35782-87 Rev. AC)
#define EC_FLASH_WRITE_BUF                          0xA0
#define EC_FLASH_READ_BUF                           0xA1
#define EC_READ_CRC_RESULT                          0xA2
#define EC_FLASH_READ_SETUP                         0xA7
#define EC_FLASH_WRITE_COMMIT                       0xAA
#define EC_FLASH_ERASE_REGION                       0xAE
#define EC_FLASH_ERASE_PARTITION                    0xAF
#define EC_FW_CODE_MIRROR                           0xBB // SPI services
#define EC_FW_CORRUPTION_STATUS                     0xEB
#define EC_STATE_AND_WP_STATUS                      0xEC

// ACPI command

#define ACPI_CMD_SUBSCRIBE(A, B, C, D, E, F)                                   \
    STRUCT_SECTION_ITERABLE(                                                   \
        acpi_cmd_t, UTIL_CAT(_acpi_cmd_, UTIL_CAT(B, UTIL_CAT(_, C)))) = {     \
        .cmd = B,                                                              \
        .cmd_hdl = C,                                                          \
        .mand = D,                                                             \
        .opt = E,                                                              \
        .resp_len = F,                                                         \
        .name = #A,                                                            \
    }

struct acpi_cmd_t; 
typedef struct acpi_cmd_t acpi_cmd_t;

/**
 * @brief ACPI command handler.
 *
 * Read or write is decided by @p cmd_len: only the mandatory part (mand) is a
 * read, mandatory + optional payload (mand + opt) is a write.
 *
 * @param cmd_info Command table entry.
 * @param cmd      Command bytes, cmd[0] is the opcode.
 * @param cmd_len  Number of command bytes.
 * @param resp     Response buffer.
 * @param resp_len Size of @p resp.
 * @retval >0 read: number of response bytes written to @p resp.
 * @retval 0  write done (no response data).
 * @retval <0 error, the ACPI response buffer is left unchanged.
 */
typedef int (*acpi_cmd_hdl_t)(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                              uint16_t cmd_len, uint8_t *resp,
                              uint16_t resp_len);

typedef struct acpi_cmd_t {
    uint8_t cmd;
    acpi_cmd_hdl_t cmd_hdl;
    uint16_t mand; // Number of mandatory arguments including the command opcode.
    uint16_t opt;  // Number of optional arguments *after* the command opcode.
    uint16_t
        resp_len; // Max response length (including byte count if present).
    const char *name;
} acpi_cmd_t;

/* -------------------------------------------------------------------------
 * ACPI command handler helpers (LOG_ERR of the calling module)
 * ------------------------------------------------------------------------- */
/*
 * ACPI command handler contract
 *
 * Read or write is decided by the input length (opcode included):
 *   read : cmd_len == mand        -> fill resp, return the response length
 *   write: cmd_len == mand + opt  -> apply the data, return 0
 * A negative return is an error; nothing is written to the ACPI buffer.
 * Commands with opt == 0 are either read only (resp_len > 0) or write only
 * (resp_len == 0).
 */
#define ACPI_IS_WRITE(cmd_info_ptr, cl)                                        \
    (((cmd_info_ptr)->opt > 0) && ((cl) > (cmd_info_ptr)->mand))

// Write payload must be complete: cmd_len == mand + opt
#define ACPI_CHECK_WRITE(cmd_info_ptr, cl)                                     \
    do {                                                                       \
        if ((cl) != ((cmd_info_ptr)->mand + (cmd_info_ptr)->opt)) {            \
            LOG_ERR("Invalid write length for cmd 0x%02x: %u (expected %u)",   \
                    (cmd_info_ptr)->cmd, (unsigned int)(cl),                   \
                    (unsigned int)((cmd_info_ptr)->mand +                      \
                                   (cmd_info_ptr)->opt));                      \
            return -EINVAL;                                                    \
        }                                                                      \
    } while (0)

#define ACPI_CHECK_IN(cmd_info_ptr, c, cl)                                     \
    do {                                                                       \
        if (!(cmd_info_ptr)) {                                                 \
            LOG_ERR("Invalid cmd_info_ptr (NULL) for ACPI_CHECK_IN");          \
            return -EINVAL;                                                    \
        }                                                                      \
        if (!(c) || (cl) < (cmd_info_ptr)->mand) {                             \
            LOG_ERR("Invalid input for cmd 0x%02x: ptr=%p, len=%u (min=%u)",   \
                    (cmd_info_ptr)->cmd, (void *)(c), (unsigned int)(cl),      \
                    (unsigned int)((cmd_info_ptr)->mand));                     \
            return -EINVAL;                                                    \
        }                                                                      \
    } while (0)

#define ACPI_CHECK_OUT(cmd_info_ptr, r, rl)                                    \
    do {                                                                       \
        if (!(cmd_info_ptr)) {                                                 \
            LOG_ERR("Invalid cmd_info_ptr (NULL) for ACPI_CHECK_OUT");         \
            return -EINVAL;                                                    \
        }                                                                      \
        if (!(r) || (rl) < (cmd_info_ptr)->resp_len) {                         \
            LOG_ERR("Invalid output for cmd 0x%02x: ptr=%p, len=%u (min=%u)",  \
                    (cmd_info_ptr)->cmd, (void *)(r), (unsigned int)(rl),      \
                    (unsigned int)((cmd_info_ptr)->resp_len));                 \
            return -EINVAL;                                                    \
        }                                                                      \
    } while (0)

/**
 * @brief Submit one complete ACPI command received from the host.
 *
 * The command (opcode followed by its arguments) is copied into the service
 * command queue and dispatched by the service thread. The driver calls it
 * once per write, when the host starts reading (repeated start) or sends a
 * stop.
 *
 * @note Implemented in service (service/acpi), called by driver (driver/acpi.c).
 *       Safe to call from ISR context.
 *
 * @param pdata Command bytes, pdata[0] is the opcode.
 * @param len   Number of bytes, 1 .. ACPI_RECE_LEN.
 * @retval 0 on success.
 * @retval -EINVAL @p len is 0 or larger than ACPI_RECE_LEN.
 * @retval -ENOMEM command queue is full, the command is dropped.
 */
int acpi_cmd_submit(const uint8_t *pdata, uint16_t len);

/**
 * @brief Load the response that the host reads back over I2C.
 *
 * Data beyond the response buffer size (ACPI_RESP_LEN) is truncated.
 *
 * @note Implemented in driver (driver/acpi.c), called by service (service/acpi).
 *
 * @param pdata Response data.
 * @param len   Response length in bytes.
 * @retval 0 always.
 */
int acpi_resp_set(uint8_t *pdata, uint16_t len);

#if !defined(CONFIG_APP_BOOTLOADER)
/**
 * @brief Set the ACPI interrupt (SCI alert) line.
 *
 * @note Implemented in driver (driver/acpi.c), called by service (service/sci).
 *
 * @param val Pin level, either 0 (deasserted) or 1 (asserted).
 * @retval 0 on success.
 * @retval -EINVAL @p val is greater than 1.
 * @retval -ENODEV ACPI interrupt GPIO is not ready.
 * @retval -ENOTSUP no ACPI interrupt pin on this board.
 */
int acpi_int_set(uint8_t val);
#endif
