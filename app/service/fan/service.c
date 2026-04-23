/*
 * @Author: andy.chang 
 * @Date: 2025-07-01 02:46:45 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-23 00:11:26
 */

#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <interface/system.h>
#include <interface/power.h>
#include <interface/fan.h>

LOG_MODULE_REGISTER(fan, CONFIG_FAN_LOG_LEVEL);

enum {
    FAN_EVT_TEMP_CHG = LOCAL_EVT_START,
    FAN_EVT_CFG_UPDATE,
    FAN_EVT_RPM_UPDATE,
};

#define FAN_TEMP_CHG BIT(FAN_EVT_TEMP_CHG)
#define FAN_CFG_UPDATE BIT(FAN_EVT_CFG_UPDATE)
#define FAN_RPM_UPDATE BIT(FAN_EVT_RPM_UPDATE)

static K_EVENT_DEFINE(event);
SYS_EVENT_SUBSCRIBE(fan, event);

static uint16_t temps[THERM_SRC_MAX] = {0};
static fan_ctrl_t fan_blks[FAN_ID_MAX] = {0};

int fan_tmp_get(fan_id_t tmp_src, uint16_t *tmp) {

    if (tmp_src == 0 || tmp_src >= ARRAY_SIZE(temps) || tmp == NULL) {
        return -EINVAL;
    }

    *tmp = temps[tmp_src];

    return 0;
}

int fan_tmp_set(fan_id_t tmp_src, uint16_t tmp) {

    if (tmp_src == 0 || tmp_src >= ARRAY_SIZE(temps)) {
        return -EINVAL;
    }

    temps[tmp_src] = tmp;
    k_event_post(&event, FAN_TEMP_CHG);

    return 0;
}

int fan_rpm_write(fan_id_t fan_id, uint16_t rpm) {

    if (fan_id == 0 || fan_id >= ARRAY_SIZE(fan_blks)) {
        return -EINVAL;
    }

    fan_blks[fan_id].rpm = rpm;

    return 0;
}

int fan_ctrl_get(fan_id_t fan_id, fan_ctrl_t *ctrl) {
    if (fan_id == 0 || fan_id >= ARRAY_SIZE(fan_blks) || ctrl == NULL) {
        return -EINVAL;
    }

    memcpy(ctrl, &fan_blks[fan_id], sizeof(fan_ctrl_t));

    return 0;
}

int fan_ctrl_set(fan_id_t fan_id, const fan_ctrl_t *ctrl) {
    if (fan_id == 0 || fan_id >= ARRAY_SIZE(fan_blks) || ctrl == NULL) {
        return -EINVAL;
    }

    memcpy(&fan_blks[fan_id], ctrl, sizeof(fan_ctrl_t));

    k_event_post(&event, FAN_CFG_UPDATE);

    return 0;
}

static k_timeout_t rpm_update = K_MSEC(1000);

static int fan_rpm_update(fan_ctrl_t *fan_blk, uint16_t rpm) {

    if (rpm == 0) {
        // Fan off by PWM
        fan_blk->rpm = 0;
        rpm_update = K_FOREVER;
        fan_pwm_set(fan_blk->id, 0);
    } else {
        // Mapping PWM to RPM
        fan_rpm_set(fan_blk->id, rpm);

        // Get RPM from driver
        fan_rpm_get(fan_blk->id, &rpm);
        fan_blk->rpm = rpm;

        // TODO: RPM PID via PWM
    }

    return 0;
}

static inline bool check_fan_debug(fan_ctrl_t *fan_blk) {
    if (fan_blk == NULL)
        return 0;

    if (fan_blk->dbg_mode & BIT(0)) {
        if (fan_blk->dbg_mode & BIT(1)) {
            if (fan_blk->dbg_mode & BIT(2)) {
                // Set fan via pwm
                fan_pwm_set(fan_blk->id, fan_blk->dbg_pwm);
            } else {
                // Set fan via RPM
                fan_rpm_update(fan_blk, fan_blk->dbg_rpm);
            }
        } else {
            // Fan off
            fan_rpm_update(fan_blk, 0);
        }
        return true;
    }

    return false;
}

static inline int check_fan_rpm(fan_ctrl_t *fan_blk, uint16_t *rpm) {
    *rpm = 0;

    for (fan_id_t src = THERM_SRC_CPU; src < THERM_SRC_MAX; src++) {
        fan_tbl_t *tbl = fan_blk->fan_tbl[src];
        int8_t size = fan_blk->tbl_size[src] - 1;
        /* temps stores in 0.1C, LUT uses 1C. Convert to 1C for comparison */
        uint8_t temp = (uint8_t)(temps[src] / 10);

        for (; size >= 0; size--) {
            if (temp > tbl[size].temp_low && temp <= tbl[size].temp_high) {
                *rpm = (tbl[size].rpm > *rpm) ? tbl[size].rpm : *rpm;
            }
        }
    }

    *rpm *= 100; // Convert to actual RPM

    return 0;
}

static void service(void) {
    int ret = 0;
    uint32_t evt = 0;
    fan_ctrl_t *fan_blk = NULL;

    fan_id_t profile = FAN_PROFILE_BEST_PERFORMANCE_CHG_IN;
    for (fan_id_t fan = FAN_ID_1; fan < ARRAY_SIZE(fan_blks); fan++) {
        fan_blk = &fan_blks[fan];
        fan_blk->id = fan;

        for (fan_id_t src = THERM_SRC_CPU; src < THERM_SRC_MAX; src++) {
            fan_tbl_t **tbl = &fan_blk->fan_tbl[src];
            uint8_t *tbl_size = &fan_blk->tbl_size[src];
            fan_tbl_get(profile, fan, src, tbl, tbl_size);
            fan_blk->profile = profile;
            fan_blk->state = FAN_STA_ON;
        }
    }

    while (1) {

        // TODO: Wait for event (temp change, update rpm, pwr, etc.)
        evt = k_event_wait(
            &event,
            (SYS_EVT_MASK | FAN_TEMP_CHG | FAN_CFG_UPDATE | FAN_RPM_UPDATE),
            true, rpm_update);

        pwr_sta_t state;
        pwr_state_get(&state);

        for (fan_id_t fan = FAN_ID_1; fan < ARRAY_SIZE(fan_blks); fan++) {
            fan_blk = &fan_blks[fan];

            // Check debug mode
            if (check_fan_debug(fan_blk)) {
                continue;
            }

            // Check power state
            if (state != PWR_STA_S0) {
                fan_rpm_update(fan_blk, 0);
                continue;
            }

            // Check fan on
            if (fan_blk->state != FAN_STA_ON) {
                fan_rpm_update(fan_blk, 0);
                continue;
            }

            // Get RPM from table
            uint16_t rpm = 0;
            ret = check_fan_rpm(fan_blk, &rpm);
            if (ret < 0) {
                LOG_WRN("Fan%d RPM can't found in LUT", fan);
            } else {
                LOG_DBG("Fan: %d, rpm: %d", fan, rpm);
                fan_rpm_update(fan_blk, rpm);
                continue;
            }

            // TODO: RPM control for Fan

            // TODO: check RPM with trip point
        }
    }
}

K_THREAD_DEFINE(fan_id, APP_STACK_MIN, service, NULL, NULL, NULL, APP_PRIO_M, 0,
                0);

#ifdef CONFIG_FAN_SHELL
#include <zephyr/shell/shell.h>

static void dump_fan_info(const struct shell *sh, fan_ctrl_t *fan_blk) {
    shell_info(sh, "Fan ID: %d", fan_blk->id);
    shell_info(sh, "Fan state: %d", fan_blk->state);
    shell_info(sh, "Fan rpm: %d", fan_blk->rpm);
    shell_info(sh, "Fan trip_low: %d rpm", fan_blk->trip_low);
    shell_info(sh, "Fan trip_high: %d rpm", fan_blk->trip_high);
    shell_info(sh, "Fan profile: %d", fan_blk->profile);

    for (size_t i = THERM_SRC_CPU; i < THERM_SRC_MAX; i++) {
        uint8_t size = fan_blk->tbl_size[i];
        fan_tbl_t *tbl = fan_blk->fan_tbl[i];
        shell_info(sh, "Fan tbl_size[%d]: %d", (int)i, size);
        for (size_t j = 0; j < size; j++) {
            shell_info(sh, "\ttbl[%d]: rpm: %d, high: %d, low: %d", (int)j,
                       tbl[j].rpm * 100, tbl[j].temp_high, tbl[j].temp_low);
        }
    }

    shell_info(sh, "Fan dbg_mode: 0x%02x", fan_blk->dbg_mode);
    shell_info(sh, "Fan dbg_rpm: %d", fan_blk->dbg_rpm);
    shell_info(sh, "Fan dbg_pwm: %d", fan_blk->dbg_pwm);
}

static int cmd_fan_get(const struct shell *sh, size_t argc, char **argv) {
    fan_id_t id = (fan_id_t)strtoul(argv[1], NULL, 0);
    if (id == 0 || id >= ARRAY_SIZE(fan_blks)) {
        shell_error(sh, "Invalid fan ID");
        return -EINVAL;
    }
    dump_fan_info(sh, &fan_blks[id]);
    return 0;
}

static int cmd_fan_state(const struct shell *sh, size_t argc, char **argv) {
    fan_id_t id = (fan_id_t)strtoul(argv[1], NULL, 0);
    if (id == 0 || id >= ARRAY_SIZE(fan_blks)) {
        shell_error(sh, "Invalid fan ID");
        return -EINVAL;
    }
    uint8_t state = (uint8_t)strtoul(argv[2], NULL, 0);
    fan_blks[id].state = state;
    k_event_post(&event, FAN_CFG_UPDATE);
    shell_info(sh, "Fan %d state set to %d", id, state);
    return 0;
}

static int cmd_fan_trip(const struct shell *sh, size_t argc, char **argv) {
    fan_id_t id = (fan_id_t)strtoul(argv[1], NULL, 0);
    if (id == 0 || id >= ARRAY_SIZE(fan_blks)) {
        shell_error(sh, "Invalid fan ID");
        return -EINVAL;
    }
    fan_blks[id].trip_low = (uint16_t)strtoul(argv[2], NULL, 0);
    fan_blks[id].trip_high = (uint16_t)strtoul(argv[3], NULL, 0);
    k_event_post(&event, FAN_CFG_UPDATE);
    shell_info(sh, "Fan %d trip points: low=%d, high=%d", id, fan_blks[id].trip_low, fan_blks[id].trip_high);
    return 0;
}

static int cmd_fan_profile(const struct shell *sh, size_t argc, char **argv) {
    fan_id_t id = (fan_id_t)strtoul(argv[1], NULL, 0);
    if (id == 0 || id >= ARRAY_SIZE(fan_blks)) {
        shell_error(sh, "Invalid fan ID");
        return -EINVAL;
    }
    fan_id_t profile = (fan_id_t)strtoul(argv[2], NULL, 0);
    if (profile == 0 || profile >= FAN_PROFILE_MAX) {
        shell_error(sh, "Invalid profile ID");
        return -EINVAL;
    }

    fan_ctrl_t *fan_blk = &fan_blks[id];
    fan_blk->profile = (uint8_t)profile;
    for (fan_id_t src = THERM_SRC_CPU; src < THERM_SRC_MAX; src++) {
        fan_tbl_get(profile, id, src, &fan_blk->fan_tbl[src], &fan_blk->tbl_size[src]);
    }
    k_event_post(&event, FAN_CFG_UPDATE);
    shell_info(sh, "Fan %d profile updated to %d", id, profile);
    return 0;
}

static int cmd_fan_debug(const struct shell *sh, size_t argc, char **argv) {
    fan_id_t id = (fan_id_t)strtoul(argv[1], NULL, 0);
    if (id == 0 || id >= ARRAY_SIZE(fan_blks)) {
        shell_error(sh, "Invalid fan ID");
        return -EINVAL;
    }
    fan_blks[id].dbg_mode = (uint8_t)strtoul(argv[2], NULL, 0);
    if (argc >= 4) {
        uint32_t val = strtoul(argv[3], NULL, 0);
        if (fan_blks[id].dbg_mode & BIT(2)) {
            fan_blks[id].dbg_pwm = (uint8_t)val;
        } else {
            fan_blks[id].dbg_rpm = (uint16_t)val;
        }
    }
    k_event_post(&event, FAN_CFG_UPDATE);
    shell_info(sh, "Fan %d debug mode: 0x%02x updated", id, fan_blks[id].dbg_mode);
    return 0;
}

static int cmd_temp_set(const struct shell *sh, size_t argc, char **argv) {
    int ret = 0;

    fan_id_t id = strtoul(argv[1], NULL, 0);
    uint16_t tmp = strtoul(argv[2], NULL, 0);

    /* Shell input is in 0.1C (e.g. 350 = 35.0C) */
    shell_info(sh, "Fan ID: %d, tmp set to: %d.%d C", id, tmp / 10, abs(tmp % 10));
    fan_tmp_set(id, tmp);

    return ret;
}

static int cmd_dump(const struct shell *sh, size_t argc, char **argv) {
    int ret = 0;

    for (fan_id_t fan = FAN_ID_1; fan < ARRAY_SIZE(fan_blks); fan++) {
        dump_fan_info(sh, &fan_blks[fan]);
        shell_info(sh, "-------------------");
    }
    
    return ret;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_fan,
	SHELL_CMD_ARG(temp, NULL,
		"Set temp <id> <temp>", cmd_temp_set, 3, 0),
	SHELL_CMD_ARG(dump, NULL,
		"Dump fan info", cmd_dump, 0, 0),
	SHELL_CMD_ARG(get, NULL,
		"Get fan info <id>", cmd_fan_get, 2, 0),
	SHELL_CMD_ARG(state, NULL,
		"Set fan state <id> <state>", cmd_fan_state, 3, 0),
	SHELL_CMD_ARG(trip, NULL,
		"Set fan trip points <id> <low> <high>", cmd_fan_trip, 4, 0),
	SHELL_CMD_ARG(profile, NULL,
		"Set fan profile <id> <profile_id>", cmd_fan_profile, 3, 0),
	SHELL_CMD_ARG(debug, NULL,
		"Set debug settings <id> <mode_hex> [val]\n"
        "Usage:\n"
        "\t<mode_hex>\n"
        "\t\tBit 0 : Debug Mode ON/OFF (OFF: 0, ON: 1)\n"
        "\t\tBit 1 : Fan ON/OFF (OFF: 0, ON: 1)\n"
        "\t\tBit 2 : Debug Type (RPM: 0, PWM: 1)",
        cmd_fan_debug, 3, 1),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(fan, &sub_fan, "Fan commands", NULL);
#endif
