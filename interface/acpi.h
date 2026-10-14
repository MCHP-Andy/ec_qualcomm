
#pragma once

#define ACPI_RECE_LEN 512
#if defined(CONFIG_EC_BOOTLOADER) && CONFIG_EC_BOOTLOADER
#define ACPI_RESP_LEN 512
#else
#define ACPI_RESP_LEN 64
#endif

#include <stdbool.h>
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
 * 
 */
int acpi_buf_set(acpi_type_t id, uint8_t data);

/*
 *
 */
int acpi_resp_set(uint8_t *pdata, uint16_t len);

/**
 * @brief Pulse the ACPI interrupt (SCI alert) line to notify the host.
 *
 * The line is only pulsed when an SCI event is sent, it is not held asserted
 * while waiting for the host to read the event back with cmd
 * EC_ACTIVE_COOLING_SCI_EVENT (0x05).
 */
int acpi_int_pulse(void);
