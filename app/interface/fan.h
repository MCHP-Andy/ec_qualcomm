/*
 * @Author: andy.chang 
 * @Date: 2025-07-01 02:43:05 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-17 00:05:40
 */

#pragma once

#include <stdio.h>
#include <stdint.h>

typedef enum {
    // Temperature source
    THERM_SRC_CPU = 1,
    THERM_SRC_SKIN = 2,
    THERM_SRC_MAX,

    // Fan ID
    FAN_ID_1 = 1,
    FAN_ID_2 = 2,
    FAN_ID_MAX,

    // FAN profile
    FAN_PROFILE_BATTERY_SAVER = 1,
    FAN_PROFILE_BETTER_BAT_CHG_IN = 2,
    FAN_PROFILE_BETTER_BAT_CHG_OUT = 3,
    FAN_PROFILE_BETTER_PERFORMANCE_CHG_IN = 4,
    FAN_PROFILE_BETTER_PERFORMANCE_CHG_OUT = 5,
    FAN_PROFILE_BEST_PERFORMANCE_CHG_IN = 6,
    FAN_PROFILE_BEST_PERFORMANCE_CHG_OUT = 7,
    FAN_PROFILE_MAX,

    // FAN status
    FAN_STA_OFF = 0,
    FAN_STA_ON = 1,
    FAN_STA_FAIL = 2,
    FAN_STA_MAX,

} fan_id_t;

typedef struct fan_tbl_t{
    uint8_t rpm; // Unit in 100 RPM (Eg: 5 => 500 RPM)
    uint8_t temp_high;
    uint8_t temp_low;
} fan_tbl_t;

typedef struct fan_ctrl_t{
    fan_id_t id; // ID for interface
    fan_id_t state; // 0x00: OFF, 0x01: ON, 0x02: Failure
    uint16_t rpm; // Fan speed in RPM (Revolutions Per Minute)
    
    uint16_t trip_low; // Trip point low in the unit of RPM.
    uint16_t trip_high; // Trip point high in the unit of RPM.

    uint8_t profile; // 0x00: Invalid
                     // 0x01: Battery saver
                     // 0x02: Better battery with charger plugged in
                     // 0x03: Better battery with charger plugged out
                     // 0x04: Better performance with charger plugged in
                     // 0x05: Better performance with charger plugged out
                     // 0x06: Best performance with charger plugged in
                     // 0x07: Best performance with charger plugged out

    uint8_t tbl_size[THERM_SRC_MAX]; // LUT number for 2 sources. 0: Dummy, 1: CPU, 2: Skin
    fan_tbl_t *fan_tbl[THERM_SRC_MAX]; // Fan profile table pointer for 2 sources. 0: Dummy, 1: CPU, 2: Skin

    uint8_t dbg_mode; // BIT0: 0-Normal, 1-Debug mode
                      // BIT1: 0-Fan off, 1-Fan on
                      // BIT2: 0-Debug RPM, 1-Debug PWM
    uint16_t dbg_rpm; // Debug RPM value in the unit of RPM
    uint8_t dbg_pwm;  // Debug PWM value (0-255)
} fan_ctrl_t;

int fan_tmp_get(fan_id_t tmp_src, uint16_t *tmp);
int fan_tmp_set(fan_id_t tmp_src, uint16_t tmp);

int fan_ctrl_get(fan_id_t fan_id, fan_ctrl_t *ctrl);
int fan_ctrl_set(fan_id_t fan_id, const fan_ctrl_t *ctrl);

int fan_tbl_get(fan_id_t profile, fan_id_t fan_id, fan_id_t tmp_src,
                fan_tbl_t **tbl, uint8_t *len);


/**
 * @brief Set the speed of a fan.
 * 
 * @param fan_id The ID of the fan to set the speed for.
 * @param speed The speed percentage (0-100).
 * @return int 0 on success, negative error code on failure.
 */
int app_fan_set_speed(int fan_id, uint8_t speed);

/**
 * @brief Get the RPM of a fan.
 * 
 * @param fan_id The ID of the fan to get the RPM for.
 * @param rpm Pointer to store the RPM value.
 * @return int 0 on success, negative error code on failure.
 */
int app_fan_get_rpm(int fan_id, uint16_t *rpm);
