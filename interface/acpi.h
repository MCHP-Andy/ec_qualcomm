
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

typedef enum {
    ACPI_TYPE_CMD = 0,
    ACPI_TYPE_DATA,
    ACPI_TYPE_PROCESS,
} acpi_type_t;

/**
 * @brief Feed one byte received from the host into the ACPI command buffer.
 *
 * ACPI_TYPE_CMD starts a new command with @p data as the opcode,
 * ACPI_TYPE_DATA appends an argument byte, and ACPI_TYPE_PROCESS marks the
 * end of the transfer. The command handler is dispatched as soon as all
 * mandatory arguments arrive, and again at the end of the transfer if
 * optional payload followed.
 *
 * @note Implemented in service (service/acpi), called by driver (driver/acpi.c).
 *
 * @param id   Byte type, see acpi_type_t.
 * @param data Received byte (ignored for ACPI_TYPE_PROCESS).
 * @retval 0 always.
 */
int acpi_buf_set(acpi_type_t id, uint8_t data);

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
