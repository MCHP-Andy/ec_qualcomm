/*
 * @Author: andy.chang 
 * @Date: 2026-04-17 00:05:32 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-18 16:42:47
 */

#pragma once

#include <stdio.h>
#include <stdint.h>

typedef enum {
    // Thermistors device
    THERM_DEV_1 = 1,
    THERM_DEV_2 = 2,
    THERM_DEV_3 = 3,
    THERM_DEV_MAX,
} therm_id_t;

typedef struct therm_dev_t {
    therm_id_t id; // ID for interface
    int16_t temp;  // Temperature value in the unit of 0.1 deg C.(Eg:251 => 25.1 deg C)
                   // Range : -40 to +125 deg C
                   // Note : If the MSB bit is set, then the temperature value is negative.
    
    uint8_t psv; // deg C
    uint8_t cr3; // deg C
    uint8_t hot; // deg C
    uint8_t crt; // deg C
} therm_dev_t;

typedef struct therm_ctrl_t{
    uint8_t therm_num; // Number of thermistors
    therm_dev_t *therm_blk; // Thermal device information. 0: Dummy, 1~4: Thermistor 0~3

    uint16_t sample_ms; // ADC sample rate in the unit of ms (Min: 100ms)
} therm_ctrl_t;

int therm_sample_get(therm_id_t id, int16_t *temp);
