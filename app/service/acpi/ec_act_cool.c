
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <interface/acpi.h>
#include <interface/fan.h>
#include <interface/thermal.h>

#include "acpi_tbl.h" // Include the new acpi_tbl.h for macros

LOG_MODULE_DECLARE(acpi, CONFIG_ACPI_LOG_LEVEL);

int acpi_soc_to_ec_temp(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                        uint16_t cmd_len, uint8_t *resp, uint16_t resp_len) {
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

int acpi_ec_fan_status(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                       uint16_t cmd_len, uint8_t *resp, uint16_t resp_len) {
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);
    ACPI_CHECK_OUT(cmd_info, resp, resp_len);

    fan_id_t fan_id = cmd[1];
    fan_ctrl_t ctrl;

    int ret = fan_ctrl_get(fan_id, &ctrl);
    if (ret < 0) {
        LOG_ERR("Failed to get fan control for fan_id=%d: %d", fan_id, ret);
        return ret;
    }

    resp[0] = ctrl.state;
    LOG_DBG("Fan %d status query: %d", fan_id, resp[0]);

    return 0;
}

int acpi_ec_fan_rpm(const acpi_cmd_t *cmd_info, uint8_t *cmd, uint16_t cmd_len,
                    uint8_t *resp, uint16_t resp_len) {
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);
    ACPI_CHECK_OUT(cmd_info, resp, resp_len);

    fan_id_t fan_id = cmd[1];
    fan_ctrl_t ctrl;

    int ret = fan_ctrl_get(fan_id, &ctrl);
    if (ret < 0) {
        LOG_ERR("Failed to get fan control for fan_id=%d: %d", fan_id, ret);
        return ret;
    }

    resp[0] = ctrl.rpm & 0xFF;
    resp[1] = (ctrl.rpm >> 8) & 0xFF;
    LOG_DBG("Fan %d status rpm: %d", fan_id, ctrl.rpm);

    return 0;
}

int acpi_soc_to_ec_modern_standby_notifi(const acpi_cmd_t *cmd_info,
                                         uint8_t *cmd, uint16_t cmd_len,
                                         uint8_t *resp, uint16_t resp_len) {
    ARG_UNUSED(cmd_info);
    ARG_UNUSED(resp);
    ARG_UNUSED(resp_len);
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);

    uint8_t standby_status = cmd[1];
    LOG_DBG("SoC Modern Standby notification: %d", standby_status);
    // TODO: Implement actual handling for modern standby notification
    return 0;
}

int acpi_ec_fan_profile(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                        uint16_t cmd_len, uint8_t *resp, uint16_t resp_len) {
    int ret = 0;
    fan_id_t profile;
    fan_id_t fan_id = FAN_ID_1; // Default fan ID
    fan_ctrl_t ctrl;

    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);    // Check for write operation
    ACPI_CHECK_OUT(cmd_info, resp, resp_len); // Check for read operation

    // If cmd_len is (mand + opt), it's a write operation (e.g., 1 + 1 = 2)
    if (cmd_len == (cmd_info->mand + cmd_info->opt)) {
        profile = cmd[1] & 0x0F;
        fan_id = (cmd[1] >> 4) & 0x0F;

        ret = fan_ctrl_get(fan_id, &ctrl);
        if (ret < 0) {
            LOG_ERR("Failed to get fan control for fan_id=%d: %d", fan_id, ret);
            return ret;
        }

        ctrl.profile = profile;

        ret = fan_ctrl_set(fan_id, &ctrl);
        if (ret < 0) {
            LOG_ERR("Failed to set fan control for fan_id=%d: %d", fan_id, ret);
            return ret;
        }

        LOG_DBG("Fan %d profile set to %d", fan_id, profile);
    }

    ret = fan_ctrl_get(fan_id, &ctrl);
    if (ret < 0) {
        LOG_ERR("Failed to get fan control for fan_id=%d: %d", fan_id, ret);
        return ret;
    }

    profile = ctrl.profile;
    resp[0] = (fan_id << 4) | (profile & 0x0F);
    LOG_DBG("Fan %d profile: %d", fan_id, profile);

    return 0;
}

int acpi_ec_fan_trip_point(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                           uint16_t cmd_len, uint8_t *resp, uint16_t resp_len) {
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);
    ACPI_CHECK_OUT(cmd_info, resp, resp_len); // Check for read operation

    int ret = 0;
    fan_id_t fan_id = cmd[1];
    uint16_t trip_low, trip_high;
    fan_ctrl_t ctrl;

    // If cmd_len is 6, it's a write operation (FanID + ByteCount + Low(2) +
    // High(2))
    if (cmd_len == (cmd_info->mand + cmd_info->opt)) { // 2 + 5 = 7
        trip_low = (cmd[4] << 8) | cmd[3];
        trip_high = (cmd[6] << 8) | cmd[5];

        ret = fan_ctrl_get(fan_id, &ctrl);
        if (ret < 0) {
            LOG_ERR("Failed to get fan control for fan_id=%d: %d", fan_id, ret);
            return ret;
        }

        ctrl.trip_low = trip_low;
        ctrl.trip_high = trip_high;

        ret = fan_ctrl_set(fan_id, &ctrl);
        if (ret < 0) {
            LOG_ERR("Failed to set fan control for fan_id=%d: %d", fan_id, ret);
            return ret;
        }

        LOG_DBG("Fan %d trip points set to %d-%d", fan_id, trip_low, trip_high);
    }

    ret = fan_ctrl_get(fan_id, &ctrl);
    if (ret < 0) {
        LOG_ERR("Failed to get fan control for fan_id=%d: %d", fan_id, ret);
        return ret;
    }

    trip_low = ctrl.trip_low;
    trip_high = ctrl.trip_high;

    resp[0] = 4; // Byte count
    resp[1] = trip_low & 0xFF;
    resp[2] = (trip_low >> 8) & 0xFF;
    resp[3] = trip_high & 0xFF;
    resp[4] = (trip_high >> 8) & 0xFF;
    LOG_DBG("Fan %d trip points: %d-%d", fan_id, trip_low, trip_high);

    return 0;
}

int acpi_ec_fan_profile_num(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                            uint16_t cmd_len, uint8_t *resp,
                            uint16_t resp_len) {
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);
    ACPI_CHECK_OUT(cmd_info, resp, resp_len);

    fan_id_t fan_id = cmd[1];
    fan_ctrl_t ctrl;

    int ret = fan_ctrl_get(fan_id, &ctrl);
    if (ret < 0) {
        LOG_ERR("Failed to get fan control for fan_id=%d: %d", fan_id, ret);
        return ret;
    }

    resp[0] = ctrl.profile;
    LOG_DBG("Fan %d profile: %d", fan_id, ctrl.profile);

    return 0;
}

int acpi_ec_fan_lut_num(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                        uint16_t cmd_len, uint8_t *resp, uint16_t resp_len) {
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

int acpi_ec_fan_lut(const acpi_cmd_t *cmd_info, uint8_t *cmd, uint16_t cmd_len,
                    uint8_t *resp, uint16_t resp_len) {
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
        memcpy((uint8_t *)tbl, &cmd[3], entries_to_write * sizeof(fan_tbl_t));
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

static int acpi_ec_thermistors(therm_id_t idx, uint8_t *resp, uint16_t resp_len,
                               const acpi_cmd_t *cmd_info) {
    therm_dev_t therm_dev;
    ARG_UNUSED(resp_len); // resp_len is passed to macro, not used directly here
    ACPI_CHECK_OUT(cmd_info, resp, resp_len);

    int ret = therm_sensor_blk_get(idx, &therm_dev);
    if (ret < 0) {
        LOG_ERR("Failed to get thermistor data for idx=%d: %d", idx, ret);
        return ret;
    }

    uint16_t report_val;

    /* Format: Bit 15 is sign bit, Bits 0-14 is magnitude in 0.1C */
    if (therm_dev.temp < 0) {
        /* Negative: Set MSB (Bit 15) and store absolute value */
        report_val = (uint16_t)((-therm_dev.temp) & 0x7FFF) | 0x8000;
    } else {
        /* Positive: Clear MSB and store value */
        report_val = (uint16_t)(therm_dev.temp & 0x7FFF);
    }

    resp[0] = 2; // Byte count
    resp[1] = report_val & 0xFF;        // Little Endian Low Byte
    resp[2] = (report_val >> 8) & 0xFF; // Little Endian High Byte

    LOG_DBG("Thermistor %d temp: %d.%d deg C (Report: 0x%04x)", idx, 
            therm_dev.temp / 10, abs(therm_dev.temp % 10), report_val);
    return 0;
}

int acpi_ec_thermistor1(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                        uint16_t cmd_len, uint8_t *resp, uint16_t resp_len) {
    ARG_UNUSED(cmd);
    ARG_UNUSED(cmd_len);
    return acpi_ec_thermistors(THERM_DEV_1, resp, resp_len, cmd_info);
}

int acpi_ec_thermistor2(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                        uint16_t cmd_len, uint8_t *resp, uint16_t resp_len) {
    ARG_UNUSED(cmd);
    ARG_UNUSED(cmd_len);
    return acpi_ec_thermistors(THERM_DEV_2, resp, resp_len, cmd_info);
}

int acpi_ec_thermistor3(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                        uint16_t cmd_len, uint8_t *resp, uint16_t resp_len) {
    ARG_UNUSED(cmd);
    ARG_UNUSED(cmd_len);
    return acpi_ec_thermistors(THERM_DEV_3, resp, resp_len, cmd_info);
}

int acpi_ec_fan_debug_ctrl(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                           uint16_t cmd_len, uint8_t *resp, uint16_t resp_len) {
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);
    ACPI_CHECK_OUT(cmd_info, resp, resp_len);

    fan_id_t fan_id = cmd[1];
    fan_ctrl_t ctrl;

    // If cmd_len is (mand + opt), it's a write operation
    if (cmd_len == (cmd_info->mand + cmd_info->opt)) { // 2 + 5 = 7
        uint8_t dbg_mode = cmd[3];
        uint16_t dbg_rpm = (cmd[5] << 8) | cmd[4];
        uint8_t dbg_pwm = cmd[6];

        int ret = fan_ctrl_get(fan_id, &ctrl);
        if (ret < 0) {
            LOG_ERR("Failed to get fan control for fan_id=%d: %d", fan_id, ret);
            return ret;
        }

        ctrl.dbg_mode = dbg_mode;
        ctrl.dbg_rpm = dbg_rpm;
        ctrl.dbg_pwm = dbg_pwm;

        ret = fan_ctrl_set(fan_id, &ctrl);
        if (ret < 0) {
            LOG_ERR("Failed to set fan control for fan_id=%d: %d", fan_id, ret);
            return ret;
        }

        LOG_DBG("Fan %d debug mode set to %d", fan_id, ctrl.dbg_mode);
        LOG_DBG("Fan %d debug RPM set to %d", fan_id, ctrl.dbg_rpm);
        LOG_DBG("Fan %d debug PWM set to %d", fan_id, ctrl.dbg_pwm);
    }

    int ret = fan_ctrl_get(fan_id, &ctrl);
    if (ret < 0) {
        LOG_ERR("Failed to get fan control for fan_id=%d: %d", fan_id, ret);
        return ret;
    }

    resp[0] = 4; // Byte count
    resp[1] = ctrl.dbg_mode;
    resp[2] = ctrl.dbg_rpm & 0xFF;
    resp[3] = (ctrl.dbg_rpm >> 8) & 0xFF;
    resp[4] = ctrl.dbg_pwm;
    LOG_DBG("Fan %d debug mode: %d", fan_id, ctrl.dbg_mode);
    LOG_DBG("Fan %d debug RPM: %d", fan_id, ctrl.dbg_rpm);
    LOG_DBG("Fan %d debug PWM: %d", fan_id, ctrl.dbg_pwm);

    return 0;
}

int acpi_ec_thermistor_temp_thre(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                                 uint16_t cmd_len, uint8_t *resp,
                                 uint16_t resp_len) {
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);
    ACPI_CHECK_OUT(cmd_info, resp, resp_len);

    therm_id_t idx = cmd[1] & 0x0f;
    therm_dev_t therm_dev;

    int ret = therm_sensor_blk_get(idx, &therm_dev);
    if (ret < 0) {
        LOG_ERR("Failed to get thermistor data for idx=%d: %d", idx, ret);
        return ret;
    }

    // If cmd_len is (mand + opt), it's a write operation
    if (cmd_len == (cmd_info->mand + cmd_info->opt)) { // 2 + 5 = 7
        therm_dev.psv = cmd[3];
        therm_dev.cr3 = cmd[4];
        therm_dev.hot = cmd[5];
        therm_dev.crt = cmd[6];

        ret = therm_sensor_blk_set(idx, &therm_dev);
        if (ret < 0) {
            LOG_ERR("Failed to set thermistor data for idx=%d: %d", idx, ret);
            return ret;
        }
    }

    resp[0] = 0x04; // Byte count
    resp[1] = therm_dev.psv;
    resp[2] = therm_dev.cr3;
    resp[3] = therm_dev.hot;
    resp[4] = therm_dev.crt;

    LOG_DBG("Thermistor %d set psv: %d", idx, therm_dev.psv);
    LOG_DBG("Thermistor %d set cr3: %d", idx, therm_dev.cr3);
    LOG_DBG("Thermistor %d set hot: %d", idx, therm_dev.hot);
    LOG_DBG("Thermistor %d set crt: %d", idx, therm_dev.crt);

    return 0;
}

int acpi_ec_thermistor_sampling_rate(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                                     uint16_t cmd_len, uint8_t *resp,
                                     uint16_t resp_len) {
    int ret = 0;
    uint16_t sample_rate_ms = 0;

    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);    // Check for read operation
    ACPI_CHECK_OUT(cmd_info, resp, resp_len); // Check for read operation

    // If cmd_len is mand + opt, it's a write operation
    if (cmd_len == (cmd_info->mand + cmd_info->opt)) { // 1 + 2 = 3
        ACPI_CHECK_IN(cmd_info, cmd, cmd_len); // Check for write operation
        sample_rate_ms = (cmd[2] << 8) | cmd[1];

        ret = therm_adc_sample_rate_set(sample_rate_ms);
        if (ret < 0) {
            LOG_ERR("Failed to set ADC sample rate: %d", ret);
            return ret;
        }

        LOG_DBG("Set ADC sample rate to %d ms", sample_rate_ms);
    }

    ret = therm_adc_sample_rate_get(&sample_rate_ms);
    if (ret < 0) {
        LOG_ERR("Failed to get ADC sample rate: %d", ret);
        return ret;
    }
    resp[0] = sample_rate_ms & 0xff;
    resp[1] = (sample_rate_ms >> 8) & 0xff;

    LOG_DBG("Get ADC sample rate %d ms", sample_rate_ms);
    return 0;
}

int acpi_func_flag(const acpi_cmd_t *cmd_info, uint8_t *cmd, uint16_t cmd_len,
                   uint8_t *resp, uint16_t resp_len) {
    uint64_t flags = 0;
    bool sci_en;

    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);    // Check for read operation
    ACPI_CHECK_OUT(cmd_info, resp, resp_len); // Check for read operation

    // If cmd_len is mand + opt, it's a write operation
    if (cmd_len == (cmd_info->mand + cmd_info->opt)) { // 1 + 9 = 10
        ACPI_CHECK_IN(cmd_info, cmd, cmd_len); // Check for write operation
        memcpy(&flags, &cmd[2], sizeof(flags));

        sci_en = flags & 0x01;
        acpi_sci_enable_set(sci_en);
        LOG_DBG("Set EC function flags: 0x%016llx", flags);
    }

    acpi_sci_enable_get(&sci_en);
    flags = sci_en;

    resp[0] = 0x08; // Byte count
    memcpy(&resp[1], &flags, sizeof(flags));

    LOG_DBG("Get EC function flags: 0x%016llx", flags);
    return 0;
}

int acpi_active_cooling_sci_event(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                                  uint16_t cmd_len, uint8_t *resp,
                                  uint16_t resp_len) {
    sci_t sci;
    ARG_UNUSED(cmd);
    ARG_UNUSED(cmd_len);
    ACPI_CHECK_OUT(cmd_info, resp, resp_len);

    acpi_sci_get(&sci);
    resp[0] = sci;
    return 0;
}
