
#pragma once

#if defined(CONFIG_APP_BOOTLOADER)
// Firmware update commands move up to 512B payloads (0xA0 / 0xA1)
#define ACPI_RECE_LEN 1024
#define ACPI_RESP_LEN 1024
#else
#define ACPI_RECE_LEN 512
#define ACPI_RESP_LEN 64
#endif

#include <stdint.h>
#include <stdio.h>

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

typedef int (*acpi_cmd_hdl_t)(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                              uint16_t cmd_len, uint8_t *resp,
                              uint16_t resp_len); // cmd now includes opcode

typedef struct acpi_cmd_t {
    uint8_t cmd;
    acpi_cmd_hdl_t cmd_hdl;
    uint16_t mand; // Number of mandatory arguments including the command opcode.
    uint16_t opt;  // Number of optional arguments *after* the command opcode.
    uint16_t
        resp_len; // Expected response length (including byte count if present).
    const char *name;
} acpi_cmd_t;

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
