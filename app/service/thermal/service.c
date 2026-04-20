/*
 * @Author: andy.chang 
 * @Date: 2025-07-01 02:46:45 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-20 14:09:44
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <interface/system.h>
#include <interface/thermal.h>

LOG_MODULE_REGISTER(thermal, LOG_LEVEL_INF);

static therm_dev_t therm_devs[THERM_DEV_MAX] = {
    [THERM_DEV_1] =
        {
            .id = THERM_DEV_1,
            .psv = 80,
            .cr3 = 80,
            .hot = 80,
            .crt = 80,
        },
    [THERM_DEV_2] =
        {
            .id = THERM_DEV_1,
            .psv = 80,
            .cr3 = 80,
            .hot = 80,
            .crt = 80,
        },
    [THERM_DEV_3] =
        {
            .id = THERM_DEV_1,
            .psv = 80,
            .cr3 = 80,
            .hot = 80,
            .crt = 80,
        },
};

static therm_ctrl_t therm_ctrl = {
    .therm_num = ARRAY_SIZE(therm_devs),
    .therm_blk = therm_devs,

    .sample_ms = 1000,
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

    *ms = therm_ctrl.sample_ms;

    return 0;
}

int therm_adc_sample_rate_set(uint16_t ms) {
    if (ms < 100) {
        ms = 100;
    }

    therm_ctrl.sample_ms = ms;

    return 0;
}

static void service(void) {
    k_timeout_t adc_wait = K_MSEC(1000);

    while (1) {

        // Wait for event (ADC sample)
        k_sleep(adc_wait);

        // Check thermal cross
        for (therm_id_t i = THERM_DEV_1; i < therm_ctrl.therm_num; i++) {
            uint16_t temp = 0;
            therm_dev_t *therm_dev = &therm_ctrl.therm_blk[i];

            // Get temp from sensor
            int ret = therm_sample_get(i, &temp);
            if (ret < 0) {
                LOG_ERR("Failed to get therm%d : %d", i, ret);
                continue;
            }

            // Update temp
            therm_dev->temp = temp;
            LOG_DBG("Thermal %d: temp: %d C", i, temp);

            // Check 
            if (temp > therm_dev->psv) {
                LOG_WRN("Thermal %d: PSV", i);
            }

            if (temp > therm_dev->cr3) {
                LOG_WRN("Thermal %d: CR3", i);
            }

            if (temp > therm_dev->hot) {
                LOG_WRN("Thermal %d: HOT", i);
            }

            if (temp > therm_dev->crt) {
                LOG_WRN("Thermal %d: CRT", i);
            }
            
        }

        adc_wait = K_MSEC(therm_ctrl.sample_ms);
    }
}

K_THREAD_DEFINE(therm_id, APP_STACK_MIN, service, NULL, NULL, NULL, APP_PRIO_M,
                0, 0);
