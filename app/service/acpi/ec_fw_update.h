/*
 * @Author: andy.chang 
 * @Date: 2026-04-16 16:43:54 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-16 16:54:24
 */

#pragma once

#include <stdio.h>
#include <stdint.h>

// ACPI commands
// EC Firmware Update Commands
#define EC_DEV_FW_CORRUPTION_STATUS 0xEB

#define EC_FW_CODE_MIRROR 0xBB
// TODO: support different sub-commands for 0xBB

#define EC_READ_CRC 0xA2
#define EC_STATE_AND_WP_STATUS 0xEC
#define EC_ERASE_MEM_REGION 0xAE
#define EC_ERASE_MEM_PARTITION 0xAF
#define EC_READ_MEM_REGION 0xA7
#define EC_READ_MEM_REGION_BUF 0xA1
#define EC_WRITE_MEM_REGION 0xA0
#define EC_WRITE_MEM_REGION_BUF 0xAA
// ACPI commands end

