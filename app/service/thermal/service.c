/*
 * @Author: andy.chang 
 * @Date: 2025-07-01 02:46:45 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-23 00:12:52
 */

#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <interface/system.h>
#include <interface/power.h>
#include <interface/thermal.h>
#include <interface/acpi.h>

#include <service/acpi/acpi_tbl.h> // ACPI_CHECK_IN/OUT macros for therm handlers

LOG_MODULE_REGISTER(thermal, CONFIG_THERMAL_LOG_LEVEL);

static K_EVENT_DEFINE(event);
SYS_EVENT_SUBSCRIBE(thermal, event);

static therm_dev_t therm_devs[THERM_DEV_MAX] = {
    [THERM_DEV_1] =
        {
            .id = THERM_DEV_1,
            .psv = 80,
            .cr3 = 80,
            .hot = 80,
            .crt = 80,
        },
    [THERM_DEV_2] =
        {
            .id = THERM_DEV_2,
            .psv = 80,
            .cr3 = 80,
            .hot = 80,
            .crt = 80,
        },
    [THERM_DEV_3] =
        {
            .id = THERM_DEV_3,
            .psv = 80,
            .cr3 = 80,
            .hot = 80,
            .crt = 80,
        },
};

static therm_ctrl_t therm_ctrl = {
    .therm_num = ARRAY_SIZE(therm_devs),
    .therm_blk = therm_devs,

    .sample_ms = 1000,
};

static therm_dev_t *therm_blk_get(therm_id_t dev_id) {
    if (dev_id == 0 || dev_id >= therm_ctrl.therm_num) {
        return NULL;
    }

    return &therm_ctrl.therm_blk[dev_id];
}

static void service(void) {
    uint32_t evt = 0;
    k_timeout_t adc_wait = K_MSEC(1000);

    while (1) {

        // Wait for event (ADC sample)
        evt = k_event_wait(&event, (SYS_EVT_MASK), true, adc_wait);

        // Check power state
        {
            pwr_sta_t state;
            pwr_state_get(&state);
            if (state != PWR_STA_S0) {
                adc_wait = K_FOREVER;
                continue;
            }
        }

        // Check thermal cross
        for (therm_id_t i = THERM_DEV_1; i < therm_ctrl.therm_num; i++) {
            int16_t temp = 0;
            therm_dev_t *therm_dev = &therm_ctrl.therm_blk[i];

            // Get temp from sensor
            int ret = therm_sample_get(i, &temp);
            if (ret < 0) {
                LOG_ERR("Failed to get therm%d : %d", i, ret);
                continue;
            }

            // Update temp
            therm_dev->temp = temp;
            LOG_DBG("Thermal %d: temp: %d.%d C", i, temp / 10, abs(temp % 10));

            /* temp is in 0.1C, thresholds (psv, cr3, hot, crt) are in 1C. 
             * Convert thresholds to 0.1C for correct comparison. */
            if (temp > (uint16_t)therm_dev->psv * 10) {
                LOG_WRN("Thermal %d: PSV", i);
            }

            if (temp > (uint16_t)therm_dev->cr3 * 10) {
                LOG_WRN("Thermal %d: CR3", i);
            }

            if (temp > (uint16_t)therm_dev->hot * 10) {
                LOG_WRN("Thermal %d: HOT", i);
            }

            if (temp > (uint16_t)therm_dev->crt * 10) {
                LOG_WRN("Thermal %d: CRT", i);
            }
            
        }

        adc_wait = K_MSEC(therm_ctrl.sample_ms);
    }
}

K_THREAD_DEFINE(therm_id, APP_STACK_MIN, service, NULL, NULL, NULL, APP_PRIO_M,
                0, 0);

static int acpi_ec_thermistors(therm_id_t idx, uint8_t *resp, uint16_t resp_len,
                               const acpi_cmd_t *cmd_info) {
    ARG_UNUSED(resp_len); // resp_len is passed to macro, not used directly here
    ACPI_CHECK_OUT(cmd_info, resp, resp_len);

    therm_dev_t *therm_dev = therm_blk_get(idx);
    if (therm_dev == NULL) {
        LOG_ERR("Invalid thermistor idx=%d", idx);
        return -EINVAL;
    }

    uint16_t report_val;

    /* Format: Bit 15 is sign bit, Bits 0-14 is magnitude in 0.1C */
    if (therm_dev->temp < 0) {
        /* Negative: Set MSB (Bit 15) and store absolute value */
        report_val = (uint16_t)((-therm_dev->temp) & 0x7FFF) | 0x8000;
    } else {
        /* Positive: Clear MSB and store value */
        report_val = (uint16_t)(therm_dev->temp & 0x7FFF);
    }

    resp[0] = 2;                        // Byte count
    resp[1] = report_val & 0xFF;        // Little Endian Low Byte
    resp[2] = (report_val >> 8) & 0xFF; // Little Endian High Byte

    LOG_DBG("Thermistor %d temp: %d.%d deg C (Report: 0x%04x)", idx,
            therm_dev->temp / 10, abs(therm_dev->temp % 10), report_val);
    return 0;
}

static int acpi_ec_thermistor1(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                               uint16_t cmd_len, uint8_t *resp,
                               uint16_t resp_len) {
    ARG_UNUSED(cmd);
    ARG_UNUSED(cmd_len);
    return acpi_ec_thermistors(THERM_DEV_1, resp, resp_len, cmd_info);
}

static int acpi_ec_thermistor2(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                               uint16_t cmd_len, uint8_t *resp,
                               uint16_t resp_len) {
    ARG_UNUSED(cmd);
    ARG_UNUSED(cmd_len);
    return acpi_ec_thermistors(THERM_DEV_2, resp, resp_len, cmd_info);
}

static int acpi_ec_thermistor3(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                               uint16_t cmd_len, uint8_t *resp,
                               uint16_t resp_len) {
    ARG_UNUSED(cmd);
    ARG_UNUSED(cmd_len);
    return acpi_ec_thermistors(THERM_DEV_3, resp, resp_len, cmd_info);
}

static int acpi_ec_thermistor_temp_thre(const acpi_cmd_t *cmd_info,
                                        uint8_t *cmd, uint16_t cmd_len,
                                        uint8_t *resp, uint16_t resp_len) {
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);
    ACPI_CHECK_OUT(cmd_info, resp, resp_len);

    therm_id_t idx = cmd[1] & 0x0f;
    therm_dev_t *therm_dev = therm_blk_get(idx);
    if (therm_dev == NULL) {
        LOG_ERR("Invalid thermistor idx=%d", idx);
        return -EINVAL;
    }

    // If cmd_len is (mand + opt), it's a write operation
    if (cmd_len == (cmd_info->mand + cmd_info->opt)) { // 2 + 5 = 7
        therm_dev->psv = cmd[3];
        therm_dev->cr3 = cmd[4];
        therm_dev->hot = cmd[5];
        therm_dev->crt = cmd[6];
    }

    resp[0] = 0x04; // Byte count
    resp[1] = therm_dev->psv;
    resp[2] = therm_dev->cr3;
    resp[3] = therm_dev->hot;
    resp[4] = therm_dev->crt;

    LOG_DBG("Thermistor %d set psv: %d", idx, therm_dev->psv);
    LOG_DBG("Thermistor %d set cr3: %d", idx, therm_dev->cr3);
    LOG_DBG("Thermistor %d set hot: %d", idx, therm_dev->hot);
    LOG_DBG("Thermistor %d set crt: %d", idx, therm_dev->crt);

    return 0;
}

static int acpi_ec_thermistor_sampling_rate(const acpi_cmd_t *cmd_info,
                                            uint8_t *cmd, uint16_t cmd_len,
                                            uint8_t *resp, uint16_t resp_len) {
    uint16_t sample_rate_ms = 0;

    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);    // Check for read operation
    ACPI_CHECK_OUT(cmd_info, resp, resp_len); // Check for read operation

    // If cmd_len is mand + opt, it's a write operation
    if (cmd_len == (cmd_info->mand + cmd_info->opt)) { // 1 + 2 = 3
        sample_rate_ms = (cmd[2] << 8) | cmd[1];

        if (sample_rate_ms < 100) {
            LOG_WRN("Invalid ADC sample rate: %d ms, using default: 100 ms",
                    sample_rate_ms);
            sample_rate_ms = 100;
        }

        therm_ctrl.sample_ms = sample_rate_ms;
        LOG_DBG("Set ADC sample rate to %d ms", sample_rate_ms);
    }

    sample_rate_ms = therm_ctrl.sample_ms;
    resp[0] = sample_rate_ms & 0xff;
    resp[1] = (sample_rate_ms >> 8) & 0xff;

    LOG_DBG("Get ADC sample rate %d ms", sample_rate_ms);
    return 0;
}

// clang-format off
ACPI_CMD_SUBSCRIBE(fan, EC_THERMISTOR1,                            acpi_ec_thermistor1, 1, 0, 3); // Page 28
ACPI_CMD_SUBSCRIBE(fan, EC_THERMISTOR2,                            acpi_ec_thermistor2, 1, 0, 3); // Page 28
ACPI_CMD_SUBSCRIBE(fan, EC_THERMISTOR3,                            acpi_ec_thermistor3, 1, 0, 3); // Page 28
ACPI_CMD_SUBSCRIBE(fan, EC_THERMISTOR_TEMP_THRE,                   acpi_ec_thermistor_temp_thre, 2, 5, 5); // Page 31/32 (ThermID + Optional ByteCount + PSV + CR3 + HOT + CRT)
ACPI_CMD_SUBSCRIBE(fan, EC_THERMISTOR_SAMPLING_RATE,               acpi_ec_thermistor_sampling_rate, 1, 2, 2); // Page 33/34 (Optional SampleRate(2))
// clang-format on

#ifdef CONFIG_THERMAL_SHELL
#include <zephyr/shell/shell.h>

static void dump_therm_info(const struct shell *sh, therm_dev_t *therm_blk) {
    shell_info(sh, "Therm ID: %d", therm_blk->id);
    shell_info(sh, "  Temp: %d.%d deg C", therm_blk->temp / 10,
               abs(therm_blk->temp % 10));
    shell_info(sh, "  PSV : %d deg C", therm_blk->psv);
    shell_info(sh, "  CR3 : %d deg C", therm_blk->cr3);
    shell_info(sh, "  HOT : %d deg C", therm_blk->hot);
    shell_info(sh, "  CRT : %d deg C", therm_blk->crt);
}

static int cmd_therm_dump(const struct shell *sh, size_t argc, char **argv) {
    for (therm_id_t i = THERM_DEV_1; i < therm_ctrl.therm_num; i++) {
        dump_therm_info(sh, &therm_ctrl.therm_blk[i]);
        shell_info(sh, "-------------------");
    }
    shell_info(sh, "Sample Rate: %d ms", therm_ctrl.sample_ms);
    return 0;
}

static int cmd_therm_set(const struct shell *sh, size_t argc, char **argv) {
    therm_id_t id = (therm_id_t)strtoul(argv[1], NULL, 0);
    uint8_t psv = (uint8_t)strtoul(argv[2], NULL, 0);
    uint8_t cr3 = (uint8_t)strtoul(argv[3], NULL, 0);
    uint8_t hot = (uint8_t)strtoul(argv[4], NULL, 0);
    uint8_t crt = (uint8_t)strtoul(argv[5], NULL, 0);

    therm_dev_t *blk = therm_blk_get(id);
    if (blk == NULL) {
        shell_error(sh, "Invalid therm ID: %d", id);
        return -EINVAL;
    }

    blk->psv = psv;
    blk->cr3 = cr3;
    blk->hot = hot;
    blk->crt = crt;

    shell_info(sh, "Updated Thermal %d thresholds", id);
    return 0;
}

static int cmd_therm_sample(const struct shell *sh, size_t argc, char **argv) {
    uint16_t ms = (uint16_t)strtoul(argv[1], NULL, 0);

    therm_ctrl.sample_ms = ms;
    shell_info(sh, "Thermal sample rate set to %d ms", therm_ctrl.sample_ms);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_therm,
	SHELL_CMD_ARG(dump, NULL, "Dump all thermistors info", cmd_therm_dump, 1, 0),
	SHELL_CMD_ARG(set, NULL, "Set thresholds <id> <psv> <cr3> <hot> <crt>", cmd_therm_set, 6, 0),
	SHELL_CMD_ARG(sample, NULL, "Set sample rate <ms>", cmd_therm_sample, 2, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(therm, &sub_therm, "Thermal commands", NULL);
#endif
