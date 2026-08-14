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
#include <interface/acpi.h>

#include <service/acpi/acpi_tbl.h> // ACPI_CHECK_IN/OUT macros for fan handlers

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

/*
 * SoC-CP fan power constraint (Ref: EC FAN Constraints Message, page 55).
 * Spec default is 0 (OFF), the fan(s) stay OFF until SoC-CP grants permission
 * to turn them ON, so a brown out cannot be triggered before SoC-CP is up.
 */
static bool fan_allow_on = false;

int fan_constraint_set(bool allow_on) {

    if (fan_allow_on == allow_on) {
        return 0;
    }

    fan_allow_on = allow_on;
    LOG_INF("Fan constraint: FAN(s) can %s", allow_on ? "turn ON" : "only stay OFF");

    // Notify service thread to re-evaluate the fan output
    k_event_post(&event, FAN_CFG_UPDATE);

    return 0;
}

int fan_constraint_get(bool *allow_on) {

    if (allow_on == NULL) {
        return -EINVAL;
    }

    *allow_on = fan_allow_on;

    return 0;
}

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

static fan_ctrl_t *fan_blk_get(fan_id_t fan_id) {
    if (fan_id == 0 || fan_id >= ARRAY_SIZE(fan_blks)) {
        return NULL;
    }

    return &fan_blks[fan_id];
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

            // Check SoC-CP fan power constraint
            if (!fan_allow_on) {
                LOG_DBG("Fan%d forced OFF by SoC-CP fan constraint", fan);
                fan_rpm_update(fan_blk, 0);
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

static int acpi_soc_to_ec_temp(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                               uint16_t cmd_len, uint8_t *resp,
                               uint16_t resp_len) {
    ARG_UNUSED(resp);
    ARG_UNUSED(resp_len);
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);

    fan_id_t tmp_src = cmd[1];
    uint16_t tmp = (cmd[4] << 8) | cmd[3]; // Little Endian
    int ret = fan_tmp_set(tmp_src, tmp);
    LOG_DBG("Set temp from SOC: tmp_src=%d, tmp=%d.%d, ret=%d", tmp_src,
            tmp / 10, tmp % 10, ret);

    return ret;
}

static int acpi_ec_fan_status(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                              uint16_t cmd_len, uint8_t *resp,
                              uint16_t resp_len) {
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);
    ACPI_CHECK_OUT(cmd_info, resp, resp_len);

    fan_id_t fan_id = cmd[1];
    fan_ctrl_t *ctrl = fan_blk_get(fan_id);
    if (ctrl == NULL) {
        LOG_ERR("Invalid fan_id=%d", fan_id);
        return -EINVAL;
    }

    resp[0] = ctrl->state;
    LOG_DBG("Fan %d status query: %d", fan_id, resp[0]);

    return 0;
}

static int acpi_ec_fan_rpm(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                           uint16_t cmd_len, uint8_t *resp, uint16_t resp_len) {
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);
    ACPI_CHECK_OUT(cmd_info, resp, resp_len);

    fan_id_t fan_id = cmd[1];
    fan_ctrl_t *ctrl = fan_blk_get(fan_id);
    if (ctrl == NULL) {
        LOG_ERR("Invalid fan_id=%d", fan_id);
        return -EINVAL;
    }

    resp[0] = 2;                       // Byte count
    resp[1] = ctrl->rpm & 0xFF;        // Fan speed LSB
    resp[2] = (ctrl->rpm >> 8) & 0xFF; // Fan speed MSB
    LOG_DBG("Fan %d status rpm: %d", fan_id, ctrl->rpm);

    return 0;
}

static int acpi_ec_fan_profile(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                               uint16_t cmd_len, uint8_t *resp,
                               uint16_t resp_len) {
    fan_id_t profile;
    fan_id_t fan_id = FAN_ID_1; // Default fan ID
    fan_ctrl_t *ctrl;

    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);    // Check for write operation
    ACPI_CHECK_OUT(cmd_info, resp, resp_len); // Check for read operation

    // If cmd_len is (mand + opt), it's a write operation (e.g., 1 + 1 = 2)
    if (cmd_len == (cmd_info->mand + cmd_info->opt)) {
        profile = cmd[1] & 0x0F;
        fan_id = (cmd[1] >> 4) & 0x0F;

        ctrl = fan_blk_get(fan_id);
        if (ctrl == NULL) {
            LOG_ERR("Invalid fan_id=%d", fan_id);
            return -EINVAL;
        }

        ctrl->profile = profile;
        k_event_post(&event, FAN_CFG_UPDATE);

        LOG_DBG("Fan %d profile set to %d", fan_id, profile);
    }

    ctrl = fan_blk_get(fan_id);
    if (ctrl == NULL) {
        LOG_ERR("Invalid fan_id=%d", fan_id);
        return -EINVAL;
    }

    profile = ctrl->profile;
    resp[0] = (fan_id << 4) | (profile & 0x0F);
    LOG_DBG("Fan %d profile: %d", fan_id, profile);

    return 0;
}

static int acpi_ec_fan_trip_point(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                                  uint16_t cmd_len, uint8_t *resp,
                                  uint16_t resp_len) {
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);
    ACPI_CHECK_OUT(cmd_info, resp, resp_len); // Check for read operation

    fan_id_t fan_id = cmd[1];
    uint16_t trip_low, trip_high;
    fan_ctrl_t *ctrl;

    // If cmd_len is 6, it's a write operation (FanID + ByteCount + Low(2) +
    // High(2))
    if (cmd_len == (cmd_info->mand + cmd_info->opt)) { // 2 + 5 = 7
        trip_low = (cmd[4] << 8) | cmd[3];
        trip_high = (cmd[6] << 8) | cmd[5];

        ctrl = fan_blk_get(fan_id);
        if (ctrl == NULL) {
            LOG_ERR("Invalid fan_id=%d", fan_id);
            return -EINVAL;
        }

        ctrl->trip_low = trip_low;
        ctrl->trip_high = trip_high;
        k_event_post(&event, FAN_CFG_UPDATE);

        LOG_DBG("Fan %d trip points set to %d-%d", fan_id, trip_low, trip_high);
    }

    ctrl = fan_blk_get(fan_id);
    if (ctrl == NULL) {
        LOG_ERR("Invalid fan_id=%d", fan_id);
        return -EINVAL;
    }

    trip_low = ctrl->trip_low;
    trip_high = ctrl->trip_high;

    resp[0] = 4; // Byte count
    resp[1] = trip_low & 0xFF;
    resp[2] = (trip_low >> 8) & 0xFF;
    resp[3] = trip_high & 0xFF;
    resp[4] = (trip_high >> 8) & 0xFF;
    LOG_DBG("Fan %d trip points: %d-%d", fan_id, trip_low, trip_high);

    return 0;
}

static int acpi_ec_fan_profile_num(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                                   uint16_t cmd_len, uint8_t *resp,
                                   uint16_t resp_len) {
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);
    ACPI_CHECK_OUT(cmd_info, resp, resp_len);

    fan_id_t fan_id = cmd[1];

    // Validate the requested fan exists before reporting capabilities.
    fan_ctrl_t *ctrl = fan_blk_get(fan_id);
    if (ctrl == NULL) {
        LOG_ERR("Invalid fan_id=%d", fan_id);
        return -EINVAL;
    }

    resp[0] = ctrl->profile;
    LOG_DBG("Fan %d profile: %d", fan_id, ctrl->profile);

    return 0;
}

static int acpi_ec_fan_debug_ctrl(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                                  uint16_t cmd_len, uint8_t *resp,
                                  uint16_t resp_len) {
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);
    ACPI_CHECK_OUT(cmd_info, resp, resp_len);

    fan_id_t fan_id = cmd[1];
    fan_ctrl_t *ctrl;

    // If cmd_len is (mand + opt), it's a write operation
    if (cmd_len == (cmd_info->mand + cmd_info->opt)) { // 2 + 5 = 7
        uint8_t dbg_mode = cmd[3];
        uint16_t dbg_rpm = (cmd[5] << 8) | cmd[4];
        uint8_t dbg_pwm = cmd[6];

        ctrl = fan_blk_get(fan_id);
        if (ctrl == NULL) {
            LOG_ERR("Invalid fan_id=%d", fan_id);
            return -EINVAL;
        }

        ctrl->dbg_mode = dbg_mode;
        ctrl->dbg_rpm = dbg_rpm;
        ctrl->dbg_pwm = dbg_pwm;
        k_event_post(&event, FAN_CFG_UPDATE);

        LOG_DBG("Fan %d debug mode set to %d", fan_id, ctrl->dbg_mode);
        LOG_DBG("Fan %d debug RPM set to %d", fan_id, ctrl->dbg_rpm);
        LOG_DBG("Fan %d debug PWM set to %d", fan_id, ctrl->dbg_pwm);
    }

    ctrl = fan_blk_get(fan_id);
    if (ctrl == NULL) {
        LOG_ERR("Invalid fan_id=%d", fan_id);
        return -EINVAL;
    }

    resp[0] = 4; // Byte count
    resp[1] = ctrl->dbg_mode;
    resp[2] = ctrl->dbg_rpm & 0xFF;
    resp[3] = (ctrl->dbg_rpm >> 8) & 0xFF;
    resp[4] = ctrl->dbg_pwm;
    LOG_DBG("Fan %d debug mode: %d", fan_id, ctrl->dbg_mode);
    LOG_DBG("Fan %d debug RPM: %d", fan_id, ctrl->dbg_rpm);
    LOG_DBG("Fan %d debug PWM: %d", fan_id, ctrl->dbg_pwm);

    return 0;
}

static int acpi_ec_fan_lut_num(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                               uint16_t cmd_len, uint8_t *resp,
                               uint16_t resp_len) {
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);
    ACPI_CHECK_OUT(cmd_info, resp, resp_len);

    int ret = 0;
    fan_id_t fan_id = cmd[1] & 0x0f;
    fan_id_t profile = (cmd[1] >> 4) & 0x0f;
    fan_tbl_t *tbl = NULL;
    uint8_t len = 0;

    ret = fan_tbl_get(profile, fan_id, THERM_SRC_CPU, &tbl,
                      &len); // Assuming CPU as default source for LUT num
    if (ret < 0) {
        LOG_ERR("Failed to get fan lookup table for fan_id=%d: %d", fan_id,
                ret);
        return ret;
    }

    resp[0] = len;
    LOG_DBG("Fan %d lookup table length: %d", fan_id, len);

    return 0;
}

static int acpi_ec_fan_lut(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                           uint16_t cmd_len, uint8_t *resp, uint16_t resp_len) {
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len); // Checks for FanProfileID + TempSrc
    int ret = 0;
    fan_id_t fan_id = cmd[1] & 0x0f;
    fan_id_t profile = (cmd[1] >> 4) & 0x0f;
    fan_id_t tmp_src = cmd[2];
    fan_tbl_t *tbl = NULL;
    uint8_t len = 0;
    uint8_t req_len = 0;

    // If cmd_len > mand, it's a write operation (ByteCount + LUTData present)
    if (cmd_len > cmd_info->mand) { // mand is 3
        // Check if enough data is provided for the ByteCount field
        if (cmd_len < (cmd_info->mand + 1)) {
            LOG_ERR("Invalid input length for LUT write operation: %u (min=%u)",
                    cmd_len, (cmd_info->mand + 1));
            return -EINVAL;
        }
        uint8_t data_byte_count = cmd[3]; // Byte count for LUT data
        if (cmd_len < (cmd_info->mand + 1 + data_byte_count)) {
            LOG_ERR(
                "Insufficient data for LUT write operation: %u (expected %u)",
                cmd_len, (cmd_info->mand + 1 + data_byte_count));
            return -EINVAL;
        }

        ret = fan_tbl_get(profile, fan_id, tmp_src, &tbl, &len);
        if (ret < 0) {
            LOG_ERR("Failed to get fan lookup table for fan_id=%d: %d", fan_id,
                    ret);
            return ret;
        }
        // The actual LUT data starts at cmd[4]
        // len is the max entries, data_byte_count is total bytes (N*3)
        uint8_t entries_to_write =
            data_byte_count /
            sizeof(fan_tbl_t); // Assuming fan_tbl_t is 3 bytes
        if (entries_to_write > len) {
            LOG_WRN("Attempting to write more LUT entries (%u) than available "
                    "space (%u)",
                    entries_to_write, len);
            entries_to_write = len;
        }
        // LUT data starts at cmd[4]; cmd[3] is the byte count field.
        memcpy((uint8_t *)tbl, &cmd[4], entries_to_write * sizeof(fan_tbl_t));
        LOG_DBG("Fan %d, profile %d, source %d LUT updated with %u entries",
                fan_id, profile, tmp_src,
                entries_to_write); // Corrected to use cmd[3]
    }

    ret = fan_tbl_get(profile, fan_id, tmp_src, &tbl, &len);
    if (ret < 0) {
        LOG_ERR("Failed to get fan lookup table for fan_id=%d: %d", fan_id,
                ret);
        return ret;
    }

    req_len = (len * sizeof(fan_tbl_t)) + 1; // +1 for the byte count field
    ACPI_CHECK_OUT(
        cmd_info, resp,
        resp_len); // Check against max possible resp_len from cmd_info

    resp[0] = len * 3; // N * 3 bytes
    memcpy(&resp[1], (uint8_t *)tbl, len * sizeof(fan_tbl_t));

    LOG_DBG("Fan %d, profile %d, source %d lookup table data:", fan_id, profile,
            tmp_src);
    for (uint8_t i = 0; i < len; i++) {
        LOG_DBG("  Entry %d: RPM=%d, Temp High=%d, Temp Low=%d", i, tbl[i].rpm,
                tbl[i].temp_high, tbl[i].temp_low);
    }

    return 0;
}

// clang-format off
ACPI_CMD_SUBSCRIBE(fan, SOC_TO_EC_TEMP,                            acpi_soc_to_ec_temp, 5, 0, 0); // Page 16 (Src + ByteCount + Temp(2))
ACPI_CMD_SUBSCRIBE(fan, EC_FAN_STATUS,                             acpi_ec_fan_status, 2, 0, 1); // Page 17 (FanID)
ACPI_CMD_SUBSCRIBE(fan, EC_FAN_RPM,                                acpi_ec_fan_rpm, 2, 0, 3); // Page 18 (FanID)
// ACPI_CMD_SUBSCRIBE(fan, SOC_TO_EC_MODERN_STANDBY_NOTIFI,           acpi_soc_to_ec_modern_standby_notifi, 2, 0, 0); // Page 19 (Status)
ACPI_CMD_SUBSCRIBE(fan, EC_FAN_PROFILE,                            acpi_ec_fan_profile, 1, 1, 1); // Page 20/21 (Optional FanProfileID)
ACPI_CMD_SUBSCRIBE(fan, EC_FAN_TRIP_POINT,                         acpi_ec_fan_trip_point, 2, 5, 5); // Page 22/23 (FanID + Optional ByteCount + Low(2) + High(2))
ACPI_CMD_SUBSCRIBE(fan, EC_FAN_PROFILE_NUM,                        acpi_ec_fan_profile_num, 2, 0, 1); // Page 24 (FanID)
ACPI_CMD_SUBSCRIBE(fan, EC_FAN_LUT_NUM,                            acpi_ec_fan_lut_num, 2, 0, 1); // Page 25 (FanProfileID)
ACPI_CMD_SUBSCRIBE(fan, EC_FAN_LUT,                                acpi_ec_fan_lut, 3, ACPI_RECE_LEN - 3, ACPI_RESP_LEN); // Page 26/27 (FanProfileID + TempSrc + Optional ByteCount + LUTData)
ACPI_CMD_SUBSCRIBE(fan, EC_FAN_DEBUG_CTRL,                         acpi_ec_fan_debug_ctrl, 2, 5, 5); // Page 29/30 (FanID + Optional ByteCount + Mode + RPM(2) + PWM)
// ACPI_CMD_SUBSCRIBE(fan, EC_FUNC_FLAG,                              acpi_func_flag, 1, 9, 9); // Page 35/36 (Optional ByteCount + Flag(8))
// ACPI_CMD_SUBSCRIBE(fan, EC_ACTIVE_COOLING_SCI_EVENT,               acpi_active_cooling_sci_event, 1, 0, 1); // Page 37
// clang-format on

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

    shell_info(sh, "Fan constraint allow_on: %d", fan_allow_on);
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

static int cmd_fan_constraint(const struct shell *sh, size_t argc, char **argv) {
    bool allow_on;

    if (argc >= 2) {
        fan_constraint_set(strtoul(argv[1], NULL, 0) != 0);
    }

    fan_constraint_get(&allow_on);
    shell_info(sh, "Fan constraint: %d (FAN(s) can %s)", allow_on,
               allow_on ? "turn ON" : "only stay OFF");

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
	SHELL_CMD_ARG(constraint, NULL,
		"Get/set SoC-CP fan constraint [0: FAN(s) OFF, 1: FAN(s) can turn ON]",
		cmd_fan_constraint, 1, 1),
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
