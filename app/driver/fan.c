/*
 * @Author: andy.chang 
 * @Date: 2026-04-18 11:29:06 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-23 00:36:37
 */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include <interface/fan.h>

LOG_MODULE_REGISTER(fan_dev, LOG_LEVEL_DBG);

static const struct fan_dev_t {
    const struct pwm_dt_spec fan;
    const struct device *tach;
} dev_list[] = {
    [FAN_ID_1] =
        {
            .fan = PWM_DT_SPEC_GET(DT_ALIAS(cpu_fan)),
            .tach = DEVICE_DT_GET(DT_ALIAS(cpu_tach)),
        },
    [FAN_ID_2] =
        {
            .fan = PWM_DT_SPEC_GET(DT_ALIAS(base_fan)),
            .tach = DEVICE_DT_GET(DT_ALIAS(base_tach)),
        },
};

int fan_pwm_set(fan_id_t id, uint16_t pwm) {
    int ret = 0;

    LOG_DBG("Fan id: %d, pwm: %d", id, pwm);

    if (id >= ARRAY_SIZE(dev_list)) {
        LOG_ERR("Invalid fan ID: %d", id);
        return -EINVAL;
    }

    if (dev_list[id].fan.dev == NULL) {
        LOG_WRN("Invalid fan device");
        return -EINVAL;
    }
    
    uint32_t pulse = ((uint64_t)dev_list[id].fan.period * pwm) >> 16;
    LOG_DBG("Fan id: %d, pwm: %d, pulse: %dns", id, pwm, pulse);

    ret = pwm_set_pulse_dt(&dev_list[id].fan, pulse);
    if (ret < 0) {
        LOG_ERR("Failed to set speed for fan%d: %d", id, ret);
    } else {
        LOG_INF("Set fan%d speed to %d%%", id, pulse);
    }

    return ret;
}

int fan_rpm_set(fan_id_t id, uint16_t rpm) {
    uint16_t pwm = rpm*30;

    return fan_pwm_set(id, pwm);
}

int fan_rpm_get(fan_id_t id, uint16_t *prpm) {
    int ret = 0;

    if (id >= ARRAY_SIZE(dev_list)) {
        LOG_ERR("Invalid fan ID: %d", id);
        return -EINVAL;
    }

    if (dev_list[id].tach == NULL) {
        LOG_WRN("Invalid fan device");
        return -EINVAL;
    }

    if (prpm == NULL) {
        LOG_WRN("Invalid rpm pointer");
        return -EINVAL;
    }

    const struct device *tach = dev_list[id].tach;
    struct sensor_value val = {0};

    ret = sensor_sample_fetch_chan(tach, SENSOR_CHAN_RPM);
    if (ret) {
        LOG_ERR("Failed to fetch RPM sample for fan%d: %d", id, ret);
        return ret;
    }

    ret = sensor_channel_get(tach, SENSOR_CHAN_RPM, &val);
    if (ret) {
        LOG_ERR("Failed to get RPM for fan%d: %d", id, ret);
        return ret;
    }

    *prpm = (uint16_t)val.val1;
    LOG_INF("Fan%d RPM is %d", id, *prpm);

    return ret;
}


#include <zephyr/init.h>

static int init_config(void) {
    int ret = 0;

    LOG_INF("Initializing fan...");

    for (size_t i = 0; i < ARRAY_SIZE(dev_list); i++) {

        if (!pwm_is_ready_dt(&dev_list[i].fan)) {
            LOG_ERR("fan%d not ready", i);
            ret = -ENODEV;
        } else {
            ret = pwm_set_pulse_dt(&dev_list[i].fan, 0);
            if (ret < 0) {
                LOG_ERR("Failed to set PWM for fan%d: %d", i, ret);
            }
        }

        if (!device_is_ready(dev_list[i].tach)) {
            LOG_ERR("tach%d not ready", i);
            ret = -ENODEV;
        }
    }

    return ret;
}

SYS_INIT(init_config, APPLICATION, 0);

#ifdef CONFIG_SHELL
#include <zephyr/shell/shell.h>

static int cmd_fan_pwm_set(const struct shell *sh, size_t argc, char **argv) {
    fan_id_t id = 0;
    uint16_t pwm = 0;

    id = atoi(argv[1]);
    pwm = atoi(argv[2]);

    int ret = fan_pwm_set(id, pwm);
    if (ret < 0) {
        shell_error(sh, "Failed to set fan%d pwm: %d", id, ret);
    } else {
        shell_info(sh, "Fan%d pwm set to %d", id, pwm);
    }

    return 0;
}

static int cmd_fan_rpm_set(const struct shell *sh, size_t argc, char **argv) {
    fan_id_t id = 0;
    uint16_t rpm = 0;

    id = atoi(argv[1]);
    rpm = atoi(argv[2]);

    int ret = fan_rpm_set(id, rpm);
    if (ret < 0) {
        shell_error(sh, "Failed to set fan%d rpm: %d", id, ret);
    } else {
        shell_info(sh, "Fan%d rpm set to %d", id, rpm);
    }

    return 0;
}

static int cmd_fan_rpm_get(const struct shell *sh, size_t argc, char **argv) {
    fan_id_t id = 0;
    uint16_t rpm = 0;

    id = atoi(argv[1]);

    int ret = fan_rpm_get(id, &rpm);
    if (ret < 0) {
        shell_error(sh, "Failed to get fan%d RPM: %d", id, ret);
    } else {
        shell_info(sh, "Fan%d RPM is %d", id, rpm);
    }

    return ret;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_fan,
	SHELL_CMD_ARG(set_pwm, NULL,
		"Set fan pwm", cmd_fan_pwm_set, 3, 0),
    SHELL_CMD_ARG(set_rpm, NULL,
		"Set fan rpm", cmd_fan_rpm_set, 3, 0),
	SHELL_CMD_ARG(get_rpm, NULL,
		"Get fan RPM", cmd_fan_rpm_get, 2, 0),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(fan_dev, &sub_fan, "Fan commands", NULL);
#endif
