#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <interface/acpi.h>
#include <interface/system.h>

#include "acpi_tbl.h"

LOG_MODULE_REGISTER(acpi, CONFIG_ACPI_LOG_LEVEL);

#define ACPI_CMD BIT(LOCAL_EVT_START)

static K_EVENT_DEFINE(event);

static uint8_t cmd_idx;
static uint8_t rece_cmd[ACPI_RECE_LEN];
static uint8_t resp_buf[ACPI_RESP_LEN];

int acpi_buf_set(acpi_type_t id, uint8_t data)
{
    static uint8_t mand;
    static bool dispatched;

    if (cmd_idx >= sizeof(rece_cmd)) {
        LOG_WRN("cmd_idx: %d", cmd_idx);
        cmd_idx = 0;
    }

    switch (id) {
    case ACPI_TYPE_CMD:
        cmd_idx = 0;
        mand = 0;
        dispatched = false;
        rece_cmd[0] = data;

        STRUCT_SECTION_FOREACH(acpi_cmd_t, cmd) {
            if (rece_cmd[0] == cmd->cmd) {
                mand = cmd->mand;
                break;
            }
        }
        __fallthrough;

    case ACPI_TYPE_DATA:
        rece_cmd[cmd_idx++] = data;

        if (cmd_idx == mand) {
            dispatched = true;
            k_event_post(&event, ACPI_CMD);
        } else if (cmd_idx > mand) {
            dispatched = false;
        }
        break;

    case ACPI_TYPE_PROCESS:
        if (!dispatched) {
            k_event_post(&event, ACPI_CMD);
        }
        break;

    default:
        LOG_WRN("Unknown ID: %d", id);
        break;
    }

    return 0;
}

static int acpi_cmd_hdl(void)
{
    int ret = 0;

    STRUCT_SECTION_FOREACH(acpi_cmd_t, cmd) {
        if (rece_cmd[0] != cmd->cmd) {
            continue;
        }

        if ((cmd_idx >= cmd->mand) && (cmd->cmd_hdl != NULL)) {
            ret = cmd->cmd_hdl(cmd, rece_cmd, cmd_idx, resp_buf,
                               sizeof(resp_buf));
            if (ret < 0) {
                LOG_ERR("Failed to handle ACPI cmd 0x%02x: %d", rece_cmd[0],
                        ret);
            } else {
                LOG_INF("Handled ACPI cmd 0x%02x successfully", rece_cmd[0]);
                acpi_resp_set(resp_buf, cmd->resp_len);
            }
        } else if (cmd->cmd_hdl == NULL) {
            LOG_WRN("No handler for ACPI cmd 0x%02x", rece_cmd[0]);
        }
        return ret;
    }

    LOG_WRN("Unknown ACPI cmd 0x%02x", rece_cmd[0]);
    return -EINVAL;
}

static void service(void)
{
    while (true) {
        uint32_t evt = k_event_wait(&event, ACPI_CMD, false, K_FOREVER);
        if (evt & ACPI_CMD) {
            k_event_clear(&event, ACPI_CMD);
            acpi_cmd_hdl();
        }
    }
}

#if defined(CONFIG_EC_BOOTLOADER) && CONFIG_EC_BOOTLOADER
K_THREAD_DEFINE(acpi_id, 512, service, NULL, NULL, NULL, 5, 0, 0);
#else
K_THREAD_DEFINE(acpi_id, APP_STACK_MIN, service, NULL, NULL, NULL, APP_PRIO_M,
                0, 0);
#endif

#ifdef CONFIG_ACPI_SHELL
#include <zephyr/shell/shell.h>

static int cmd_acpi_write(const struct shell *sh, size_t argc, char **argv)
{
    int ret = 0;

    for (size_t i = 0; i < argc - 1; i++) {
        uint8_t byte = (uint8_t)strtoul(argv[i + 1], NULL, 16);
        acpi_buf_set(i == 0 ? ACPI_TYPE_CMD : ACPI_TYPE_DATA, byte);
    }

    acpi_buf_set(ACPI_TYPE_PROCESS, 0);
    if (ret < 0) {
        shell_error(sh, "Failed to write ACPI CMD: %d", ret);
    } else {
        shell_info(sh, "ACPI CMD written successfully");
    }
    return ret;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_acpi,
    SHELL_CMD_ARG(write, NULL, "Write ACPI command and optional data (hex)",
                  cmd_acpi_write, 2, 64),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(acpi, &sub_acpi, "ACPI commands", NULL);
#endif
