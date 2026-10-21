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
};

#define ACPI_CMD BIT(ACPI_EVT_CMD)

static K_EVENT_DEFINE(event);

static uint8_t cmd_idx = 0;
static uint8_t rece_cmd[ACPI_RECE_LEN];
static uint8_t resp_buf[ACPI_RESP_LEN];

#define ACPI_EVT_LEN 2

typedef struct acpi_evt_t {
    uint8_t *pdata;
    uint16_t len;
} acpi_evt_t ;

K_MSGQ_DEFINE(acpi_evt_queue, sizeof(acpi_evt_t), ACPI_EVT_LEN, 4);

int acpi_buf_set(acpi_type_t id, uint8_t data) {
    static uint8_t mand = 0;
    // Whether the mandatory argument dispatch already handled the command as
    // it currently stands, so the end of transfer does not run it a second time
    static bool dispatched = false;

    if (cmd_idx >= sizeof(rece_cmd)) {
        LOG_WRN("cmd_idx: %d", cmd_idx);
        cmd_idx = 0;
    }

    switch (id) {
    case ACPI_TYPE_CMD:
        // Recieve cmd
        cmd_idx = 0;
        mand = 0;
        dispatched = false;

        // Store the opcode before the lookup, otherwise the previous command
        // opcode is the one being searched for
        rece_cmd[0] = data;

        // Search for the mandatory argument count for the command
        STRUCT_SECTION_FOREACH(acpi_cmd_t, p) {
            if (rece_cmd[0] == p->cmd) {
                mand = p->mand;
                break;
            }
        }
        __fallthrough;

    case ACPI_TYPE_DATA:
        rece_cmd[cmd_idx] = data;
        cmd_idx++;

        if (cmd_idx == mand) {
            // Mandatory arguments complete, handle it now so the response is
            // ready even when the host reads without ending the transfer
            dispatched = true;
            k_event_post(&event, ACPI_CMD);
        } else if (cmd_idx > mand) {
            // Optional payload arrived after that dispatch, the handler has to
            // run again to see the complete command
            dispatched = false;
        }
        break;

    case ACPI_TYPE_PROCESS:
        // End of transfer, only needed when the dispatch above did not already
        // handle the command as received
        if (dispatched == false) {
            k_event_post(&event, ACPI_CMD);
        }
        break;

    default:
        LOG_WRN("Unknow ID: %d", id);
        break;
    }

    return 0;
}

static int acpi_cmd_hdl(void) {
    int ret = 0;

    // Parse the cmd and call corresponding handler function, then copy response
    // to interface buffer
    STRUCT_SECTION_FOREACH(acpi_cmd_t, p) {
        if (rece_cmd[0] == p->cmd) {
            if ((cmd_idx >= p->mand) && (p->cmd_hdl != NULL)) {
                acpi_cmd_hdl_t cmd_hdl = p->cmd_hdl;

                ret = cmd_hdl(p, rece_cmd, cmd_idx, resp_buf, sizeof(resp_buf));
                if (ret < 0) {
                    LOG_ERR("Failed to handle ACPI cmd 0x%02x: %d", rece_cmd[0],
                            ret);
                } else {
                    acpi_resp_set(resp_buf, p->resp_len);
                    LOG_DBG("Handled ACPI cmd 0x%02x successfully",
                            rece_cmd[0]);
                }
            } else if (p->cmd_hdl == NULL) {
                LOG_WRN("No handler for ACPI cmd 0x%02x", rece_cmd[0]);
            }
            return ret;
        }
    }

    LOG_WRN("Unknown ACPI cmd 0x%02x", rece_cmd[0]);

    return -EINVAL;
}

static void service(void) {
    uint32_t evt = 0;

    while (1) {
        evt = k_event_wait(&event, ACPI_CMD, false, K_FOREVER);

        if (evt & ACPI_CMD) {
            k_event_clear(&event, ACPI_CMD);
            acpi_cmd_hdl();
        }
    }
}

K_THREAD_DEFINE(acpi_id, APP_STACK_MIN, service, NULL, NULL, NULL, APP_PRIO_H,
                0, 0);

#ifdef CONFIG_ACPI_SHELL
#include <zephyr/shell/shell.h>

static int cmd_acpi_write(const struct shell *sh, size_t argc, char **argv) {
    int ret = 0;
    uint8_t buff;

    for (size_t i = 0; i < argc-1; i++) {
        buff = strtoul(argv[i + 1], NULL, 16);

        if (i == 0) {
            acpi_buf_set(ACPI_TYPE_CMD, buff);
        } else {
            acpi_buf_set(ACPI_TYPE_DATA, buff);
        }
    }

    // Align with the I2C path, where the stop condition ends the transfer and
    // triggers the handler with the whole payload received
    acpi_buf_set(ACPI_TYPE_PROCESS, 0);

    if (ret < 0) {
        shell_error(sh, "Failed to write ACPI CMD: %d", ret);
    } else {
        shell_info(sh, "ACPI CMD written successfully");
    }
    
    return ret;
}

// static int cmd_acpi_read(const struct shell *sh, size_t argc, char **argv) {
//     int ret = 0;
//     uint8_t buff[64];

//     ret = acpi_read(buff, sizeof(buff));
//     if (ret < 0) {
//         shell_error(sh, "Failed to read ACPI CMD: %d", ret);
//         return ret;
//     }

//     shell_print(sh, "ACPI Read Response:");
//     shell_hexdump(sh, buff, sizeof(buff));

//     return ret;
// }

SHELL_STATIC_SUBCMD_SET_CREATE(sub_acpi,
	SHELL_CMD_ARG(write, NULL,
		"Write ACPI command and optional data bytes (hex)", cmd_acpi_write, 2, 64),
	// SHELL_CMD_ARG(read, NULL,
	// 	"Read the last ACPI response buffer", cmd_acpi_read, 1, 0),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(acpi, &sub_acpi, "ACPI commands", NULL);
#endif
