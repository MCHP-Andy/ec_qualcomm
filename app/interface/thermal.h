/*
 * @Author: andy.chang 
 * @Date: 2026-04-17 00:05:32 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-17 18:12:46
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

    // Thermistors device
    THERM_DEV_1 = 1,
    THERM_DEV_2 = 2,
    THERM_DEV_3 = 3,
    THERM_DEV_MAX,

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

} therm_id_t;




typedef struct fan_tbl_t{
    uint8_t rpm; // Unit in 100 RPM (Eg: 5 => 500 RPM)
    uint8_t temp_high;
    uint8_t temp_low;
} fan_tbl_t;

typedef struct fan_ctrl_t{
    therm_id_t state; // 0x00: OFF, 0x01: ON, 0x02: Failure
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

    uint8_t tbl_size; // LUT number for 2 sources. 0: Dummy, 1: CPU, 2: Skin
    fan_tbl_t *fan_tbl[THERM_SRC_MAX]; // Fan profile table pointer for 2 sources. 0: Dummy, 1: CPU, 2: Skin

    uint8_t dbg_mode; // BIT0: 0-Normal, 1-Debug mode
                      // BIT1: 0-Fan off, 1-Fan on
                      // BIT2: 0-Debug RPM, 1-Debug PWM
    uint16_t dbg_rpm; // Debug RPM value in the unit of RPM
    uint8_t dbg_pwm;  // Debug PWM value (0-255)
} fan_ctrl_t;


typedef struct therm_dev_t {
    uint16_t temp; // Temperature value in the unit of 0.1 deg C.(Eg:251 => 25.1 deg C)
                   // Range : -40 to +125 deg C
                   // Note : If the MSB bit is set, then the temperature value is negative.
    
    uint8_t psv; // deg C
    uint8_t cr3; // deg C
    uint8_t hot; // deg C
    uint8_t crt; // deg C
} therm_dev_t;

typedef struct therm_ctrl_t{
    uint8_t temp_num; // Number of temperature sources
    uint16_t *temp; // Temperature value in the unit of 0.1 deg C.(Eg: 651 => 65.1deg C)
                    // 0: Dummy, 1: CPU, 2: Skin
    
    uint8_t fan_num; // Number of fans
    fan_ctrl_t *fan_blk; // Fan control information. 0: Dummy, 1: CPU, 2: Skin

    uint8_t therm_num; // Number of thermistors
    therm_dev_t *therm_blk; // Thermal device information. 0: Dummy, 1~4: Thermistor 0~3

    uint16_t adc_sample_ms; // ADC sample rate in the unit of ms (Min: 100ms)
} therm_ctrl_t;


int therm_tmp_get(therm_id_t tmp_src, uint16_t *tmp);
int therm_tmp_set(therm_id_t tmp_src, uint16_t tmp);

int therm_fan_ctrl_get(therm_id_t fan_id, fan_ctrl_t *ctrl);
int therm_fan_ctrl_set(therm_id_t fan_id, const fan_ctrl_t *ctrl);

int therm_sensor_blk_get(therm_id_t dev_id, therm_dev_t *blk);
int therm_sensor_blk_set(therm_id_t dev_id, const therm_dev_t *blk);

int therm_adc_sample_rate_get(uint16_t *ms);
int therm_adc_sample_rate_set(uint16_t ms);

int therm_tbl_get(therm_id_t profile, therm_id_t fan_id, therm_id_t tmp_src,
                  fan_tbl_t **tbl, uint8_t *len);
