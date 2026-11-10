/*
 * @Author: andy.chang 
 * @Date: 2025-07-01 02:43:05 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-20 23:36:56
 */

#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

// typedef enum {
//     THERM_SRC_CPU = 1,
//     THERM_SRC_SKIN,
//     THERM_SRC_MAX
// } therm_src_t;

// typedef enum {
//     FAN_ID_1 = 1,
//     FAN_ID_2,
//     FAN_ID_MAX
// } fan_id_t;

// typedef enum {
//     FAN_PROFILE_BATTERY_SAVER = 1,
//     FAN_PROFILE_BETTER_BAT_CHG_IN,
//     FAN_PROFILE_BETTER_BAT_CHG_OUT,
//     FAN_PROFILE_BETTER_PERFORMANCE_CHG_IN,
//     FAN_PROFILE_BETTER_PERFORMANCE_CHG_OUT,
//     FAN_PROFILE_BEST_PERFORMANCE_CHG_IN,
//     FAN_PROFILE_BEST_PERFORMANCE_CHG_OUT,
//     FAN_PROFILE_MAX
// } fan_profile_id_t;

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

typedef struct fan_lut_t {
    uint8_t temp_src;
    uint8_t tbl_size;
    fan_tbl_t tbl[10];
} fan_lut_t;

typedef struct fan_ctrl_t{
    fan_id_t id; // ID for interface
    fan_id_t state; // 0x00: OFF, 0x01: ON, 0x02: Failure
    uint16_t rpm; // Fan speed in RPM (Revolutions Per Minute)
    
    uint16_t trip_low; // Trip point low in the unit of RPM.
    uint16_t trip_high; // Trip point high in the unit of RPM.

    fan_id_t profile; // 0x00: Invalid
                      // 0x01: Battery saver
                      // 0x02: Better battery with charger plugged in
                      // 0x03: Better battery with charger plugged out
                      // 0x04: Better performance with charger plugged in
                      // 0x05: Better performance with charger plugged out
                      // 0x06: Best performance with charger plugged in
                      // 0x07: Best performance with charger plugged out

    fan_lut_t *lut[THERM_SRC_MAX - THERM_SRC_CPU]; // [0]: SoC Tj, [1]: Tskin

    uint8_t dbg_mode; // BIT0: 0-Normal, 1-Debug mode
                      // BIT1: 0-Fan off, 1-Fan on
                      // BIT2: 0-Debug RPM, 1-Debug PWM
    uint16_t dbg_rpm; // Debug RPM value in the unit of RPM
    uint8_t dbg_pwm;  // Debug PWM value (0-255)
} fan_ctrl_t;

/**
 * @brief Get the temperature last reported for a fan control source.
 *
 * @note Implemented in service (service/fan), not called anywhere yet.
 *
 * @param tmp_src Temperature source (THERM_SRC_CPU or THERM_SRC_SKIN).
 * @param tmp     Output for the temperature in 0.1 C.
 * @retval 0 on success.
 * @retval -EINVAL invalid @p tmp_src or @p tmp is NULL.
 */
int fan_tmp_get(fan_id_t tmp_src, uint16_t *tmp);

/**
 * @brief Report a new temperature for a fan control source.
 *
 * Stores the value and wakes the fan service to re-evaluate fan speed against
 * the active LUT.
 *
 * @note Implemented in service (service/fan), called by service (service/fan).
 *
 * @param tmp_src Temperature source (THERM_SRC_CPU or THERM_SRC_SKIN).
 * @param tmp     Temperature in 0.1 C (Eg: 350 => 35.0 C).
 * @retval 0 on success.
 * @retval -EINVAL invalid @p tmp_src.
 */
int fan_tmp_set(fan_id_t tmp_src, uint16_t tmp);

/**
 * @brief Update the cached RPM of a fan in the fan service.
 *
 * Only updates the fan control block; it does not drive the fan.
 * Use fan_rpm_set() to change the actual fan speed.
 *
 * @note Implemented in service (service/fan), not called anywhere yet.
 *
 * @param fan_id Fan ID (FAN_ID_1 or FAN_ID_2).
 * @param rpm    Fan speed in RPM.
 * @retval 0 on success.
 * @retval -EINVAL invalid @p fan_id.
 */
int fan_rpm_write(fan_id_t fan_id, uint16_t rpm);

/**
 * @brief Get the fan LUT for a profile, fan and temperature source.
 *
 * @note Implemented in driver (driver/fan.c), called by service (service/fan).
 *
 * @param profile  Fan profile (FAN_PROFILE_*).
 * @param fan_id   Fan ID (FAN_ID_1 or FAN_ID_2).
 * @param temp_src Temperature source (THERM_SRC_CPU or THERM_SRC_SKIN).
 * @param lut      Output for a pointer to the LUT, which the caller may modify.
 * @retval 0 on success.
 * @retval -EINVAL invalid parameter or @p lut is NULL.
 */
int fan_lut_get(fan_id_t profile, fan_id_t fan_id, fan_id_t temp_src,
                fan_lut_t **lut);

/**
 * @brief Apply the SoC-CP power constraint on the fan(s).
 *
 * SoC-CP notifies the EC about power constraints to avoid brown out of the
 * device. While constrained the fan(s) shall remain OFF regardless of the
 * thermal LUT. Also drives the fan 12V enable line.
 *
 * @note Implemented in driver (driver/fan.c), called by service
 *       (service/fan, service/soccp).
 *
 * @param allow_on false: FAN(s) should remain OFF, true: FAN(s) can turn ON.
 * @retval 0 always.
 */
int fan_constraint_set(bool allow_on);

/**
 * @brief Get the current SoC-CP fan power constraint.
 *
 * @note Implemented in driver (driver/fan.c), called by service (service/fan).
 *
 * @param allow_on Output, false: FAN(s) should remain OFF, true: FAN(s) can turn ON.
 * @retval 0 on success.
 * @retval -EINVAL @p allow_on is NULL.
 */
int fan_constraint_get(bool *allow_on);

/**
 * @brief Set the fan PWM duty cycle.
 *
 * The duty is forced to 100% while max-RPM or thermal throttling is active.
 *
 * @note Implemented in driver (driver/fan.c), called by service (service/fan).
 *
 * @param id  Fan ID (FAN_ID_1 or FAN_ID_2).
 * @param pwm Duty cycle, 0 ~ 65535 maps to 0 ~ 100%.
 * @retval 0 on success.
 * @retval -EINVAL invalid @p id or fan device.
 * @retval <0 PWM driver error.
 */
int fan_pwm_set(fan_id_t id, uint16_t pwm);

/**
 * @brief Set the fan speed in RPM.
 *
 * The RPM is converted to a PWM duty by linear interpolation over the fan
 * calibration table, then applied with fan_pwm_set().
 *
 * @note Implemented in driver (driver/fan.c), called by service (service/fan).
 *
 * @param id  Fan ID (FAN_ID_1 or FAN_ID_2).
 * @param rpm Target fan speed in RPM.
 * @retval 0 on success.
 * @retval -EINVAL invalid @p id.
 * @retval <0 PWM driver error.
 */
int fan_rpm_set(fan_id_t id, uint16_t rpm);

/**
 * @brief Read the fan speed measured by the tachometer.
 *
 * @note Implemented in driver (driver/fan.c), called by service (service/fan).
 *
 * @param id   Fan ID (FAN_ID_1 or FAN_ID_2).
 * @param prpm Output for the fan speed in RPM.
 * @retval 0 on success.
 * @retval -EINVAL invalid @p id, tach device or @p prpm is NULL.
 * @retval <0 sensor driver error.
 */
int fan_rpm_get(fan_id_t id, uint16_t *prpm);
