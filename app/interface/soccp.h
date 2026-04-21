/*
 * @Author: andy.chang 
 * @Date: 2026-04-21 22:29:58 
 * @Last Modified by:   andy.chang 
 * @Last Modified time: 2026-04-21 22:29:58 
 */

#pragma once

#include <stdint.h>
#include <stdio.h>

#define SOCCP_RECE_LEN 64
#define SOCCP_RESP_LEN 64

int soccp_write(uint8_t *data, uint16_t len);

int soccp_read(uint8_t *data, uint16_t len);
