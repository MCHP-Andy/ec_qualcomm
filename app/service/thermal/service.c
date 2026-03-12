/*
 * @Author: andy.chang 
 * @Date: 2025-07-01 02:46:45 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-03-12 15:17:46
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

// #include <interface/system.h>
#include <interface/thermal.h>

LOG_MODULE_REGISTER(thermal, LOG_LEVEL_INF);

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

#ifdef CONFIG_SHELL
#include <zephyr/shell/shell.h>

static int cmd_fan_set(const struct shell *sh, size_t argc, char **argv) {
    uint8_t ch = 0, duty = 0;

    ch = atoi(argv[1]);
    
    duty = atoi(argv[2]);
    if (duty > 100) {
        duty = 100;
    }

    int ret = app_fan_set_speed(ch, duty);
    if (ret < 0) {
        shell_error(sh, "Failed to set fan%d speed: %d", ch, ret);
    } else {
        shell_info(sh, "Fan%d speed set to %d%%", ch, duty);
    }
    
    return 0;
}

static int cmd_fan_get(const struct shell *sh, size_t argc, char **argv) {
    uint8_t ch = 0;
    uint16_t rpm = 0;

    ch = atoi(argv[1]);

    int ret = app_fan_get_rpm(ch, &rpm);
    if (ret < 0) {
        shell_error(sh, "Failed to get fan%d RPM: %d", ch, ret);
    } else {
        shell_info(sh, "Fan%d RPM is %d", ch, rpm);
    }

    return ret;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_fan,
	SHELL_CMD_ARG(set, NULL,
		"Set fan duty", cmd_fan_set, 2, 0),
	SHELL_CMD_ARG(off, NULL,
		"Get fan RPM", cmd_fan_get, 1, 0),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(fan, &sub_fan, "Fan commands", NULL);
#endif
