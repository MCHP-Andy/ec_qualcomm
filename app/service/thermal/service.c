/*
 * @Author: andy.chang 
 * @Date: 2025-07-01 02:46:45 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-17 19:55:25
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

// #include <interface/system.h>
#include <interface/thermal.h>
#include <interface/fan.h>

LOG_MODULE_REGISTER(thermal, LOG_LEVEL_DBG);

#define STACKSIZE 1024
#define PRIORITY 7

static uint16_t temps[THERM_SRC_MAX] = {0};
static fan_ctrl_t fan_blks[FAN_ID_MAX] = {0};
static therm_dev_t therm_devs[THERM_DEV_MAX] = {0};

static therm_ctrl_t therm_ctrl = {
    .temp_num = ARRAY_SIZE(temps),
    .temp = temps,

    .fan_num = ARRAY_SIZE(fan_blks),
    .fan_blk = fan_blks,

    .therm_num = ARRAY_SIZE(therm_devs),
    .therm_blk = therm_devs,

    .adc_sample_ms = 1000,
};

int therm_tmp_get(therm_id_t tmp_src, uint16_t *tmp) {

    if (tmp_src == 0 || tmp_src >= therm_ctrl.temp_num || tmp == NULL) {
        return -EINVAL;
    }

    *tmp = therm_ctrl.temp[tmp_src];

    return 0;
}

int therm_tmp_set(therm_id_t tmp_src, uint16_t tmp) {

    if (tmp_src == 0 || tmp_src >= therm_ctrl.temp_num) {
        return -EINVAL;
    }

    therm_ctrl.temp[tmp_src] = tmp;

    return 0;
}

int therm_fan_ctrl_get(therm_id_t fan_id, fan_ctrl_t *ctrl) {
    if (fan_id == 0 || fan_id >= therm_ctrl.fan_num || ctrl == NULL) {
        return -EINVAL;
    }

    memcpy(ctrl, &therm_ctrl.fan_blk[fan_id], sizeof(fan_ctrl_t));

    return 0;
}

int therm_fan_ctrl_set(therm_id_t fan_id, const fan_ctrl_t *ctrl) {
    if (fan_id == 0 || fan_id >= therm_ctrl.fan_num || ctrl == NULL) {
        return -EINVAL;
    }

    memcpy(&therm_ctrl.fan_blk[fan_id], ctrl, sizeof(fan_ctrl_t));

    return 0;
}

int therm_sensor_blk_get(therm_id_t dev_id, therm_dev_t *blk) {
    if (dev_id == 0 || dev_id >= therm_ctrl.therm_num || blk == NULL) {
        return -EINVAL;
    }

    memcpy(blk, &therm_ctrl.therm_blk[dev_id], sizeof(therm_dev_t));

    return 0;
}

int therm_sensor_blk_set(therm_id_t dev_id, const therm_dev_t *blk) {
    if (dev_id == 0 || dev_id >= therm_ctrl.therm_num || blk == NULL) {
        return -EINVAL;
    }

    memcpy(&therm_ctrl.therm_blk[dev_id], blk, sizeof(therm_dev_t));

    return 0;
}

int therm_adc_sample_rate_get(uint16_t *ms) {
    if (ms == NULL) {
        return -EINVAL;
    }

    *ms = therm_ctrl.adc_sample_ms;

    return 0;
}

int therm_adc_sample_rate_set(uint16_t ms) {
    if (ms < 100) {
        ms = 100;
    }

    therm_ctrl.adc_sample_ms = ms;

    return 0;
}

#define APP_FAN_SET_SPEED(fan_id, speed)                                       \
    do {                                                                       \
        int ret = app_fan_set_speed(fan_id, speed);                            \
        if (ret < 0) {                                                         \
            LOG_ERR("Failed to set fan%d speed: %d", fan_id, ret);             \
        } else {                                                               \
            LOG_INF("Fan%d speed set to %d%%", fan_id, speed);                 \
        }                                                                      \
    } while (0)

static void therm_service(void) {
    k_timeout_t adc_wait = K_MSEC(1000);


    therm_id_t profile = FAN_PROFILE_BEST_PERFORMANCE_CHG_IN;
    for (therm_id_t fan = FAN_ID_1; fan < therm_ctrl.fan_num; fan++) {
        for (therm_id_t src = THERM_SRC_CPU; src < THERM_SRC_MAX; src++) {
            fan_ctrl_t *fan_blk = &therm_ctrl.fan_blk[fan];
            fan_tbl_t **tbl = &fan_blk->fan_tbl[src];
            uint8_t *tbl_size = &fan_blk->tbl_size;
            therm_tbl_get(profile, fan, src, tbl, tbl_size);
            fan_blk->profile = profile;
        }
    }

    while (1) {

        // Wait for event (ADC sample)
        k_sleep(adc_wait);

        // Check thermal cross
        for (therm_id_t i = THERM_DEV_1; i < therm_ctrl.therm_num; i++) {
            uint16_t temp = 0;
            therm_dev_t *therm_dev = &therm_ctrl.therm_blk[i];

            // TODO: Get temp from sensor
            // board_therm_get(i, &temp);

            // Update temp
            therm_dev->temp = temp;

            // Check 
            if (temp > therm_dev->psv) {
                LOG_WRN("PSV");
            }

            if (temp > therm_dev->cr3) {
                LOG_WRN("CR3");
            }

            if (temp > therm_dev->hot) {
                LOG_WRN("HOT");
            }

            if (temp > therm_dev->crt) {
                LOG_WRN("CRT");
            }
            
        }


        adc_wait = K_MSEC(therm_ctrl.adc_sample_ms);
    }
}

K_THREAD_DEFINE(therm_id, STACKSIZE, therm_service, NULL, NULL, NULL, PRIORITY, 0, 0);



static void fan_service(void) {
    while (1) {

        // TODO: Wait for event (temp change, update rpm, etc.)
        k_sleep(K_SECONDS(1));

        // Get pwm from table
        for (therm_id_t fan = FAN_ID_1; fan < therm_ctrl.fan_num; fan++) {
            fan_ctrl_t *fan_blk = &therm_ctrl.fan_blk[fan];
            uint16_t rpm = 0;

            for (therm_id_t src = THERM_SRC_CPU; src < THERM_SRC_MAX; src++) {
                fan_tbl_t *tbl = fan_blk->fan_tbl[src];
                int8_t size = fan_blk->tbl_size - 1;
                uint8_t temp = therm_ctrl.temp[src];

                for (; size >= 0; size--) {
                    if (temp > tbl[size].temp_low &&
                        temp <= tbl[size].temp_high) {
                        rpm = (tbl[size].temp_low > rpm) ? tbl[size].temp_low
                                                         : rpm;
                    }
                }
            }

            LOG_DBG("Fan: %d, rpm: %d", fan, rpm);
            // TODO: Set RPM to Fan controller

        }
    }
}

K_THREAD_DEFINE(fan_id, STACKSIZE, fan_service, NULL, NULL, NULL, PRIORITY, 0, 0);

