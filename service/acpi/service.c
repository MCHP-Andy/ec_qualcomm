/*
 * @Author: andy.chang 
 * @Date: 2025-07-01 02:46:45 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-23 23:17:00
 */

#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <interface/system.h>
#include <interface/acpi.h>

#include "acpi_tbl.h"

LOG_MODULE_REGISTER(acpi, CONFIG_ACPI_LOG_LEVEL);

// Number of commands that can wait for the service thread
#define ACPI_CMD_QUEUE_LEN 2

// One complete command from the host: opcode followed by its arguments
typedef struct acpi_cmd_pkt_t {
    uint16_t len;
    uint8_t data[ACPI_RECE_LEN];
} acpi_cmd_pkt_t;

// Packets live in the slab, only their pointers go through the queue, so the
// I2C ISR does not copy a whole packet onto its stack
K_MEM_SLAB_DEFINE_STATIC(acpi_cmd_slab, sizeof(acpi_cmd_pkt_t),
                         ACPI_CMD_QUEUE_LEN, 4);
K_MSGQ_DEFINE(acpi_cmd_queue, sizeof(acpi_cmd_pkt_t *), ACPI_CMD_QUEUE_LEN, 4);

static uint8_t resp_buf[ACPI_RESP_LEN];

int acpi_cmd_submit(const uint8_t *pdata, uint16_t len) {
    acpi_cmd_pkt_t *pkt;

    if ((pdata == NULL) || (len == 0) || (len > ACPI_RECE_LEN)) {
        return -EINVAL;
    }

    if (k_mem_slab_alloc(&acpi_cmd_slab, (void **)&pkt, K_NO_WAIT) != 0) {
        LOG_WRN("ACPI cmd queue full, drop cmd 0x%02x", pdata[0]);
        return -ENOMEM;
    }

    pkt->len = len;
    memcpy(pkt->data, pdata, len);

    // Cannot fail: the queue holds as many entries as the slab has blocks
    (void)k_msgq_put(&acpi_cmd_queue, &pkt, K_NO_WAIT);

    return 0;
}

static int acpi_cmd_hdl(uint8_t *cmd, uint16_t cmd_len) {
    int ret = 0;

    // Parse the cmd and call corresponding handler function, then copy response
    // to interface buffer
    STRUCT_SECTION_FOREACH(acpi_cmd_t, p) {
        if (cmd[0] == p->cmd) {
            if ((cmd_len >= p->mand) && (p->cmd_hdl != NULL)) {
                acpi_cmd_hdl_t cmd_hdl = p->cmd_hdl;

                // >0: read, response length / 0: write / <0: error
                ret = cmd_hdl(p, cmd, cmd_len, resp_buf, sizeof(resp_buf));
                if (ret < 0) {
                    LOG_ERR("Failed to handle ACPI cmd 0x%02x: %d", cmd[0],
                            ret);
                } else if (ret > sizeof(resp_buf)) {
                    LOG_ERR("ACPI cmd 0x%02x response too long: %d", cmd[0],
                            ret);
                    ret = -EOVERFLOW;
                } else {
                    if (ret > 0) {
                        acpi_resp_set(resp_buf, ret);
                    }
                    LOG_DBG("Handled ACPI cmd 0x%02x successfully, resp %d",
                            cmd[0], ret);
                }
            } else if (p->cmd_hdl == NULL) {
                LOG_WRN("No handler for ACPI cmd 0x%02x", cmd[0]);
            } else {
                LOG_WRN("ACPI cmd 0x%02x too short: %u < %u", cmd[0], cmd_len,
                        p->mand);
            }
            return ret;
        }
    }

    LOG_WRN("Unknown ACPI cmd 0x%02x", cmd[0]);

    return -EINVAL;
}

static void service(void) {
    acpi_cmd_pkt_t *pkt;

    while (1) {
        k_msgq_get(&acpi_cmd_queue, &pkt, K_FOREVER);

        acpi_cmd_hdl(pkt->data, pkt->len);

        k_mem_slab_free(&acpi_cmd_slab, (void *)pkt);
    }
}

K_THREAD_DEFINE(acpi_id, APP_STACK_MIN, service, NULL, NULL, NULL, APP_PRIO_H,
                0, 0);

#ifdef CONFIG_ACPI_SHELL
#include <zephyr/shell/shell.h>

static int cmd_acpi_write(const struct shell *sh, size_t argc, char **argv) {
    int ret = 0;
    uint8_t buff[64];
    uint16_t len = 0;

    for (size_t i = 1; (i < argc) && (len < sizeof(buff)); i++) {
        buff[len++] = strtoul(argv[i], NULL, 16);
    }

    // Same as the I2C path: the whole write is submitted as one command
    ret = acpi_cmd_submit(buff, len);

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
