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
#include <string.h>

#include <interface/fan.h>
#include <interface/thermal.h>
#include <interface/hidi2c.h>
#include <interface/keyboard.h>

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

int therm_sample_get(therm_id_t id, int16_t *temp) {
    if (id >= THERM_DEV_MAX || temp == NULL) return -EINVAL;
    *temp = sim_therm_temp[id];
    return 0;
}

/* --- HIDI2C Simulator APIs ---
 *
 * QEMU 下沒有真 I2C target / Host，post_report 只把內容印出來，方便驗證
 * service 層 6KRO 是否正確。也保留最近一筆 report 供 shell 查詢。
 */

static struct keyboard_report sim_last_report = {
    .length = sizeof(struct keyboard_report),
};
static uint32_t sim_post_count;

int hidi2c_post_report(const struct keyboard_report *report) {
    if (report == NULL) return -EINVAL;

    memcpy(&sim_last_report, report, sizeof(sim_last_report));
    sim_post_count++;

    LOG_INF("[SIM] hidi2c_post_report #%u: mod=0x%02x keys=%02x %02x %02x %02x %02x %02x",
            sim_post_count,
            report->modifier,
            report->keys[0], report->keys[1], report->keys[2],
            report->keys[3], report->keys[4], report->keys[5]);
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

/* --- Keyboard / HIDI2C Simulator Shell ---
 *
 * 真 keyboard driver 透過 kscan callback 把 (col,row) 轉成 INPUT_KEY_* 後呼叫
 * keyboard_service_process_event；QEMU 下用 shell 直接注入 input_code 模擬該路徑。
 * reset 對應真 hidi2c driver 收到 Host RESET command 時的行為。
 */

static int cmd_sim_kbd_press(const struct shell *sh, size_t argc, char **argv) {
    uint16_t input_code = (uint16_t)strtoul(argv[1], NULL, 0);

    keyboard_service_process_event(input_code, true);
    shell_info(sh, "[SIM] kscan press input_code=0x%04x", input_code);
    return 0;
}

static int cmd_sim_kbd_release(const struct shell *sh, size_t argc, char **argv) {
    uint16_t input_code = (uint16_t)strtoul(argv[1], NULL, 0);

    keyboard_service_process_event(input_code, false);
    shell_info(sh, "[SIM] kscan release input_code=0x%04x", input_code);
    return 0;
}

static int cmd_sim_kbd_tap(const struct shell *sh, size_t argc, char **argv) {
    uint16_t input_code = (uint16_t)strtoul(argv[1], NULL, 0);

    keyboard_service_process_event(input_code, true);
    keyboard_service_process_event(input_code, false);
    shell_info(sh, "[SIM] kscan tap input_code=0x%04x", input_code);
    return 0;
}

static int cmd_sim_hidi2c_reset(const struct shell *sh, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    keyboard_service_reset();
    memset(&sim_last_report, 0, sizeof(sim_last_report));
    sim_last_report.length = sizeof(struct keyboard_report);
    sim_post_count = 0;
    shell_info(sh, "[SIM] hidi2c host RESET");
    return 0;
}

static int cmd_sim_hidi2c_show(const struct shell *sh, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    shell_print(sh, "--- Last HIDI2C report (#%u posts) ---", sim_post_count);
    shell_print(sh, "  length   : %u",   sim_last_report.length);
    shell_print(sh, "  modifier : 0x%02x", sim_last_report.modifier);
    shell_print(sh, "  keys     : %02x %02x %02x %02x %02x %02x",
                sim_last_report.keys[0], sim_last_report.keys[1],
                sim_last_report.keys[2], sim_last_report.keys[3],
                sim_last_report.keys[4], sim_last_report.keys[5]);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_sim_kbd,
    SHELL_CMD_ARG(press,   NULL, "Inject kscan press   <input_code>", cmd_sim_kbd_press,   2, 0),
    SHELL_CMD_ARG(release, NULL, "Inject kscan release <input_code>", cmd_sim_kbd_release, 2, 0),
    SHELL_CMD_ARG(tap,     NULL, "Inject kscan tap     <input_code>", cmd_sim_kbd_tap,     2, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_sim_hidi2c,
    SHELL_CMD_ARG(reset, NULL, "Simulate Host RESET command",  cmd_sim_hidi2c_reset, 1, 0),
    SHELL_CMD_ARG(show,  NULL, "Show last posted report",      cmd_sim_hidi2c_show,  1, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_sim,
    SHELL_CMD_ARG(fan, NULL, "Set simulated fan RPM <id> <val>", cmd_sim_fan_set, 3, 0),
    SHELL_CMD_ARG(temp, NULL, "Set simulated therm temp <id> <val>", cmd_sim_temp_set, 3, 0),
    SHELL_CMD_ARG(status, NULL, "Show all simulated values", cmd_sim_status, 1, 0),
    SHELL_CMD(kbd,    &sub_sim_kbd,    "Keyboard kscan injection",   NULL),
    SHELL_CMD(hidi2c, &sub_sim_hidi2c, "HIDI2C host simulation",     NULL),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sim, &sub_sim, "Simulator control commands", NULL);
