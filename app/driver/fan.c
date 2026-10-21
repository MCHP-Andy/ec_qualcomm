/*
 * @Author: andy.chang 
 * @Date: 2026-04-18 11:29:06 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-23 00:36:37
 */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include <interface/fan.h>

LOG_MODULE_REGISTER(fan_dev, LOG_LEVEL_INF);

static const struct gpio_dt_spec fan_en =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), fan_12v_enable_gpios);

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

// clang-format off
static struct fan_profile_t {
    fan_lut_t lut[FAN_ID_MAX][THERM_SRC_MAX];
} fan_profiles[FAN_PROFILE_MAX] = {
    [FAN_PROFILE_BETTER_PERFORMANCE_CHG_IN] = {
        .lut = {
            [FAN_ID_1][THERM_SRC_CPU] = {
                .temp_src = THERM_SRC_CPU,
                .tbl_size = 10,
                .tbl = {
                    {.rpm = 0x19, .temp_high = 0x4A, .temp_low = 0x00},
                    {.rpm = 0x23, .temp_high = 0x4D, .temp_low = 0x47},
                    {.rpm = 0x2D, .temp_high = 0x50, .temp_low = 0x4A},
                    {.rpm = 0x37, .temp_high = 0x53, .temp_low = 0x4D},
                    {.rpm = 0x41, .temp_high = 0x55, .temp_low = 0x50},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0x53},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                },
            },
            [FAN_ID_1][THERM_SRC_SKIN] = {
                .temp_src = THERM_SRC_SKIN,
                .tbl_size = 10,
                .tbl = {
                    {.rpm = 0x19, .temp_high = 0x4A, .temp_low = 0x00},
                    {.rpm = 0x23, .temp_high = 0x4D, .temp_low = 0x47},
                    {.rpm = 0x2D, .temp_high = 0x50, .temp_low = 0x4A},
                    {.rpm = 0x37, .temp_high = 0x53, .temp_low = 0x4D},
                    {.rpm = 0x41, .temp_high = 0x55, .temp_low = 0x50},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0x53},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                },
            },
            [FAN_ID_2][THERM_SRC_SKIN] = {
                .temp_src = THERM_SRC_SKIN,
                .tbl_size = 10,
                .tbl = {
                    {.rpm = 0x2D, .temp_high = 0x36, .temp_low = 0x00},
                    {.rpm = 0x32, .temp_high = 0x39, .temp_low = 0x34},
                    {.rpm = 0x37, .temp_high = 0x3B, .temp_low = 0x36},
                    {.rpm = 0x3C, .temp_high = 0x3D, .temp_low = 0x38},
                    {.rpm = 0x46, .temp_high = 0x3F, .temp_low = 0x3A},
                    {.rpm = 0x50, .temp_high = 0x41, .temp_low = 0x3C},
                    {.rpm = 0x5A, .temp_high = 0x43, .temp_low = 0x3E},
                    {.rpm = 0x64, .temp_high = 0xFF, .temp_low = 0x40},
                    {.rpm = 0x64, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x64, .temp_high = 0xFF, .temp_low = 0xFF},
                },
            },
        },
    },
    [FAN_PROFILE_BETTER_PERFORMANCE_CHG_OUT] = {
        .lut = {
            [FAN_ID_1][THERM_SRC_CPU] = {
                .temp_src = THERM_SRC_CPU,
                .tbl_size = 10,
                .tbl = {
                    {.rpm = 0x19, .temp_high = 0x4A, .temp_low = 0x00},
                    {.rpm = 0x23, .temp_high = 0x4D, .temp_low = 0x47},
                    {.rpm = 0x2D, .temp_high = 0x50, .temp_low = 0x4A},
                    {.rpm = 0x37, .temp_high = 0x53, .temp_low = 0x4D},
                    {.rpm = 0x41, .temp_high = 0x55, .temp_low = 0x50},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0x53},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                },
            },
            [FAN_ID_1][THERM_SRC_SKIN] = {
                .temp_src = THERM_SRC_SKIN,
                .tbl_size = 10,
                .tbl = {
                    {.rpm = 0x19, .temp_high = 0x4A, .temp_low = 0x00},
                    {.rpm = 0x23, .temp_high = 0x4D, .temp_low = 0x47},
                    {.rpm = 0x2D, .temp_high = 0x50, .temp_low = 0x4A},
                    {.rpm = 0x37, .temp_high = 0x53, .temp_low = 0x4D},
                    {.rpm = 0x41, .temp_high = 0x55, .temp_low = 0x50},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0x53},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                },
            },
            [FAN_ID_2][THERM_SRC_SKIN] = {
                .temp_src = THERM_SRC_SKIN,
                .tbl_size = 10,
                .tbl = {
                    {.rpm = 0x2D, .temp_high = 0x36, .temp_low = 0x00},
                    {.rpm = 0x32, .temp_high = 0x39, .temp_low = 0x34},
                    {.rpm = 0x37, .temp_high = 0x3B, .temp_low = 0x36},
                    {.rpm = 0x3C, .temp_high = 0x3D, .temp_low = 0x38},
                    {.rpm = 0x46, .temp_high = 0x3F, .temp_low = 0x3A},
                    {.rpm = 0x50, .temp_high = 0x41, .temp_low = 0x3C},
                    {.rpm = 0x5A, .temp_high = 0x43, .temp_low = 0x3E},
                    {.rpm = 0x64, .temp_high = 0xFF, .temp_low = 0x40},
                    {.rpm = 0x64, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x64, .temp_high = 0xFF, .temp_low = 0xFF},
                },
            },
        },
    },
    [FAN_PROFILE_BEST_PERFORMANCE_CHG_IN] = {
        .lut = {
            [FAN_ID_1][THERM_SRC_CPU] = {
                .temp_src = THERM_SRC_CPU,
                .tbl_size = 10,
                .tbl = {
                    {.rpm = 0x19, .temp_high = 0x4A, .temp_low = 0x00},
                    {.rpm = 0x23, .temp_high = 0x4D, .temp_low = 0x47},
                    {.rpm = 0x2D, .temp_high = 0x50, .temp_low = 0x4A},
                    {.rpm = 0x37, .temp_high = 0x53, .temp_low = 0x4D},
                    {.rpm = 0x41, .temp_high = 0x55, .temp_low = 0x50},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0x53},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                },
            },
            [FAN_ID_1][THERM_SRC_SKIN] = {
                .temp_src = THERM_SRC_SKIN,
                .tbl_size = 10,
                .tbl = {
                    {.rpm = 0x19, .temp_high = 0x4A, .temp_low = 0x00},
                    {.rpm = 0x23, .temp_high = 0x4D, .temp_low = 0x47},
                    {.rpm = 0x2D, .temp_high = 0x50, .temp_low = 0x4A},
                    {.rpm = 0x37, .temp_high = 0x53, .temp_low = 0x4D},
                    {.rpm = 0x41, .temp_high = 0x55, .temp_low = 0x50},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0x53},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                },
            },
            [FAN_ID_2][THERM_SRC_SKIN] = {
                .temp_src = THERM_SRC_SKIN,
                .tbl_size = 10,
                .tbl = {
                    {.rpm = 0x2D, .temp_high = 0x36, .temp_low = 0x00},
                    {.rpm = 0x32, .temp_high = 0x39, .temp_low = 0x34},
                    {.rpm = 0x37, .temp_high = 0x3B, .temp_low = 0x36},
                    {.rpm = 0x3C, .temp_high = 0x3D, .temp_low = 0x38},
                    {.rpm = 0x46, .temp_high = 0x3F, .temp_low = 0x3A},
                    {.rpm = 0x50, .temp_high = 0x41, .temp_low = 0x3C},
                    {.rpm = 0x5A, .temp_high = 0x43, .temp_low = 0x3E},
                    {.rpm = 0x64, .temp_high = 0xFF, .temp_low = 0x40},
                    {.rpm = 0x64, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x64, .temp_high = 0xFF, .temp_low = 0xFF},
                },
            },
        },
    },
    [FAN_PROFILE_BEST_PERFORMANCE_CHG_OUT] = {
        .lut = {
            [FAN_ID_1][THERM_SRC_CPU] = {
                .temp_src = THERM_SRC_CPU,
                .tbl_size = 10,
                .tbl = {
                    {.rpm = 0x19, .temp_high = 0x4A, .temp_low = 0x00},
                    {.rpm = 0x23, .temp_high = 0x4D, .temp_low = 0x47},
                    {.rpm = 0x2D, .temp_high = 0x50, .temp_low = 0x4A},
                    {.rpm = 0x37, .temp_high = 0x53, .temp_low = 0x4D},
                    {.rpm = 0x41, .temp_high = 0x55, .temp_low = 0x50},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0x53},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                },
            },
            [FAN_ID_1][THERM_SRC_SKIN] = {
                .temp_src = THERM_SRC_SKIN,
                .tbl_size = 10,
                .tbl = {
                    {.rpm = 0x19, .temp_high = 0x4A, .temp_low = 0x00},
                    {.rpm = 0x23, .temp_high = 0x4D, .temp_low = 0x47},
                    {.rpm = 0x2D, .temp_high = 0x50, .temp_low = 0x4A},
                    {.rpm = 0x37, .temp_high = 0x53, .temp_low = 0x4D},
                    {.rpm = 0x41, .temp_high = 0x55, .temp_low = 0x50},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0x53},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
                },
            },
            [FAN_ID_2][THERM_SRC_SKIN] = {
                .temp_src = THERM_SRC_SKIN,
                .tbl_size = 10,
                .tbl = {
                    {.rpm = 0x2D, .temp_high = 0x36, .temp_low = 0x00},
                    {.rpm = 0x32, .temp_high = 0x39, .temp_low = 0x34},
                    {.rpm = 0x37, .temp_high = 0x3B, .temp_low = 0x36},
                    {.rpm = 0x3C, .temp_high = 0x3D, .temp_low = 0x38},
                    {.rpm = 0x46, .temp_high = 0x3F, .temp_low = 0x3A},
                    {.rpm = 0x50, .temp_high = 0x41, .temp_low = 0x3C},
                    {.rpm = 0x5A, .temp_high = 0x43, .temp_low = 0x3E},
                    {.rpm = 0x64, .temp_high = 0xFF, .temp_low = 0x40},
                    {.rpm = 0x64, .temp_high = 0xFF, .temp_low = 0xFF},
                    {.rpm = 0x64, .temp_high = 0xFF, .temp_low = 0xFF},
                },
            },
        },
    },
};
// clang-format on

/*
 * SoC-CP fan power constraint (Ref: EC FAN Constraints Message, page 55).
 * Spec default is 0 (OFF), the fan(s) stay OFF until SoC-CP grants permission
 * to turn them ON, so a brown out cannot be triggered before SoC-CP is up.
 */
static bool fan_allow_on = false;

int fan_constraint_set(bool allow_on) {

    fan_allow_on = allow_on;
    gpio_pin_set_dt(&fan_en, fan_allow_on);
    LOG_INF("Fan constraint: FAN(s) can %s", allow_on ? "turn ON" : "only stay OFF");

    return 0;
}

int fan_constraint_get(bool *allow_on) {

    if (allow_on == NULL) {
        return -EINVAL;
    }

    *allow_on = fan_allow_on;

    return 0;
}

int fan_lut_get(fan_id_t profile, fan_id_t fan_id, fan_id_t temp_src,
                fan_lut_t **lut) {
    if ((profile < FAN_PROFILE_BATTERY_SAVER ||
         profile > FAN_PROFILE_BEST_PERFORMANCE_CHG_OUT) ||
        (fan_id < FAN_ID_1 || fan_id > FAN_ID_2) ||
        (temp_src < THERM_SRC_CPU || temp_src >= THERM_SRC_MAX) ||
        lut == NULL) {
        LOG_ERR("Invalid parameters: profile=%d, fan_id=%d, lut=%p", profile,
                fan_id, lut);
        return -EINVAL;
    }

    *lut = &fan_profiles[profile].lut[fan_id][temp_src];

    return 0;
}

// pwm: 0~65535 (0~100% duty cycle)
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
    
    // dev_list[id].fan.period = 40000ns
    uint32_t pulse = ((uint64_t)dev_list[id].fan.period * pwm) >> 16;
    LOG_DBG("Fan id: %d, pwm: %d, pulse: %dns", id, pwm, pulse);

    // Invert the pulse width
    ret = pwm_set_pulse_dt(&dev_list[id].fan, pulse);
    if (ret < 0) {
        LOG_ERR("Failed to set speed for fan%d: %d", id, ret);
    } else {
        LOG_DBG("Set fan%d speed to %d%%", id, pulse);
    }

    return ret;
}

int fan_rpm_set(fan_id_t id, uint16_t rpm) {
    uint16_t pwm = 0;

    switch (id) {
        case FAN_ID_1:
            if (rpm < 3000) {
                pwm = 0;
            } else if (rpm > 10327) {
                pwm = 65535;
            } else {
                pwm = (uint16_t)((rpm - 3000.0f) * 65535.0f / 7327.0f);
            }
            break;
        case FAN_ID_2:
            if (rpm < 2000) {
                pwm = 0;
            } else if (rpm > 9500) {
                pwm = 65535;
            } else {
                pwm = (uint16_t)(rpm - 1931) * 65535.0f / 7488.0f;
            }
            break;
        default:
            LOG_ERR("Invalid fan ID: %d", id);
            return -EINVAL;
            break;
    }

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
    LOG_DBG("Fan%d RPM is %d", id, *prpm);

    return ret;
}


#include <zephyr/init.h>

static int init_config(void) {
    int ret = 0;

    LOG_INF("Initializing fan...");

    if (!gpio_is_ready_dt(&fan_en)) {
        LOG_ERR("Fan enable pin not ready");
        return -ENODEV;
    }

    if (gpio_pin_configure_dt(&fan_en, GPIO_OUTPUT_INACTIVE) < 0) {
        LOG_ERR("Failed to configure FAN enable pin");
        return -1;
    }
    fan_constraint_set(fan_allow_on);

    for (size_t i = 0; i < ARRAY_SIZE(dev_list); i++) {

        if (!pwm_is_ready_dt(&dev_list[i].fan)) {
            LOG_ERR("fan%d not ready", i);
            ret = -ENODEV;
        } else {
            ret = fan_pwm_set(i, 0);
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
