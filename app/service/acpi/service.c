/*
 * @Author: andy.chang 
 * @Date: 2025-07-01 02:46:45 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-23 23:17:00
 */

#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <interface/system.h>
#include <interface/acpi.h>

#include "acpi_tbl.h"

LOG_MODULE_REGISTER(acpi, CONFIG_ACPI_LOG_LEVEL);

enum {
    ACPI_EVT_CMD = LOCAL_EVT_START,
    ACPI_EVT_SCI,
};

#define ACPI_CMD BIT(ACPI_EVT_CMD)
#define ACPI_SCI BIT(ACPI_EVT_SCI)

static K_EVENT_DEFINE(event);
SYS_EVENT_SUBSCRIBE(acpi, event);

static const acpi_cmd_t acpi_cmd_tbl[] = {
    // clang-format off
    // EC Version and Capabilities
    // Cmd, Handler, Mand args (incl cmd), Opt args, Resp len
    {EC_DEV_FW_VER,                             acpi_dev_fw_ver, 1, 0, 4}, // Page 8
    {EC_DEV_FW_VER_AND_LOWEST_SUPPORTED_FW_VER, acpi_dev_fw_ver_and_lowest_supported, 1, 0, 8}, // Page 9
    // {EC_DEV_FLASHING_CAP,                       acpi_dev_flashing_capabilities, 1, 0, 65}, // Page 10 (Not Supported)
    {EC_DEV_THERMAL_CAP,                        acpi_dev_thermal_capabilities, 2, 0, 3}, // Page 11 (SubCmd)
    {EC_DEV_ACTIVE_COOLING_IF_VER_AND_CAP,      acpi_dev_active_cooling_caps, 1, 0, 6}, // Page 12
    {EC_ACPI_WHOAMI_IF,                         acpi_who_am_i, 1, 0, 1}, // Page 13
    {EC_DEV_ID,                                 acpi_dev_id, 1, 0, 3}, // Page 14

    // EC Active Cooling Commands
    {SOC_TO_EC_TEMP,                            acpi_soc_to_ec_temp, 5, 0, 0}, // Page 16 (Src + ByteCount + Temp(2))
    {EC_FAN_STATUS,                             acpi_ec_fan_status, 2, 0, 1}, // Page 17 (FanID)
    {EC_FAN_RPM,                                acpi_ec_fan_rpm, 2, 0, 3}, // Page 18 (FanID)
    {SOC_TO_EC_MODERN_STANDBY_NOTIFI,           acpi_soc_to_ec_modern_standby_notifi, 2, 0, 0}, // Page 19 (Status)
    {EC_FAN_PROFILE,                            acpi_ec_fan_profile, 1, 1, 1}, // Page 20/21 (Optional FanProfileID)
    {EC_FAN_TRIP_POINT,                         acpi_ec_fan_trip_point, 2, 5, 5}, // Page 22/23 (FanID + Optional ByteCount + Low(2) + High(2))
    {EC_FAN_PROFILE_NUM,                        acpi_ec_fan_profile_num, 2, 0, 1}, // Page 24 (FanID)
    {EC_FAN_LUT_NUM,                            acpi_ec_fan_lut_num, 2, 0, 1}, // Page 25 (FanProfileID)
    {EC_FAN_LUT,                                acpi_ec_fan_lut, 3, ACPI_RECE_LEN - 3, ACPI_RESP_LEN}, // Page 26/27 (FanProfileID + TempSrc + Optional ByteCount + LUTData)
    {EC_THERMISTOR1,                            acpi_ec_thermistor1, 1, 0, 3}, // Page 28
    {EC_THERMISTOR2,                            acpi_ec_thermistor2, 1, 0, 3}, // Page 28
    {EC_THERMISTOR3,                            acpi_ec_thermistor3, 1, 0, 3}, // Page 28
    {EC_FAN_DEBUG_CTRL,                         acpi_ec_fan_debug_ctrl, 2, 5, 5}, // Page 29/30 (FanID + Optional ByteCount + Mode + RPM(2) + PWM)
    {EC_THERMISTOR_TEMP_THRE,                   acpi_ec_thermistor_temp_thre, 2, 5, 5}, // Page 31/32 (ThermID + Optional ByteCount + PSV + CR3 + HOT + CRT)
    {EC_THERMISTOR_SAMPLING_RATE,               acpi_ec_thermistor_sampling_rate, 1, 2, 2}, // Page 33/34 (Optional SampleRate(2))
    {EC_FUNC_FLAG,                              acpi_func_flag, 1, 9, 9}, // Page 35/36 (Optional ByteCount + Flag(8))
    {EC_ACTIVE_COOLING_SCI_EVENT,               acpi_active_cooling_sci_event, 1, 0, 1}, // Page 37

    // EC Firmware Update Commands
    {EC_DEV_FW_CORRUPTION_STATUS, NULL, 1, 0, 2}, // Page 39
    {EC_FW_CODE_MIRROR, NULL, 3, 0, 0}, // Page 40 (ByteCount + CodeMirrorCmd)
    {EC_READ_CRC, NULL, 1, 0, 6}, // Page 46
    {EC_STATE_AND_WP_STATUS, NULL, 1, 0, 3}, // Page 47
    {EC_ERASE_MEM_REGION, NULL, 7, 0, 0}, // Page 48 (ByteCount + ECControlPath + BlockCount + Address(3))
    {EC_ERASE_MEM_PARTITION, NULL, 3, 0, 0}, // Page 49 (ByteCount + QuickEraseCmd)
    {EC_READ_MEM_REGION, NULL, 6, 0, ACPI_RESP_LEN}, // Page 50 (ByteCount + ECControlPath + Address(3))
    // EC_READ_MEM_REGION_BUF (0xA1) is a response command, not a top-level request command.
    {EC_WRITE_MEM_REGION, NULL, 1, ACPI_RECE_LEN - 1, 0}, // Page 51 (Optional Data(variable))
    {EC_WRITE_MEM_REGION_BUF, NULL, 7, 0, 0} // Page 51 (ByteCount + ECControlPath + Size + Address(3))
    // clang-format on
};

static uint8_t rece_cmd[ACPI_RECE_LEN];
static uint8_t resp_buf[ACPI_RESP_LEN];
static sci_t sci_buf;
static bool sci_en = true;

#define ACPI_EVT_LEN 2
#define SCI_LEN 16

typedef struct acpi_evt_t {
    uint8_t *pdata;
    uint16_t len;
} acpi_evt_t ;

K_MSGQ_DEFINE(acpi_evt_queue, sizeof(acpi_evt_t), ACPI_EVT_LEN, 4);
K_MSGQ_DEFINE(sci_queue, sizeof(sci_t), SCI_LEN, 4);

int acpi_cmd_info_get(uint8_t cmd, const acpi_cmd_t *cmd_info) {
    if (cmd_info == NULL) {
        return -EINVAL;
    }

    for (size_t i = 0; i < ARRAY_SIZE(acpi_cmd_tbl); i++) {
        if (acpi_cmd_tbl[i].cmd == cmd) {
            memcpy((void *)cmd_info, &acpi_cmd_tbl[i], sizeof(acpi_cmd_t));
            return 0;
        }
    }
    return -EINVAL;
}

int acpi_write(uint8_t *data, uint16_t len) {
    int ret = 0;
    acpi_evt_t cmd;

    LOG_DBG("Put ACPI CMD");

    cmd.pdata = data;
    cmd.len = len;

    ret = k_msgq_put(&acpi_evt_queue, &cmd, K_NO_WAIT);
    if (ret < 0) {
        LOG_ERR("Put ACPI CMD fail: %d", ret);
        return ret;
    }

    k_event_post(&event, ACPI_CMD);

    return ret;
}

int acpi_read(uint8_t *data, uint16_t len) {
    uint16_t max = MIN(len, sizeof(resp_buf));

    if (data == NULL) {
        return -ENOMEM;
    }

    memcpy(data, resp_buf, max);
    return 0;
}

int acpi_sci_enable_set(bool en) {
    sci_en = en;
    return 0;
}

int acpi_sci_enable_get(bool *en) {
    if (en == NULL) {
        return -EINVAL;
    }

    *en = sci_en;

    return 0;
}

int acpi_sci_put(sci_t sci) {
    int ret;

    if (sci_en == false) {
        // Drop data
        return 0;
    }

    ret = k_msgq_put(&sci_queue, &sci, K_NO_WAIT);
    if (ret < 0) {
        LOG_ERR("Put SCI fail: %d", ret);
        return ret;
    }

    k_event_post(&event, ACPI_SCI);

    return ret;
}

int acpi_sci_get(sci_t * psci) {
    if (psci == NULL) {
        return -EINVAL;
    }

    *psci = sci_buf;
    sci_buf = SCI_NONE;
    k_event_post(&event, ACPI_SCI);

    return 0;
}

static int acpi_cmd_hdl(void) {
    int ret = 0;
    uint8_t len = 0;
    acpi_evt_t event;

    ret = k_msgq_get(&acpi_evt_queue, &event, K_NO_WAIT);
    if (ret < 0)
        return 0;

    // Copy ACPI CMD from interface buffer to local variable
    len = event.len;
    memcpy(rece_cmd, event.pdata, len);

    // Parse the cmd and call corresponding handler function, then copy response
    // to interface buffer
    size_t i = 0;
    for (i = 0; i < ARRAY_SIZE(acpi_cmd_tbl); i++) {
        const acpi_cmd_t *cmd_info = &acpi_cmd_tbl[i];

        if (rece_cmd[0] == cmd_info->cmd) {
            if (cmd_info->cmd_hdl != NULL) {
                acpi_cmd_hdl_t cmd_hdl = cmd_info->cmd_hdl;
                
                ret = cmd_hdl(cmd_info, rece_cmd, len, resp_buf,
                              sizeof(resp_buf));
                if (ret < 0) {
                    LOG_ERR("Failed to handle ACPI cmd 0x%02x: %d", rece_cmd[0],
                            ret);
                } else {
                    LOG_INF("Handled ACPI cmd 0x%02x successfully",
                            rece_cmd[0]);
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

    return ret;
}

static int acpi_sci_hdl(void) {
    int ret = 0;
    sci_t sci;

    // TODO: Check sci_en status for drop data
    if (sci_en == false) {
        // TODO: Drop all data in queue
        return 0;
    }

    // TODO: Check power status for drop data

    // TODO: Check exist/timeout for data retry/drop
    if (sci_buf != SCI_NONE) {
        // TODO: Retry or Drop
    }

    // Get SCI from queue
    ret = k_msgq_get(&sci_queue, &sci, K_NO_WAIT);
    if (ret < 0)
        return 0;

    // Put SCI data to buffer
    sci_buf = sci;

    // TODO: Raise ACPI alert pin (Pulse?? Level??)
    
    // TODO: Set up timeout

    return 0;
}

static void service(void) {
    uint32_t evt = 0;

    while (1) {

        // Wait for event (ACPI cmd or SCI)
        evt = k_event_wait(&event, (ACPI_CMD | ACPI_SCI), true, K_FOREVER);

        if (evt & ACPI_CMD) {
            acpi_cmd_hdl();
        }

        if (evt & ACPI_SCI) {
            acpi_sci_hdl();
        }
    }
}

K_THREAD_DEFINE(acpi_id, APP_STACK_MIN, service, NULL, NULL, NULL, APP_PRIO_M,
                0, 0);

#ifdef CONFIG_ACPI_SHELL
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

static int cmd_acpi_sci_en(const struct shell *sh, size_t argc, char **argv) {
    bool en = (bool)strtoul(argv[1], NULL, 0);
    acpi_sci_enable_set(en);
    shell_info(sh, "SCI functionality %s", en ? "enabled" : "disabled");
    return 0;
}

static int cmd_acpi_sci_put(const struct shell *sh, size_t argc, char **argv) {
    sci_t sci = (sci_t)strtoul(argv[1], NULL, 16);
    int ret = acpi_sci_put(sci);
    if (ret < 0) {
        shell_error(sh, "Failed to put SCI: %d", ret);
    } else {
        shell_info(sh, "SCI 0x%02x put into queue", sci);
    }
    return ret;
}

static int cmd_acpi_sci_get(const struct shell *sh, size_t argc, char **argv) {
    sci_t sci;
    bool en;
    acpi_sci_enable_get(&en);
    // Note: acpi_sci_get will clear the current sci_buf
    acpi_sci_get(&sci);
    shell_info(sh, "SCI Enable Status: %d", en);
    shell_info(sh, "Retrieved SCI Value: 0x%02x", sci);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_sci,
    SHELL_CMD_ARG(en, NULL, "Enable or disable SCI notifications <0|1>", cmd_acpi_sci_en, 2, 0),
    SHELL_CMD_ARG(put, NULL, "Queue a System Control Interrupt (SCI) event <hex_val>", cmd_acpi_sci_put, 2, 0),
    SHELL_CMD_ARG(get, NULL, "Read and clear the pending SCI event", cmd_acpi_sci_get, 1, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sci, &sub_sci, "SCI control commands", NULL);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_acpi,
	SHELL_CMD_ARG(write, NULL,
		"Write ACPI command and optional data bytes (hex)", cmd_acpi_write, 2, 64),
	SHELL_CMD_ARG(read, NULL,
		"Read the last ACPI response buffer", cmd_acpi_read, 1, 0),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(acpi, &sub_acpi, "ACPI commands", NULL);
#endif
