
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "ec_act_cool.h"
#include <interface/thermal.h>
#include <interface/fan.h>
#include <interface/acpi.h>

LOG_MODULE_DECLARE(acpi, LOG_LEVEL_DBG);

int acpi_soc_to_ec_temp(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp,
                        uint8_t resp_len) {
    if (cmd == NULL || cmd_len < 4) {
        return -EINVAL;
    }

    fan_id_t tmp_src = cmd[0];
    uint16_t tmp = (cmd[3] << 8) | cmd[2]; // Little Endian
    int ret = fan_tmp_set(tmp_src, tmp);
    LOG_DBG("Set temp from SOC: tmp_src=%d, tmp=%d.%d, ret=%d", tmp_src,
            tmp / 10, tmp % 10, ret);

    return ret;
}

int acpi_ec_fan_status(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp,
                       uint8_t resp_len) {
    if (cmd == NULL || cmd_len < 1) {
        LOG_ERR("Invalid command buffer");
        return -EINVAL;
    }
    if (resp == NULL || resp_len < 1) {
        LOG_ERR("Response buffer is too small");
        return -EINVAL;
    }

    fan_id_t fan_id = cmd[0];
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

int acpi_ec_fan_rpm(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp,
                    uint8_t resp_len) {
    fan_id_t fan_id = cmd[0];
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

// int acpi_soc_to_ec_modern_standby_notifi(uint8_t *cmd, uint8_t cmd_len,
//                                          uint8_t *resp, uint8_t resp_len) {}

int acpi_ec_fan_profile(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp,
                        uint8_t resp_len) {
    int ret = 0;
    fan_id_t profile;
    fan_id_t fan_id = FAN_ID_1;
    fan_ctrl_t ctrl;

    if (cmd_len == 1) {
        profile = cmd[0] & 0x0F;
        fan_id = (cmd[0] >> 4) & 0x0F;

        int ret = fan_ctrl_get(fan_id, &ctrl);
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

int acpi_ec_fan_trip_point(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp,
                           uint8_t resp_len) {
    int ret = 0;
    fan_id_t fan_id = cmd[0];
    uint16_t trip_low, trip_high;
    fan_ctrl_t ctrl;

    if (cmd_len == 6) {
        trip_low = (cmd[3] << 8) | cmd[2];
        trip_high = (cmd[5] << 8) | cmd[4];

        int ret = fan_ctrl_get(fan_id, &ctrl);
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

    resp[0] = 4;
    resp[1] = trip_low & 0xFF;
    resp[2] = (trip_low >> 8) & 0xFF;
    resp[3] = trip_high & 0xFF;
    resp[4] = (trip_high >> 8) & 0xFF;
    LOG_DBG("Fan %d trip points: %d-%d", fan_id, trip_low, trip_high);

    return 0;
}

int acpi_ec_fan_profile_num(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp,
                            uint8_t resp_len) {
    fan_id_t fan_id = cmd[0];
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

int acpi_ec_fan_lut_num(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp,
                        uint8_t resp_len) {
    int ret = 0;
    fan_id_t fan_id = cmd[0] & 0x0f;
    fan_id_t profile = (cmd[0] >> 4) & 0x0f;
    fan_tbl_t *tbl = NULL;
    uint8_t len = 0;

    ret = fan_tbl_get(profile, fan_id, THERM_SRC_CPU, &tbl, &len);
    if (ret < 0) {
        LOG_ERR("Failed to get fan lookup table for fan_id=%d: %d", fan_id,
                ret);
        return ret;
    }

    resp[0] = len;
    LOG_DBG("Fan %d lookup table length: %d", fan_id, len);

    return 0;
}

int acpi_ec_fan_lut(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp,
                    uint8_t resp_len) {
    int ret = 0;
    fan_id_t fan_id = cmd[0] & 0x0f;
    fan_id_t profile = (cmd[0] >> 4) & 0x0f;
    fan_id_t tmp_src = cmd[1];
    fan_tbl_t *tbl = NULL;
    uint8_t len = 0;

    if (cmd_len > 2) {
        ret = fan_tbl_get(profile, fan_id, tmp_src, &tbl, &len);
        if (ret < 0) {
            LOG_ERR("Failed to get fan lookup table for fan_id=%d: %d", fan_id,
                    ret);
            return ret;
        }

        len = MIN(len, cmd[2]);
        memcpy((uint8_t *)tbl, &cmd[3], len * sizeof(fan_tbl_t));
    }

    ret = fan_tbl_get(profile, fan_id, tmp_src, &tbl, &len);
    if (ret < 0) {
        LOG_ERR("Failed to get fan lookup table for fan_id=%d: %d", fan_id,
                ret);
        return ret;
    }

    resp[0] = len * 3;
    memcpy(&resp[1], (uint8_t *)tbl, len * sizeof(fan_tbl_t));
    // for (uint8_t i = 0; i < len; i++) {
    //     resp[1 + i*3] = tbl[i].rpm;
    //     resp[2 + i*3] = tbl[i].temp_high;
    //     resp[3 + i*3] = tbl[i].temp_low;
    // }

    LOG_DBG("Fan %d, profile %d, source %d lookup table data:", fan_id, profile,
            tmp_src);
    for (uint8_t i = 0; i < len; i++) {
        LOG_DBG("  Entry %d: RPM=%d, Temp High=%d, Temp Low=%d", i, tbl[i].rpm,
                tbl[i].temp_high, tbl[i].temp_low);
    }

    return 0;
}

static int acpi_ec_thermistors(therm_id_t idx, uint8_t *resp,
                               uint8_t resp_len) {
    therm_dev_t therm_dev;

    int ret = therm_sensor_blk_get(idx, &therm_dev);
    if (ret < 0) {
        LOG_ERR("Failed to get thermistor data for idx=%d: %d", idx, ret);
        return ret;
    }

    resp[0] = 2;
    resp[1] = therm_dev.temp & 0xFF;
    resp[2] = (therm_dev.temp >> 8) & 0xFF;

    LOG_DBG("Thermistor %d temp: %d.%d deg C", idx, therm_dev.temp / 10,
            therm_dev.temp % 10);
    return 0;
}

int acpi_ec_thermistor1(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp,
                        uint8_t resp_len) {
    return acpi_ec_thermistors(THERM_DEV_1, resp, resp_len);
}

int acpi_ec_thermistor2(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp,
                        uint8_t resp_len) {
    return acpi_ec_thermistors(THERM_DEV_2, resp, resp_len);
}

int acpi_ec_thermistor3(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp,
                        uint8_t resp_len) {
    return acpi_ec_thermistors(THERM_DEV_3, resp, resp_len);
}

int acpi_ec_fan_debug_ctrl(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp,
                           uint8_t resp_len) {
    fan_id_t fan_id = cmd[0];
    fan_ctrl_t ctrl;

    if (cmd_len == 6) {
        uint8_t dbg_mode = cmd[2];
        uint16_t dbg_rpm = (cmd[4] << 8) | cmd[3];
        uint8_t dbg_pwm = cmd[5];

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

    resp[0] = 4;
    resp[1] = ctrl.dbg_mode;
    resp[2] = ctrl.dbg_rpm & 0xFF;
    resp[3] = (ctrl.dbg_rpm >> 8) & 0xFF;
    resp[4] = ctrl.dbg_pwm;
    LOG_DBG("Fan %d debug mode: %d", fan_id, ctrl.dbg_mode);
    LOG_DBG("Fan %d debug RPM: %d", fan_id, ctrl.dbg_rpm);
    LOG_DBG("Fan %d debug PWM: %d", fan_id, ctrl.dbg_pwm);

    return 0;
}

int acpi_ec_thermistor_temp_thre(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp,
                                 uint8_t resp_len) {
    therm_id_t idx = cmd[0] & 0x0f;
    therm_dev_t therm_dev;

    int ret = therm_sensor_blk_get(idx, &therm_dev);
    if (ret < 0) {
        LOG_ERR("Failed to get thermistor data for idx=%d: %d", idx, ret);
        return ret;
    }

    if (cmd_len == 6) {
        therm_dev.psv = cmd[2];
        therm_dev.cr3 = cmd[3];
        therm_dev.hot = cmd[4];
        therm_dev.crt = cmd[5];

        ret = therm_sensor_blk_set(idx, &therm_dev);
        if (ret < 0) {
            LOG_ERR("Failed to set thermistor data for idx=%d: %d", idx, ret);
            return ret;
        }
    }

    resp[0] = 0x04;
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

int acpi_ec_thermistor_sampling_rate(uint8_t *cmd, uint8_t cmd_len,
                                     uint8_t *resp, uint8_t resp_len) {
    int ret = 0;
    uint16_t sample_rate_ms = 0;

    if (cmd_len == 2) {
        sample_rate_ms = (cmd[1] << 8) | cmd[0];

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

int acpi_func_flag(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp,
                   uint8_t resp_len) {
    uint64_t flags = 0;
    bool sci_en;

    if (cmd_len == 9) {
        memcpy(&flags, &cmd[1], sizeof(flags));

        sci_en = flags & 0x01;
        acpi_sci_enable_set(sci_en);

        LOG_DBG("Set EC function flags: 0x%016llx", flags);
    }

    acpi_sci_enable_get(&sci_en);
    flags = sci_en;

    resp[0] = 0x08;
    memcpy(&resp[1], &flags, sizeof(flags));

    LOG_DBG("Get EC function flags: 0x%016llx", flags);
    return 0;
}

int acpi_active_cooling_sci_event(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp,
                                  uint8_t resp_len) {
    sci_t sci;
    acpi_sci_get(&sci);
    resp[0] = sci;
    return 0;
}
