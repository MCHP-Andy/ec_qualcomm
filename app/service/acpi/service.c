/*
 * @Author: andy.chang 
 * @Date: 2025-07-01 02:46:45 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-17 17:47:29
 */

#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

// #include <interface/system.h>
#include <interface/acpi.h>

#include "ec_ver_and_cap.h"
#include "ec_act_cool.h"
#include "ec_fw_update.h"


LOG_MODULE_REGISTER(acpi, LOG_LEVEL_DBG);

#define STACKSIZE 1024
#define PRIORITY 7

typedef struct acpi_cmd_t{
    uint8_t cmd;
    int (*cmd_hdl)(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp, uint8_t resp_len);
} acpi_cmd_t;

static acpi_cmd_t acpi_cmd_tbl[] = {
    // clang-format off
    // EC Version and Capabilities
    {EC_DEV_FW_VER,                             acpi_dev_fw_ver},
    {EC_DEV_FW_VER_AND_LOWEST_SUPPORTED_FW_VER, acpi_dev_fw_ver_and_lowest_supported},
    // {EC_DEV_FLASHING_CAP,                       acpi_dev_flashing_capabilities},
    {EC_DEV_THERMAL_CAP,                        acpi_dev_thermal_capabilities},
    {EC_DEV_ACTIVE_COOLING_IF_VER_AND_CAP,      acpi_dev_active_cooling_caps},
    {EC_ACPI_WHOAMI_IF,                         acpi_who_am_i},
    {EC_DEV_ID,                                 acpi_dev_id},

    // EC Active Cooling Commands
    {SOC_TO_EC_TEMP,                            acpi_soc_to_ec_temp},
    {EC_FAN_STATUS,                             acpi_ec_fan_status},
    {EC_FAN_RPM,                                acpi_ec_fan_rpm},
    // {SOC_TO_EC_MODERN_STANDBY_NOTIFI,           acpi_soc_to_ec_modern_standby_notifi},
    {EC_FAN_PROFILE,                            acpi_ec_fan_profile},
    {EC_FAN_TRIP_POINT,                         acpi_ec_fan_trip_point},
    {EC_FAN_PROFILE_NUM,                        acpi_ec_fan_profile_num},
    {EC_FAN_LUT_NUM,                            acpi_ec_fan_lut_num},
    {EC_FAN_LUT,                                acpi_ec_fan_lut},
    {EC_THERMISTOR1,                            acpi_ec_thermistor1},
    {EC_THERMISTOR2,                            acpi_ec_thermistor2},
    {EC_THERMISTOR3,                            acpi_ec_thermistor3},
    {EC_FAN_DEBUG_CTRL,                         acpi_ec_fan_debug_ctrl},
    {EC_THERMISTOR_TEMP_THRE,                   acpi_ec_thermistor_temp_thre},
    {EC_THERMISTOR_SAMPLING_RATE,               acpi_ec_thermistor_sampling_rate},
    {EC_FUNC_FLAG, NULL},
    {EC_ACTIVE_COOLING_SCI_EVENT, NULL},

    // EC Firmware Update Commands
    {EC_DEV_FW_CORRUPTION_STATUS, NULL},
    {EC_FW_CODE_MIRROR, NULL},
    {EC_READ_CRC, NULL},
    {EC_STATE_AND_WP_STATUS, NULL},
    {EC_ERASE_MEM_REGION, NULL},
    {EC_ERASE_MEM_PARTITION, NULL},
    {EC_READ_MEM_REGION, NULL},
    {EC_READ_MEM_REGION_BUF, NULL},
    {EC_WRITE_MEM_REGION, NULL},
    {EC_WRITE_MEM_REGION_BUF, NULL}
    // clang-format on
};

static uint8_t rece_cmd[ACPI_RECE_LEN];
static uint8_t resp_buf[ACPI_RESP_LEN];

#define ACPI_EVT_LEN 2

typedef struct acpi_evt_t {
    uint8_t *pdata;
    uint16_t len;
} acpi_evt_t ;

K_MSGQ_DEFINE(acpi_evt_queue, sizeof(acpi_evt_t), ACPI_EVT_LEN, 4);

int acpi_write(uint8_t *data, uint16_t len) {
    acpi_evt_t event;

    LOG_DBG("Put ACPI CMD");

    event.pdata = data;
    event.len = len;

    return k_msgq_put(&acpi_evt_queue, &event, K_NO_WAIT);
}

int acpi_read(uint8_t *data, uint16_t len) {
    uint16_t max = MIN(len, sizeof(resp_buf));

    if (data == NULL) {
        return -ENOMEM;
    }

    memcpy(data, resp_buf, max);
    return 0;
}

static void service(void) {


    while (1) {
        uint8_t len = 0;
        acpi_evt_t event;

        // Wait for acpi event
        k_msgq_get(&acpi_evt_queue, &event, K_FOREVER);

        // Copy ACPI CMD from interface buffer to local variable
        len = event.len;
        memcpy(rece_cmd, event.pdata, len);
        
        // Parse the cmd and call corresponding handler function, then copy response to interface buffer
        size_t i = 0;
        for (i = 0; i < ARRAY_SIZE(acpi_cmd_tbl); i++) {
            if (rece_cmd[0] == acpi_cmd_tbl[i].cmd) {
                if (acpi_cmd_tbl[i].cmd_hdl != NULL) {
                    int ret = acpi_cmd_tbl[i].cmd_hdl(&rece_cmd[1], len-1, resp_buf, sizeof(resp_buf));
                    if (ret < 0) {
                        LOG_ERR("Failed to handle ACPI cmd 0x%02x: %d", rece_cmd[0], ret);
                    } else {
                        LOG_INF("Handled ACPI cmd 0x%02x successfully", rece_cmd[0]);
                    }
                } else {
                    LOG_WRN("No handler for ACPI cmd 0x%02x", rece_cmd[0]);
                }
                break;
            }
        }

        if (i == ARRAY_SIZE(acpi_cmd_tbl)) {
            LOG_WRN("Unknown ACPI cmd 0x%02x", rece_cmd[0]);
        }
    }
}

K_THREAD_DEFINE(acpi_id, STACKSIZE, service, NULL, NULL, NULL, PRIORITY, 0, 0);

#ifdef CONFIG_SHELL
#include <zephyr/shell/shell.h>

static int cmd_acpi_write(const struct shell *sh, size_t argc, char **argv) {
    int ret = 0;
    uint8_t buff[64];
    uint16_t max = MIN(argc - 1, sizeof(buff));

    for (size_t i = 0; i < max; i++) {
        buff[i] = strtoul(argv[i + 1], NULL, 16);
    }

    ret = acpi_write(buff, max);
    if (ret < 0) {
        shell_error(sh, "Failed to write ACPI CMD: %d", ret);
    } else {
        shell_info(sh, "ACPI CMD written successfully");
    }
    
    return ret;
}

static int cmd_acpi_read(const struct shell *sh, size_t argc, char **argv) {
    int ret = 0;
    uint8_t buff[64];

    ret = acpi_read(buff, sizeof(buff));
    if (ret < 0) {
        shell_error(sh, "Failed to read ACPI CMD: %d", ret);
        return ret;
    }

    shell_print(sh, "ACPI Read Response:");
    shell_hexdump(sh, buff, sizeof(buff));

    return ret;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_acpi,
	SHELL_CMD_ARG(write, NULL,
		"Write ACPI CMD", cmd_acpi_write, 1, 64),
	SHELL_CMD_ARG(read, NULL,
		"Read ACPI CMD", cmd_acpi_read, 0, 0),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(acpi, &sub_acpi, "ACPI commands", NULL);
#endif
