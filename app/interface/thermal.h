/*
 * @Author: andy.chang 
 * @Date: 2025-07-01 02:43:05 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-03-12 13:54:19
 */

#pragma once

#include <zephyr/kernel.h>

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
