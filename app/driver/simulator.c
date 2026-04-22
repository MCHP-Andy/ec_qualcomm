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

#include <interface/fan.h>
#include <interface/thermal.h>

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

/* --- Thermal Simulator APIs --- */

int therm_sample_get(therm_id_t id, uint16_t *temp) {
    if (id >= THERM_DEV_MAX || temp == NULL) return -EINVAL;
    *temp = sim_therm_temp[id];
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
    uint16_t temp = (uint16_t)strtoul(argv[2], NULL, 0);

    if (id >= THERM_DEV_MAX) {
        shell_error(sh, "Invalid Thermistor ID (0-%d)", THERM_DEV_MAX - 1);
        return -EINVAL;
    }

    sim_therm_temp[id] = temp;
    shell_info(sh, "Simulator: Thermistor %d Temp fixed to %d", id, temp);
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
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_sim,
    SHELL_CMD_ARG(fan, NULL, "Set simulated fan RPM <id> <val>", cmd_sim_fan_set, 3, 0),
    SHELL_CMD_ARG(temp, NULL, "Set simulated therm temp <id> <val>", cmd_sim_temp_set, 3, 0),
    SHELL_CMD_ARG(status, NULL, "Show all simulated values", cmd_sim_status, 1, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sim, &sub_sim, "Simulator control commands", NULL);
