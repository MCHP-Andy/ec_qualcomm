/*
 * @Author: andy.chang 
 * @Date: 2025-07-01 02:46:45 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-17 21:34:52
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

// #include <interface/system.h>
#include <interface/fan.h>

LOG_MODULE_REGISTER(fan, LOG_LEVEL_DBG);

#define FAN_TEMP_CHG BIT(0)
#define FAN_CFG_UPDATE BIT(1)
#define FAN_RPM_UPDATE BIT(2)

#define STACKSIZE 1024
#define PRIORITY 7

#define APP_FAN_SET_SPEED(fan_id, speed)                                       \
    do {                                                                       \
        int ret = app_fan_set_speed(fan_id, speed);                            \
        if (ret < 0) {                                                         \
            LOG_ERR("Failed to set fan%d speed: %d", fan_id, ret);             \
        } else {                                                               \
            LOG_INF("Fan%d speed set to %d%%", fan_id, speed);                 \
        }                                                                      \
    } while (0)


K_EVENT_DEFINE(fan_event);

static uint16_t temps[THERM_SRC_MAX] = {0};
static fan_ctrl_t fan_blks[FAN_ID_MAX] = {0};

int fan_tmp_get(fan_id_t tmp_src, uint16_t *tmp) {

    if (tmp_src == 0 || tmp_src >= ARRAY_SIZE(temps) || tmp == NULL) {
        return -EINVAL;
    }

    *tmp = temps[tmp_src];

    return 0;
}

int fan_tmp_set(fan_id_t tmp_src, uint16_t tmp) {

    if (tmp_src == 0 || tmp_src >= ARRAY_SIZE(temps)) {
        return -EINVAL;
    }

    temps[tmp_src] = tmp;

    return 0;
}

int fan_ctrl_get(fan_id_t fan_id, fan_ctrl_t *ctrl) {
    if (fan_id == 0 || fan_id >= ARRAY_SIZE(fan_blks) || ctrl == NULL) {
        return -EINVAL;
    }

    memcpy(ctrl, &fan_blks[fan_id], sizeof(fan_ctrl_t));

    return 0;
}

int fan_ctrl_set(fan_id_t fan_id, const fan_ctrl_t *ctrl) {
    if (fan_id == 0 || fan_id >= ARRAY_SIZE(fan_blks) || ctrl == NULL) {
        return -EINVAL;
    }

    memcpy(&fan_blks[fan_id], ctrl, sizeof(fan_ctrl_t));

    return 0;
}

static k_timeout_t rpm_update = K_MSEC(1000);

static int fan_rpm_update(fan_ctrl_t *fan_blk, uint8_t rpm) {

    if (rpm == 0) {
        // TODO: Fan off by PWM
        fan_blk->rpm = 0;
        rpm_update = K_FOREVER;
    } else {
        // TODO: mapping PWM to RPM
        // TODO: Get RPM from driver
        rpm_update = K_MSEC(1000);

        // TODO: RPM PID via PWM
    }

    return 0;
}

static inline bool check_fan_debug(fan_ctrl_t *fan_blk) {
    if (fan_blk == NULL)
        return 0;

    if (fan_blk->dbg_mode & BIT(0)) {
        if (fan_blk->dbg_mode & BIT(1)) {
            if (fan_blk->dbg_mode & BIT(2)) {
                // TODO: Set fan via pwm
                // fan_blk->dbg_pwm
            } else {
                fan_rpm_update(fan_blk, fan_blk->dbg_rpm);
            }
        } else {
            // Fan off
            fan_rpm_update(fan_blk, 0);
        }
        return true;
    }

    return false;
}

static inline int check_fan_rpm(fan_ctrl_t *fan_blk, uint16_t *rpm) {
    *rpm = 0;

    for (fan_id_t src = THERM_SRC_CPU; src < THERM_SRC_MAX; src++) {
        fan_tbl_t *tbl = fan_blk->fan_tbl[src];
        int8_t size = fan_blk->tbl_size[src] - 1;
        uint8_t temp = temps[src];

        for (; size >= 0; size--) {
            if (temp > tbl[size].temp_low && temp <= tbl[size].temp_high) {
                *rpm = (tbl[size].temp_low > *rpm) ? tbl[size].temp_low : *rpm;
            }
        }
    }

    return 0;
}

static void fan_service(void) {
    int ret = 0;
    fan_ctrl_t *fan_blk = NULL;

    fan_id_t profile = FAN_PROFILE_BEST_PERFORMANCE_CHG_IN;
    for (fan_id_t fan = FAN_ID_1; fan < ARRAY_SIZE(fan_blks); fan++) {
        fan_blk = &fan_blks[fan];
        fan_blk->id = fan;

        for (fan_id_t src = THERM_SRC_CPU; src < THERM_SRC_MAX; src++) {
            fan_tbl_t **tbl = &fan_blk->fan_tbl[src];
            uint8_t *tbl_size = &fan_blk->tbl_size[src];
            fan_tbl_get(profile, fan, src, tbl, tbl_size);
            fan_blk->profile = profile;
        }
    }

    while (1) {

        // TODO: Wait for event (temp change, update rpm, etc.)
        k_sleep(rpm_update);

        for (fan_id_t fan = FAN_ID_1; fan < ARRAY_SIZE(fan_blks); fan++) {
            fan_blk = &fan_blks[fan];

            // Check debug mode
            if (check_fan_debug(fan_blk)) {
                continue;
            }

            // TODO: check power state
            // if (pwr_state != s0) {
            //     fan_rpm_update(fan_blk, 0);
            //     continue;
            // }

            // Check fan on
            if (fan_blk->state != FAN_STA_ON) {
                fan_rpm_update(fan_blk, 0);
                continue;
            }

            // Get RPM from table
            uint16_t rpm = 0;
            ret = check_fan_rpm(fan_blk, &rpm);
            if (ret < 0) {
                // TODO: LOG
            } else {
                LOG_DBG("Fan: %d, rpm: %d", fan, rpm);
                fan_rpm_update(fan_blk, rpm);
                continue;
            }

            // TODO: RPM control for Fan
        }
    }
}

K_THREAD_DEFINE(fan_id, STACKSIZE, fan_service, NULL, NULL, NULL, PRIORITY, 0, 0);

