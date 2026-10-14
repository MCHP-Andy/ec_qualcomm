/*
 * @Author: andy.chang 
 * @Date: 2026-04-21 22:35:31 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-23 00:11:53
 */

#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <interface/system.h>
#include "soccp_handler.h"
#include <interface/soccp.h>

LOG_MODULE_REGISTER(soccp, CONFIG_SOCCP_LOG_LEVEL);

static const soccp_cmd_t soccp_cmd_tbl[] = {
    // clang-format off
    {SOCCP_CMD_WHO_AM_I,            soccp_who_am_i, 1, 0, 1},
    {SOCCP_CMD_POWER_STATE_FEATURE, soccp_power_state_ctrl, 4, 0, 0},
    // clang-format on
};

static uint8_t cmd_idx = 0;
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

int soccp_buf_set(soccp_type_t id, uint8_t data) {

    if (cmd_idx >= sizeof(rece_cmd)) {
        LOG_WRN("cmd_idx: %d", cmd_idx);
        cmd_idx = 0;
    }

    switch (id) {
    case SOCCP_TYPE_CMD:
        // Recieve cmd
        cmd_idx = 0;
        __fallthrough;

    case SOCCP_TYPE_DATA:
        rece_cmd[cmd_idx] = data;
        cmd_idx++;

        k_event_post(&event, SOCCP_EVT_CMD);
        break;

    default:
        LOG_WRN("Unknow ID: %d", id);
        break;
    }

    return 0;
}

static int soccp_cmd_dispatcher(void) {
    int ret;

    // Parse the cmd and call corresponding handler function, then copy response
    // to interface buffer
    size_t i = 0;
    for (i = 0; i < ARRAY_SIZE(soccp_cmd_tbl); i++) {
        const soccp_cmd_t *cmd_info = &soccp_cmd_tbl[i];

        if (rece_cmd[0] == cmd_info->cmd) {
            if (((cmd_idx == cmd_info->mand) || (cmd_idx == cmd_info->mand + cmd_info->opt)) &&
                (cmd_info->cmd_hdl != NULL)) {
                soccp_cmd_hdl_t cmd_hdl = cmd_info->cmd_hdl;

                ret = cmd_hdl(cmd_info, rece_cmd, cmd_idx, resp_buf,
                              sizeof(resp_buf));
                if (ret < 0) {
                    LOG_ERR("Failed to handle SoCCP cmd 0x%02x: %d", rece_cmd[0],
                            ret);
                } else {
                    LOG_INF("Handled SoCCP cmd 0x%02x successfully",
                            rece_cmd[0]);
                    /* Write-only commands have no response, leave the
                     * interface buffer untouched */
                    if (cmd_info->resp_len) {
                        soccp_resp_set(resp_buf, cmd_info->resp_len);
                    }
                }
            } else if (cmd_info->cmd_hdl == NULL) {
                LOG_WRN("No handler for SoCCP cmd 0x%02x", rece_cmd[0]);
            }
            break;
        }
    }

    if (i == ARRAY_SIZE(soccp_cmd_tbl)) {
        LOG_WRN("Unknown SoCCP command: 0x%02x", rece_cmd[0]);
    }

    return 0;
}

static void service(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {
        uint32_t evt = k_event_wait(&event, SOCCP_EVT_CMD, false, K_FOREVER);

        if (evt & SOCCP_EVT_CMD) {
            k_event_clear(&event, SOCCP_EVT_CMD);
            soccp_cmd_dispatcher();
        }
    }
}

K_THREAD_DEFINE(soccp_id, APP_STACK_MIN, service, NULL, NULL, NULL, APP_PRIO_M,
                0, 0);

#ifdef CONFIG_SOCCP_SHELL
#include <zephyr/shell/shell.h>

static int cmd_soccp_write(const struct shell *sh, size_t argc, char **argv) {
    int ret = 0;
    uint8_t buff;

    for (size_t i = 0; i < argc-1; i++) {
        buff = strtoul(argv[i + 1], NULL, 16);

        if (i == 0) {
            soccp_buf_set(SOCCP_TYPE_CMD, buff);
        } else {
            soccp_buf_set(SOCCP_TYPE_DATA, buff);
        }
    }

    if (ret < 0) {
        shell_error(sh, "Failed to write SoCCP CMD: %d", ret);
    } else {
        shell_info(sh, "SoCCP CMD written successfully");
    }

    return ret;
}

// static int cmd_soccp_read(const struct shell *sh, size_t argc, char **argv) {
//     int ret = 0;
//     uint8_t buff[64];

//     ret = soccp_read(buff, sizeof(buff));
//     if (ret < 0) {
//         shell_error(sh, "Failed to read SoCCP CMD: %d", ret);
//         return ret;
//     }

//     shell_print(sh, "SoCCP Read Response:");
//     shell_hexdump(sh, buff, sizeof(buff));

//     return ret;
// }

static int cmd_soccp_state(const struct shell *sh, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    uint16_t status = 0;

    soccp_oob_state_get(&status);

    shell_info(sh, "OffMode/OOB status: 0x%04x", status);
    shell_info(sh, "\tBit 0 SoC on Off-mode : %d",
               !!(status & SOCCP_OOB_STA_OFF_MODE));
    shell_info(sh, "\tBit 1 SoCCP active    : %d",
               !!(status & SOCCP_OOB_STA_ACTIVE));
    shell_info(sh, "\tBit 2 OOB initialized : %d",
               !!(status & SOCCP_OOB_STA_INITED));

    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_soccp,
	SHELL_CMD_ARG(write, NULL,
		"Write SoCCP command and optional data bytes (hex)", cmd_soccp_write, 2, 64),
	SHELL_CMD_ARG(state, NULL,
		"Dump the latest OffMode/OOB status reported by SoC-CP", cmd_soccp_state, 1, 0),
	// SHELL_CMD_ARG(read, NULL,
	// 	"Read the last SoCCP response buffer", cmd_soccp_read, 1, 0),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(soccp, &sub_soccp, "SoCCP commands", NULL);
#endif
