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

/**
 * @brief Get the current system power state.
 *
 * @note Implemented in service (service/power), called by service
 *       (service/fan, service/sci, service/thermal).
 *
 * @param state Output for the current power state.
 * @retval 0 on success.
 * @retval -EINVAL @p state is NULL.
 */
int pwr_state_get(pwr_sta_t *state);

/**
 * @brief Request a system power state transition.
 *
 * The transition is processed asynchronously by the power service thread.
 * Requesting the current state is a no-op.
 *
 * @note Implemented in service (service/power), called by service (service/soccp).
 *
 * @param state Target power state (PWR_STA_S0 ~ PWR_STA_G3).
 * @retval 0 on success.
 * @retval -EINVAL invalid @p state.
 */
int pwr_state_set(pwr_sta_t state);

/**
 * @brief Apply the hardware side of a power state transition.
 *
 * With CONFIG_PM_POLICY_CUSTOM, S0 -> MS allows the EC to enter heavy sleep
 * and MS -> S0 disables it again.
 *
 * @note Implemented in driver (driver/power.c), called by service (service/power).
 *
 * @param cur  Current power state.
 * @param next Next power state.
 * @retval 0 always.
 */
int pwr_state_chg(pwr_sta_t cur, pwr_sta_t next);
