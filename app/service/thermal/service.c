/*
 * @Author: andy.chang 
 * @Date: 2025-07-01 02:46:45 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-17 18:12:31
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

// #include <interface/system.h>
#include <interface/thermal.h>
#include <interface/fan.h>

LOG_MODULE_REGISTER(thermal, LOG_LEVEL_INF);

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

enum {
    THERMAL_STATE_IDLE = 0,
    THERMAL_STATE_INITIAL,
    THERMAL_STATE_RUNNING,
    THERMAL_STATE_CLOSE,
};

typedef struct {
    uint16_t thre;
    uint8_t pwm;
    uint8_t hyst;
} fan_profile_t;

static const fan_profile_t fan_profile[] = {
    {.thre = 20, .pwm = 30, .hyst = 10},
    {.thre = 40, .pwm = 45, .hyst = 10},
    {.thre = 70, .pwm = 70, .hyst = 10},
    {.thre = 85, .pwm = 100, .hyst = 0}, // No need hysteresis
};

static uint8_t fan_tbl_size = ARRAY_SIZE(fan_profile);
static fan_profile_t *fan_tbl = (fan_profile_t *)fan_profile;
static uint8_t pre_tmp[2];
static uint16_t cur_tmp = 0;
static K_SEM_DEFINE(thermal_sem, 0, 1);

/**
 * @brief Read the thermal sensors and update the current temperature
 * 
 */
static inline void read_thermal_sensor(void) {
    // TODO: Read the current temperature from sensors
    cur_tmp = 45;
}

/**
 * @brief Update fan speed based on the current temperature
 *
 * This function iterates through the fan profile table and sets the fan speed
 * according to the current temperature. If no profile matches, it sets the fan
 * speed to 0.
 */
static inline void fan_update(void) {
    for (size_t idx = 0; idx < 2; idx++) {
        int ret;
        uint16_t rpm = 0;
        bool pwm_updated = false;

        for (int i = fan_tbl_size - 1; i >= 0; i--) {
            uint16_t thre = fan_tbl[i].thre;
            uint8_t pwm = fan_tbl[i].pwm;
            uint8_t hyst = fan_tbl[i].hyst;

            if (cur_tmp >= thre) {
                if ((pre_tmp[idx] > cur_tmp) && hyst > 0) {
                    if ((i + 1 < fan_tbl_size) &&
                        ((fan_tbl[i + 1].thre - cur_tmp) < hyst)) {
                        // keep origin PWM
                    } else {
                        APP_FAN_SET_SPEED(idx, pwm);
                    }
                } else {
                    APP_FAN_SET_SPEED(idx, pwm);
                }
                pwm_updated = true;
                break;
            }
        }

        if (pwm_updated == false) {
            // If no PWM updated, set to 0
            APP_FAN_SET_SPEED(idx, 0);
        }

        pre_tmp[idx] = cur_tmp;

        ret = app_fan_get_rpm(idx, &rpm);
        if (ret < 0) {
            LOG_ERR("Failed to get fan%d RPM: %d", idx, ret);
        } else {
            LOG_INF("Fan%d RPM is %d", idx, rpm);
        }
    }
}

static void service(void) {
    uint8_t state = THERMAL_STATE_RUNNING; // THERMAL_STATE_IDLE;
    k_timeout_t wait_time = K_MSEC(100);

    k_sleep(wait_time);

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

    // wait_time = K_FOREVER;
    wait_time = K_MSEC(5000);

    while (1) {

        switch (state) {
        case THERMAL_STATE_IDLE: // idle
            LOG_INF("Thermal service is idle, waiting for initialization");
            k_sem_take(&thermal_sem, wait_time);
        case THERMAL_STATE_INITIAL: // initial
            LOG_INF("Thermal service is initializing");
            // TODO: Initialize thermal sensors and fans

            // Reset previous temperature
            memset(pre_tmp, 0xff, sizeof(pre_tmp));

            state = THERMAL_STATE_RUNNING; // Change state to running
            LOG_INF("Thermal service is running");
        case THERMAL_STATE_RUNNING: // running
            // Read the current temperature from TMP451 sensors
            read_thermal_sensor();

            // Update the fan speed based on the temperature
            fan_update();

            k_sem_take(&thermal_sem, wait_time);
            break;

        case THERMAL_STATE_CLOSE: // close
            LOG_INF("Thermal service is closing");
            state = THERMAL_STATE_IDLE; // Change state to idle
            break;

        default:
            state = THERMAL_STATE_IDLE; // Change state to idle
            break;
        }

        // TODO: check state change request, e.g. from system event or shell command
    }
}

K_THREAD_DEFINE(thermal_id, STACKSIZE, service, NULL, NULL, NULL, PRIORITY, 0, 0);


static void therm_service(void) {
    while (1) {

        // TODO: Wait for event (ADC sample)
        k_sleep(K_SECONDS(1));

        // TODO: check thermal cross


    }
}

K_THREAD_DEFINE(therm_id, STACKSIZE, therm_service, NULL, NULL, NULL, PRIORITY, 0, 0);



static void fan_service(void) {
    while (1) {

        // TODO: Wait for event (temp change, update rpm, etc.)
        k_sleep(K_SECONDS(1));
    }
}

K_THREAD_DEFINE(fan_id, STACKSIZE, fan_service, NULL, NULL, NULL, PRIORITY, 0, 0);

