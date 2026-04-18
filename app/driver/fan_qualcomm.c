/*
 * @Author: andy.chang
 * @Date: 2025-06-29 17:06:01
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-17 01:16:04
 */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(fan_qualcomm, LOG_LEVEL_WRN);

static const struct fan_dev_t {
    const struct pwm_dt_spec fan;
    const struct device *tach;
} fan_dev_list[] = {
    {
    #if DT_NODE_HAS_STATUS(DT_ALIAS(cpu_fan), okay)
        .fan = PWM_DT_SPEC_GET(DT_ALIAS(cpu_fan)),
    #endif
    #if DT_NODE_HAS_STATUS(DT_ALIAS(cpu_tach), okay)
        .tach = DEVICE_DT_GET(DT_ALIAS(cpu_tach)),
    #endif
    },
    {
    #if DT_NODE_HAS_STATUS(DT_ALIAS(base_fan), okay)
        .fan = PWM_DT_SPEC_GET(DT_ALIAS(base_fan)),
    #endif
    #if DT_NODE_HAS_STATUS(DT_ALIAS(base_tach), okay)
        .tach = DEVICE_DT_GET(DT_ALIAS(base_tach)),
    #endif
    },
};

int app_fan_set_speed(int fan_id, uint8_t speed) {
    if (fan_id >= ARRAY_SIZE(fan_dev_list)) {
        LOG_ERR("Invalid fan ID: %d", fan_id);
        return -EINVAL;
    }

    if (speed > 100) {
        speed = 100; // Cap speed at 100%
    }

    int ret = 0;
    uint32_t pulse = (uint64_t)fan_dev_list[fan_id].fan.period * speed / 100;

    ret = pwm_set_pulse_dt(&fan_dev_list[fan_id].fan, pulse);
    if (ret < 0) {
        LOG_ERR("Failed to set speed for fan%d: %d", fan_id, ret);
    } else {
        LOG_INF("Set fan%d speed to %d%%", fan_id, speed);
    }

    return ret;
}

int app_fan_get_rpm(int fan_id, uint16_t *rpm) {
    if (fan_id >= ARRAY_SIZE(fan_dev_list)) {
        LOG_ERR("Invalid fan ID: %d", fan_id);
        return -EINVAL;
    }

    int ret = 0;
    const struct device *tach = fan_dev_list[fan_id].tach;
    struct sensor_value val = {0};

    ret = sensor_sample_fetch_chan(tach, SENSOR_CHAN_RPM);
    if (ret) {
        LOG_ERR("Failed to fetch RPM sample for fan%d: %d", fan_id, ret);
        return ret;
    }

    ret = sensor_channel_get(tach, SENSOR_CHAN_RPM, &val);
    if (ret) {
        LOG_ERR("Failed to get RPM for fan%d: %d", fan_id, ret);
        return ret;
    }

    *rpm = (uint16_t)val.val1;
    LOG_INF("Fan%d RPM is %d", fan_id, *rpm);

    return ret;
}

#include <zephyr/init.h>

static int init_config(void) {
    int ret = 0;

    LOG_INF("Initializing thermal configuration...");

    for (size_t i = 0; i < ARRAY_SIZE(fan_dev_list); i++) {

        if (!pwm_is_ready_dt(&fan_dev_list[i].fan)) {
            LOG_ERR("fan%d not ready", i);
            return -ENODEV;
        }

        ret = pwm_set_pulse_dt(&fan_dev_list[i].fan, 0);
        if (ret < 0) {
            LOG_ERR("Failed to set PWM for fan%d: %d", i, ret);
            return ret;
        }

        if (!device_is_ready(fan_dev_list[i].tach)) {
            LOG_ERR("tach%d not ready", i);
            return -ENODEV;
        }
    }

    return ret;
}

SYS_INIT(init_config, APPLICATION, 0);

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
