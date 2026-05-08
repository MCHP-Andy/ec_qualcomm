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

int soccp_cmd_info_get(uint8_t cmd, const soccp_cmd_t *cmd_info);

int soccp_write(uint8_t *data, uint16_t len);

int soccp_read(uint8_t *data, uint16_t len);
