/*
 * @Author: andy.chang 
 * @Date: 2025-07-01 02:46:45 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-20 23:56:04
 */

#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <interface/system.h>
#include <interface/thermal.h>

LOG_MODULE_REGISTER(thermal, LOG_LEVEL_INF);

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

int therm_sensor_blk_get(therm_id_t dev_id, therm_dev_t *blk) {
    if (dev_id == 0 || dev_id >= therm_ctrl.therm_num || blk == NULL) {
        return -EINVAL;
    }

    memcpy(blk, &therm_ctrl.therm_blk[dev_id], sizeof(therm_dev_t));

    return 0;
}

int therm_sensor_blk_set(therm_id_t dev_id, const therm_dev_t *blk) {
    if (dev_id == 0 || dev_id >= therm_ctrl.therm_num || blk == NULL) {
        return -EINVAL;
    }

    memcpy(&therm_ctrl.therm_blk[dev_id], blk, sizeof(therm_dev_t));

    return 0;
}

int therm_adc_sample_rate_get(uint16_t *ms) {
    if (ms == NULL) {
        return -EINVAL;
    }

    *ms = therm_ctrl.sample_ms;

    return 0;
}

int therm_adc_sample_rate_set(uint16_t ms) {
    if (ms < 100) {
        ms = 100;
    }

    therm_ctrl.sample_ms = ms;

    return 0;
}

static void service(void) {
    k_timeout_t adc_wait = K_MSEC(1000);

    while (1) {

        // Wait for event (ADC sample)
        k_sleep(adc_wait);

        // Check thermal cross
        for (therm_id_t i = THERM_DEV_1; i < therm_ctrl.therm_num; i++) {
            uint16_t temp = 0;
            therm_dev_t *therm_dev = &therm_ctrl.therm_blk[i];

            // Get temp from sensor
            int ret = therm_sample_get(i, &temp);
            if (ret < 0) {
                LOG_ERR("Failed to get therm%d : %d", i, ret);
                continue;
            }

            // Update temp
            therm_dev->temp = temp;
            LOG_DBG("Thermal %d: temp: %d C", i, temp);

            // Check 
            if (temp > therm_dev->psv) {
                LOG_WRN("Thermal %d: PSV", i);
            }

            if (temp > therm_dev->cr3) {
                LOG_WRN("Thermal %d: CR3", i);
            }

            if (temp > therm_dev->hot) {
                LOG_WRN("Thermal %d: HOT", i);
            }

            if (temp > therm_dev->crt) {
                LOG_WRN("Thermal %d: CRT", i);
            }
            
        }

        adc_wait = K_MSEC(therm_ctrl.sample_ms);
    }
}

K_THREAD_DEFINE(therm_id, APP_STACK_MIN, service, NULL, NULL, NULL, APP_PRIO_M,
                0, 0);

#ifdef CONFIG_SHELL
#include <zephyr/shell/shell.h>

static void dump_therm_info(const struct shell *sh, therm_dev_t *therm_blk) {
    shell_info(sh, "Therm ID: %d", therm_blk->id);
    shell_info(sh, "  Temp: %d.%d deg C", therm_blk->temp / 10,
               therm_blk->temp % 10);
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

    therm_dev_t blk;

    if (therm_sensor_blk_get(id, &blk) != 0) {
        shell_error(sh, "Invalid therm ID: %d", id);
        return -EINVAL;
    }

    blk.psv = psv;
    blk.cr3 = cr3;
    blk.hot = hot;
    blk.crt = crt;

    therm_sensor_blk_set(id, &blk);
    shell_info(sh, "Updated Thermal %d thresholds", id);
    return 0;
}

static int cmd_therm_sample(const struct shell *sh, size_t argc, char **argv) {
    uint16_t ms = (uint16_t)strtoul(argv[1], NULL, 0);

    therm_adc_sample_rate_set(ms);
    shell_info(sh, "Thermal sample rate set to %d ms", ms);
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
