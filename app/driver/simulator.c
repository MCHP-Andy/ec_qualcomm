/*
 * @Author: andy.chang 
 * @Date: 2026-04-23 00:26:28 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-23 00:28:53
 */

/*
 * @Description: Simulator driver for Fan and Thermal sensors
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <stdlib.h>

#include <interface/acpi.h>
#include <interface/fan.h>
#include <interface/thermal.h>
#include <interface/aoss.h>

LOG_MODULE_REGISTER(sim_dev, LOG_LEVEL_DBG);

/* 模擬變數儲存空間 */
static uint16_t sim_fan_rpm[FAN_ID_MAX] = {
    [FAN_ID_1] = 3000,
    [FAN_ID_2] = 3000
};

static uint16_t sim_therm_temp[THERM_DEV_MAX] = {
    [THERM_DEV_1] = 350, // 35.0 C (假設單位為 0.1C)
    [THERM_DEV_2] = 400, // 40.0 C
    [THERM_DEV_3] = 450  // 45.0 C
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

/* EC-AOSS 模擬暫存器 */
static uint32_t sim_aoss_status = AOSS_STA_SOC_TEMP_TELEM | AOSS_STA_LMT_CAP;
static uint32_t sim_aoss_temp = 553; // 55.3 C (單位 0.1C, 2's complement)

/* --- Fan Simulator APIs --- */

int fan_pwm_set(fan_id_t id, uint16_t pwm) {
    if (id <= 0 || id >= FAN_ID_MAX) return -EINVAL;
    LOG_INF("[SIM] Fan %d PWM set to: %d", id, pwm);
    return 0;
}

int fan_rpm_set(fan_id_t id, uint16_t rpm) {
    if (id <= 0 || id >= FAN_ID_MAX) return -EINVAL;
    LOG_INF("[SIM] Fan %d Target RPM set to: %d", id, rpm);
    return 0;
}

int fan_rpm_get(fan_id_t id, uint16_t *prpm) {
    if (id <= 0 || id >= FAN_ID_MAX || prpm == NULL) return -EINVAL;
    *prpm = sim_fan_rpm[id];
    return 0;
}

static bool fan_allow_on = false;

int fan_constraint_set(bool allow_on) {
    fan_allow_on = allow_on;
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

    // TODO: Default to CPU source, can be changed later
    *lut = &fan_profiles[profile].lut[fan_id][temp_src];

    return 0;
}

/* --- Thermal Simulator APIs --- */

int therm_sample_get(therm_id_t id, int16_t *temp) {
    if (id >= THERM_DEV_MAX || temp == NULL) return -EINVAL;
    *temp = sim_therm_temp[id];
    return 0;
}

int acpi_resp_set(uint8_t *pdata, uint16_t len) {
    LOG_INF("[SIM] ACPI data len: %d", len);

    // Dump acpi data
    LOG_HEXDUMP_INF(pdata, len, "[SIM] ACPI data :");

    return 0;
}

int acpi_int_set(uint8_t val) {
    if (val > 1) {
        return -EINVAL;
    }

    LOG_INF("[SIM] ACPI int pin set to %u", val);

    return 0;
}

int soccp_resp_set(uint8_t *pdata, uint16_t len) {
    LOG_INF("[SIM] SOCCP data len: %d", len);

    // Dump soccp data
    LOG_HEXDUMP_INF(pdata, len, "[SIM] SOCCP data :");

    return 0;
}

/* --- EC-AOSS Simulator APIs --- */

int aoss_status_get(aoss_status_t *sta) {
    if (sta == NULL) return -EINVAL;
    aoss_status_parse(sim_aoss_status, sta);
    return 0;
}

int aoss_temp_get(int16_t *temp_c, int16_t *temp_dc) {
    int16_t dc = aoss_temp_to_dc(sim_aoss_temp);

    if (temp_dc) *temp_dc = dc;
    if (temp_c) *temp_c = aoss_dc_to_c(dc);
    return 0;
}

/* --- Shell Interface --- */

static int cmd_sim_fan_set(const struct shell *sh, size_t argc, char **argv) {
    fan_id_t id = (fan_id_t)strtoul(argv[1], NULL, 0);
    uint16_t rpm = (uint16_t)strtoul(argv[2], NULL, 0);

    if (id <= 0 || id >= FAN_ID_MAX) {
        shell_error(sh, "Invalid Fan ID (1-%d)", FAN_ID_MAX - 1);
        return -EINVAL;
    }

    sim_fan_rpm[id] = rpm;
    shell_info(sh, "Simulator: Fan %d RPM fixed to %d", id, rpm);
    return 0;
}

static int cmd_sim_temp_set(const struct shell *sh, size_t argc, char **argv) {
    therm_id_t id = (therm_id_t)strtoul(argv[1], NULL, 0);
    int16_t temp = (int16_t)strtoul(argv[2], NULL, 0);

    if (id >= THERM_DEV_MAX) {
        shell_error(sh, "Invalid Thermistor ID (0-%d)", THERM_DEV_MAX - 1);
        return -EINVAL;
    }

    sim_therm_temp[id] = temp;
    shell_info(sh, "Simulator: Thermistor %d Temp fixed to %d.%d", id, temp / 10, abs(temp % 10));
    return 0;
}

static int cmd_sim_aoss_sta_set(const struct shell *sh, size_t argc, char **argv) {
    sim_aoss_status = (uint32_t)strtoul(argv[1], NULL, 0) & AOSS_STA_MASK;
    shell_info(sh, "Simulator: AOSS status fixed to 0x%08x", sim_aoss_status);
    return 0;
}

static int cmd_sim_aoss_temp_set(const struct shell *sh, size_t argc, char **argv) {
    int16_t temp = (int16_t)strtol(argv[1], NULL, 0);

    sim_aoss_temp = (uint16_t)temp;
    shell_info(sh, "Simulator: AOSS temp fixed to %s%d.%d", temp < 0 ? "-" : "",
               abs(temp / 10), abs(temp % 10));
    return 0;
}

static int cmd_sim_status(const struct shell *sh, size_t argc, char **argv) {
    shell_print(sh, "--- Simulator Current Values ---");
    for (int i = 1; i < FAN_ID_MAX; i++) {
        shell_print(sh, "Fan %d RPM: %d", i, sim_fan_rpm[i]);
    }
    for (int i = 0; i < THERM_DEV_MAX; i++) {
        shell_print(sh, "Therm %d Temp: %d", i, sim_therm_temp[i]);
    }
    shell_print(sh, "AOSS status: 0x%08x", sim_aoss_status);
    shell_print(sh, "AOSS temp: %d", aoss_temp_to_dc(sim_aoss_temp));
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_sim,
    SHELL_CMD_ARG(fan, NULL, "Set simulated fan RPM <id> <val>", cmd_sim_fan_set, 3, 0),
    SHELL_CMD_ARG(temp, NULL, "Set simulated therm temp <id> <val>", cmd_sim_temp_set, 3, 0),
    SHELL_CMD_ARG(aoss_sta, NULL, "Set simulated AOSS status <val>", cmd_sim_aoss_sta_set, 2, 0),
    SHELL_CMD_ARG(aoss_temp, NULL, "Set simulated AOSS temp in 0.1C <val>", cmd_sim_aoss_temp_set, 2, 0),
    SHELL_CMD_ARG(status, NULL, "Show all simulated values", cmd_sim_status, 1, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sim, &sub_sim, "Simulator control commands", NULL);
