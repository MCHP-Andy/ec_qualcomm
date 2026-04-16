/*
 * @Author: andy.chang 
 * @Date: 2026-04-17 00:05:32 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-17 03:32:05
 */

#pragma once

#include <stdio.h>
#include <stdint.h>

typedef struct fan_tbl_t{
    uint8_t rpm; // Unit in 100 RPM (Eg: 5 => 500 RPM)
    uint8_t temp_high;
    uint8_t temp_low;
} fan_tbl_t;

typedef struct fan_ctrl_t{
    uint8_t state; // 0x00: OFF, 0x01: ON, 0x02: Failure
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

    uint8_t lut[3]; // LUT number for 2 sources. 0: Dummy, 1: CPU, 2: Skin
    fan_tbl_t *fan_tbl[3]; // Fan profile table pointer for 2 sources. 0: Dummy, 1: CPU, 2: Skin

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

typedef struct thermal_ctrl_t{
    uint16_t temp[3]; // Temperature value in the unit of 0.1 deg C.(Eg: 651 => 65.1deg C)
                      // 0: Dummy, 1: CPU, 2: Skin

    fan_ctrl_t fan_ctrl[3]; // Fan control information. 0: Dummy
    therm_dev_t therm_dev[4]; // Thermal device information. 0: Dummy

    uint16_t adc_sample_ms; // ADC sample rate in the unit of ms (Min: 100ms)
} thermal_ctrl_t;


int thermal_ctrl_get(thermal_ctrl_t *ctrl);

int thermal_ctrl_set(const thermal_ctrl_t *ctrl);

int board_lut_get(uint8_t profile, uint8_t fan_id, uint8_t tmp_src,
                  fan_tbl_t **tbl, uint8_t *len);
                  