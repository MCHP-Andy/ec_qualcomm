/*
 * @Author: andy.chang 
 * @Date: 2026-04-16 16:43:54 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-16 16:46:58
 */

#pragma once

#include <stdio.h>
#include <stdint.h>

// ACPI commands
// EC Version and Capabilities
#define EC_DEV_FW_VER 0x0E
#define EC_DEV_FW_VER_AND_LOWEST_SUPPORTED_FW_VER 0x0F
#define EC_DEV_FLASHING_CAP 0xB0
#define EC_DEV_THERMAL_CAP 0x42
#define EC_DEV_ACTIVE_COOLING_IF_VER_AND_CAP 0x44
#define EC_ACPI_WHOAMI_IF 0x43
#define EC_DEV_ID 0x06
// ACPI commands end


/*
 * @brief Handler for EC_DEV_FW_VER command
 */
int acpi_dev_fw_ver(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp, uint8_t resp_len);
