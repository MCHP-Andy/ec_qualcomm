/*
 * @Author: andy.chang 
 * @Date: 2025-07-01 02:46:45 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-17 21:34:52
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

// #include <interface/system.h>
#include <interface/thermal.h>

LOG_MODULE_REGISTER(thermal, LOG_LEVEL_DBG);

#define STACKSIZE 1024
#define PRIORITY 7

static therm_dev_t therm_devs[THERM_DEV_MAX] = {0};

static therm_ctrl_t therm_ctrl = {
    .therm_num = ARRAY_SIZE(therm_devs),
    .therm_blk = therm_devs,

    .adc_sample_ms = 1000,
};

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

static void therm_service(void) {
    k_timeout_t adc_wait = K_MSEC(1000);

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
