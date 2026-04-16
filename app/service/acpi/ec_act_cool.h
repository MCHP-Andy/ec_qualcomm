/*
 * @Author: andy.chang 
 * @Date: 2026-04-16 16:43:54 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-16 16:52:30
 */

#pragma once

#include <stdio.h>
#include <stdint.h>

// ACPI commands
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
#define EC_THERMISTORS 0x29
// 0x2A and 0x2B for thermistor 2 and 3
#define EC_FAN_DEBUG_CTRL 0x30
#define EC_THERMISTOR_TEMP_THRE 0x32
#define EC_THERMISTOR_SAMPLING_RATE 0x34
#define EC_FUNC_FLAG 0x35
#define EC_ACTIVE_COOLING_SCI_EVENT 0x05
// ACPI commands end

