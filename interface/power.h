/*
 * @Author: andy.chang 
 * @Date: 2026-04-18 18:01:20 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-21 15:23:32
 */

#pragma once

#include <stdio.h>
#include <stdint.h>

typedef enum {
    PWR_STA_S0 = 1,
    PWR_STA_MS,
    PWR_STA_S3,
    PWR_STA_S4,
    PWR_STA_S5,
    PWR_STA_G3,

    PWR_STA_MAX,
} pwr_sta_t;

int pwr_state_get(pwr_sta_t *state);

int pwr_state_set(pwr_sta_t state);
