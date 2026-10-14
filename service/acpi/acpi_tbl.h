/*
 * @Author: andy.chang 
 * @Date: 2026-04-21 21:46:34 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-23 23:14:14
 */

#pragma once

#include <errno.h>
#include <interface/acpi.h>
#include <stdint.h>

#include <interface/acpi.h>

#include <zephyr/logging/log.h>


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


// ACPI commands
// EC Version and Capabilities
#define EC_DEV_FW_VER 0x0E
#define EC_DEV_FW_VER_AND_LOWEST_SUPPORTED_FW_VER 0x0F
#define EC_DEV_FLASHING_CAP 0xB0
#define EC_DEV_THERMAL_CAP 0x42
#define EC_DEV_ACTIVE_COOLING_IF_VER_AND_CAP 0x44
#define EC_ACPI_WHOAMI_IF 0x43
#define EC_DEV_ID 0x06

// EC Active Cooling Commands
#define SOC_TO_EC_TEMP 0x20
#define EC_FAN_STATUS 0x21
#define EC_FAN_RPM 0x22
#define SOC_TO_EC_MODERN_STANDBY_NOTIFI 0x23
#define EC_FAN_PROFILE 0x24
#define EC_FAN_TRIP_POINT 0x25
#define EC_FAN_PROFILE_NUM 0x26
#define EC_FAN_LUT_NUM 0x27
#define EC_FAN_LUT 0x28
#define EC_THERMISTOR1 0x29
#define EC_THERMISTOR2 0x2A
#define EC_THERMISTOR3 0x2B
#define EC_FAN_DEBUG_CTRL 0x30
#define EC_THERMISTOR_TEMP_THRE 0x32
#define EC_THERMISTOR_SAMPLING_RATE 0x34
#define EC_FUNC_FLAG 0x35
#define EC_ACTIVE_COOLING_SCI_EVENT 0x05

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
