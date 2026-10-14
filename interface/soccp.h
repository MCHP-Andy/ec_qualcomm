/*
 * @Author: andy.chang 
 * @Date: 2026-04-21 22:29:58 
 * @Last Modified by:   andy.chang 
 * @Last Modified time: 2026-04-21 22:29:58 
 */

#pragma once

#define SOCCP_RECE_LEN 8
#define SOCCP_RESP_LEN 8

#include <stdint.h>
#include <stdio.h>

struct soccp_cmd_t; 
typedef struct soccp_cmd_t soccp_cmd_t;

typedef int (*soccp_cmd_hdl_t)(const soccp_cmd_t *cmd_info, uint8_t *cmd,
                              uint16_t cmd_len, uint8_t *resp,
                              uint16_t resp_len); // cmd now includes opcode

typedef struct soccp_cmd_t {
    uint8_t cmd;
    soccp_cmd_hdl_t cmd_hdl;
    uint16_t mand; // Number of mandatory arguments including the command opcode.
    uint16_t opt;  // Number of optional arguments *after* the command opcode.
    uint16_t
        resp_len; // Expected response length (including byte count if present).
} soccp_cmd_t;

typedef enum {
    SOCCP_TYPE_CMD = 0,
    SOCCP_TYPE_DATA,
} soccp_type_t;

/*
 * OffMode/OOB status bits (Ref: EC Off mode / OOB State Message, page 56)
 *
 * Expected combinations:
 *   S0 + OOB    : 0x06 (bit 1, 2)
 *   S4/S5 + OOB : 0x07 (bit 0, 1, 2)
 *   S0          : 0x02 (bit 1)
 *   S4/S5       : 0x03 (bit 0, 1)
 */
#define SOCCP_OOB_STA_OFF_MODE (1U << 0) // SoC is on Off-mode
#define SOCCP_OOB_STA_ACTIVE   (1U << 1) // SoCCP is active (always set)
#define SOCCP_OOB_STA_INITED   (1U << 2) // OOB initialized, can talk to SoCCP
#define SOCCP_OOB_STA_MASK     (0x07U)   // Bit 3 to 7 are reserved

/**
 * @brief Get the latest OffMode/OOB status reported by SoC-CP.
 *
 * @param status Output for the raw status, see SOCCP_OOB_STA_*.
 * @retval 0 on success, -EINVAL when @p status is NULL.
 */
int soccp_oob_state_get(uint16_t *status);

/**
 * 
 */
int soccp_buf_set(soccp_type_t id, uint8_t data);

/**
 * 
 */
int soccp_resp_set(uint8_t *pdata, uint16_t len);
