/*
 * @Author: andy.chang 
 * @Date: 2026-04-18 11:29:06 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-23 00:37:00
 */

#include <errno.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>

#include <interface/thermal.h>

LOG_MODULE_REGISTER(therm_dev, LOG_LEVEL_INF);

static const struct adc_dt_spec dev_list[THERM_DEV_MAX] = {
    [THERM_DEV_1] = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0),
    [THERM_DEV_2] = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 1),
    [THERM_DEV_3] = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 2),
};

int therm_sample_get(therm_id_t id, uint16_t *temp) {
    int ret;

    if (id >= ARRAY_SIZE(dev_list)) {
        LOG_ERR("Invalid therm ID: %d", id);
        return -EINVAL;
    }

    if (temp == NULL) {
        LOG_WRN("Invalid temp pointer");
        return -EINVAL;
    }

    if (dev_list[id].dev == NULL) {
        LOG_WRN("Invalid therm device");
        return -EINVAL;
    }

    uint16_t raw_buf;
    struct adc_sequence sequence = {
        .buffer = &raw_buf,
        .buffer_size = sizeof(raw_buf),
    };

    adc_sequence_init_dt(&dev_list[id], &sequence);

    ret = adc_read_dt(&dev_list[id], &sequence);
    if (ret < 0)
        return ret;

    // Convert to mV
    int32_t val_mv = (int32_t)raw_buf;
    ret = adc_raw_to_millivolts_dt(&dev_list[id], &val_mv);
    if (ret < 0) {
		LOG_ERR(" (value in mV not available)");
	}
    LOG_DBG(" = %"PRId32" mV", val_mv);

    // TODO: Convert to deg C
    *temp = 55;

    LOG_DBG("Therm%d, %d C", id, *temp);

    return 0;
}

#include <zephyr/init.h>

static int init_config(void) {
    int ret = 0;

    LOG_INF("Initializing thermal sensor...");

    for (size_t i = 0; i < ARRAY_SIZE(dev_list); i++) {
        if (!device_is_ready(dev_list[i].dev)) {
            LOG_ERR("ADC device for channel %d not ready", i);
            ret = -ENODEV;
        } else {
            ret = adc_channel_setup_dt(&dev_list[i]);
            if (ret < 0) {
                LOG_ERR("Setup failed for channel %d: %d", i, ret);
            }
        }
    }

    return ret;
}

SYS_INIT(init_config, APPLICATION, 0);

#ifdef CONFIG_SHELL
#include <zephyr/shell/shell.h>

static int cmd_therm_get(const struct shell *sh, size_t argc, char **argv) {
    therm_id_t id = 0;
    uint16_t temp = 0;

    id = (therm_id_t)strtoul(argv[1], NULL, 0);

    int ret = therm_sample_get(id, &temp);
    if (ret < 0) {
        shell_error(sh, "Failed to get therm%d : %d", id, ret);
    } else {
        shell_info(sh, "Therm%d temp is %d C", id, temp);
    }

    return ret;
}

SHELL_STATIC_SUBCMD_SET_CREATE(therm_dev,
	SHELL_CMD_ARG(get, NULL,
		"Get therm", cmd_therm_get, 2, 0),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(therm_dev, &therm_dev, "Thermal device commands", NULL);
#endif
