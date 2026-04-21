/*
 * @Author: andy.chang 
 * @Date: 2026-04-21 22:35:31 
 * @Last Modified by:   andy.chang 
 * @Last Modified time: 2026-04-21 22:35:31 
 */

#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <interface/system.h>
#include "soccp_handler.h"
#include <interface/soccp.h>

LOG_MODULE_REGISTER(soccp, LOG_LEVEL_DBG);

typedef struct {
    uint8_t cmd;
    int (*cmd_hdl)(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp,
                   uint8_t resp_len);
} soccp_cmd_t;

static const soccp_cmd_t soccp_cmd_tbl[] = {
    // clang-format off
    {SOCCP_CMD_WHO_AM_I,            soccp_who_am_i},
    {SOCCP_CMD_POWER_STATE_FEATURE, soccp_power_state_ctrl},
    // clang-format on
};

static uint8_t rece_cmd[SOCCP_RECE_LEN];
static uint8_t resp_buf[SOCCP_RESP_LEN];
static K_EVENT_DEFINE(event);

typedef struct soccp_evt_t {
    uint8_t *pdata;
    uint16_t len;
} soccp_evt_t;

#define SOCCP_EVT_CMD BIT(LOCAL_EVT_START)
#define SOCCP_EVT_LEN 4

K_MSGQ_DEFINE(soccp_evt_queue, sizeof(soccp_evt_t), SOCCP_EVT_LEN, 4);

int soccp_write(uint8_t *data, uint16_t len) {
    int ret;
    soccp_evt_t evt = {.pdata = data, .len = len};

    ret = k_msgq_put(&soccp_evt_queue, &evt, K_NO_WAIT);
    if (ret < 0) {
        LOG_ERR("Put SoCCP CMD fail: %d", ret);
        return ret;
    }

    k_event_post(&event, SOCCP_EVT_CMD);
    return 0;
}

int soccp_read(uint8_t *data, uint16_t len) {
    uint16_t max = MIN(len, sizeof(resp_buf));

    if (data == NULL) {
        return -ENOMEM;
    }

    memcpy(data, resp_buf, max);
    return 0;
}

static int soccp_cmd_dispatcher(void) {
    int ret;
    soccp_evt_t event;

    ret = k_msgq_get(&soccp_evt_queue, &event, K_NO_WAIT);
    if (ret < 0)
        return 0;

    uint16_t len = MIN(event.len, sizeof(rece_cmd));
    memcpy(rece_cmd, event.pdata, len);

    if (len < 1)
        return;

    uint8_t cmd_id = rece_cmd[0];
    size_t i;

    for (i = 0; i < ARRAY_SIZE(soccp_cmd_tbl); i++) {
        if (cmd_id == soccp_cmd_tbl[i].cmd) {
            if (soccp_cmd_tbl[i].cmd_hdl) {
                ret = soccp_cmd_tbl[i].cmd_hdl(&rece_cmd[1], len - 1, resp_buf,
                                               sizeof(resp_buf));
                if (ret < 0) {
                    LOG_ERR("SoCCP Cmd 0x%02x handle failed: %d", cmd_id, ret);
                } else {
                    LOG_INF("SoCCP Cmd 0x%02x executed", cmd_id);
                }
            }
            break;
        }
    }

    if (i == ARRAY_SIZE(soccp_cmd_tbl)) {
        LOG_WRN("Unknown SoCCP command: 0x%02x", cmd_id);
    }
    return 0;
}

static void service(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {
        uint32_t evt = k_event_wait(&event, SOCCP_EVT_CMD, true, K_FOREVER);
        if (evt & SOCCP_EVT_CMD) {
            soccp_cmd_dispatcher();
        }
    }
}

K_THREAD_DEFINE(soccp_id, APP_STACK_MIN, service, NULL, NULL, NULL, APP_PRIO_M,
                0, 0);

#ifdef CONFIG_SHELL
#include <zephyr/shell/shell.h>

static int cmd_soccp_write(const struct shell *sh, size_t argc, char **argv) {
    int ret = 0;
    uint8_t buff[64];
    uint16_t max = MIN(argc - 1, sizeof(buff));

    for (size_t i = 0; i < max; i++) {
        buff[i] = strtoul(argv[i + 1], NULL, 16);
    }

    ret = soccp_write(buff, max);
    if (ret < 0) {
        shell_error(sh, "Failed to write SoCCP CMD: %d", ret);
    } else {
        shell_info(sh, "SoCCP CMD written successfully");
    }

    return ret;
}

static int cmd_soccp_read(const struct shell *sh, size_t argc, char **argv) {
    int ret = 0;
    uint8_t buff[64];

    ret = soccp_read(buff, sizeof(buff));
    if (ret < 0) {
        shell_error(sh, "Failed to read SoCCP CMD: %d", ret);
        return ret;
    }

    shell_print(sh, "SoCCP Read Response:");
    shell_hexdump(sh, buff, sizeof(buff));

    return ret;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_soccp,
	SHELL_CMD_ARG(write, NULL,
		"Write SoCCP command and optional data bytes (hex)", cmd_soccp_write, 2, 64),
	SHELL_CMD_ARG(read, NULL,
		"Read the last SoCCP response buffer", cmd_soccp_read, 1, 0),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(soccp, &sub_soccp, "SoCCP commands", NULL);
#endif
