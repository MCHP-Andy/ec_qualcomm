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
#include <interface/power.h>

#include "acpi_tbl.h"

LOG_MODULE_REGISTER(acpi, CONFIG_ACPI_LOG_LEVEL);

enum {
    ACPI_EVT_CMD = LOCAL_EVT_START,
    ACPI_EVT_SCI,
    ACPI_EVT_SCI_TMO,
};

#define ACPI_CMD BIT(ACPI_EVT_CMD)
#define ACPI_SCI BIT(ACPI_EVT_SCI)
#define ACPI_SCI_TMO BIT(ACPI_EVT_SCI_TMO)

static K_EVENT_DEFINE(event);
SYS_EVENT_SUBSCRIBE(acpi, event);

static uint8_t cmd_idx = 0;
static uint8_t rece_cmd[ACPI_RECE_LEN];
static uint8_t resp_buf[ACPI_RESP_LEN];
static sci_t sci_buf;
static bool sci_en = true;

#define ACPI_EVT_LEN 2
#define SCI_LEN CONFIG_ACPI_SCI_QUEUE_LEN

typedef struct acpi_evt_t {
    uint8_t *pdata;
    uint16_t len;
} acpi_evt_t ;

K_MSGQ_DEFINE(acpi_evt_queue, sizeof(acpi_evt_t), ACPI_EVT_LEN, 4);
K_MSGQ_DEFINE(sci_queue, sizeof(sci_t), SCI_LEN, 4);

static void sci_tmr_hdl(struct k_timer *tmr) {
    ARG_UNUSED(tmr);

    // Defer the drop decision to the service thread
    k_event_post(&event, ACPI_SCI_TMO);
}

K_TIMER_DEFINE(sci_tmr, sci_tmr_hdl, NULL);

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

int acpi_sci_enable_set(bool en) {
    sci_en = en;

    // Let the handler flush the queue when disabled, or resume when enabled
    k_event_post(&event, ACPI_SCI);

    return 0;
}

int acpi_sci_enable_get(bool *en) {
    if (en == NULL) {
        return -EINVAL;
    }

    *en = sci_en;

    return 0;
}

/*
 * @brief Check whether the host is able to service an SCI in current power
 * state.
 */
static bool sci_pwr_allow(void) {
    pwr_sta_t state = PWR_STA_G3;

    if (pwr_state_get(&state) < 0) {
        return false;
    }

    // Only a running or modern standby host can read the notification back
    return ((state == PWR_STA_S0) || (state == PWR_STA_MS));
}

/*
 * @brief Drop the pending SCI and every queued one.
 */
static void sci_flush(void) {
    uint32_t cnt = k_msgq_num_used_get(&sci_queue);

    k_timer_stop(&sci_tmr);
    k_msgq_purge(&sci_queue);

    if ((sci_buf != SCI_NONE) || (cnt != 0)) {
        LOG_WRN("Drop SCI 0x%02x and %u queued event(s)", sci_buf, cnt);
    }

    sci_buf = SCI_NONE;
}

int sci_enque(sci_t sci) {
    int ret;

    if (sci == SCI_NONE) {
        LOG_ERR("Invalid SCI: 0x%02x", sci);
        return -EINVAL;
    }

    if (sci_en == false) {
        // Notification disabled by host (cmd 0x35 bit 0), drop data
        LOG_DBG("SCI 0x%02x dropped: notification disabled", sci);
        return 0;
    }

    if (sci_pwr_allow() == false) {
        // Host cannot service the event in this power state, drop data
        LOG_DBG("SCI 0x%02x dropped: power state", sci);
        return 0;
    }

    ret = k_msgq_put(&sci_queue, &sci, K_NO_WAIT);
    if (ret < 0) {
        LOG_ERR("Put SCI 0x%02x fail: %d", sci, ret);
        return -ENOMSG;
    }

    LOG_DBG("SCI 0x%02x queued (%u pending)", sci,
            k_msgq_num_used_get(&sci_queue));

    k_event_post(&event, ACPI_SCI);

    return 0;
}

static int acpi_sci_get(sci_t * psci) {
    if (psci == NULL) {
        return -EINVAL;
    }

    // Host has served the notification, stop the timeout
    k_timer_stop(&sci_tmr);

    *psci = sci_buf;
    sci_buf = SCI_NONE;

    if (*psci == SCI_NONE) {
        LOG_WRN("Host read SCI while none pending");
    }

    // Let the next queued event take over
    k_event_post(&event, ACPI_SCI);

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
                    LOG_INF("Handled ACPI cmd 0x%02x successfully",
                            rece_cmd[0]);
                    acpi_resp_set(resp_buf, p->resp_len);
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

/*
 * @brief Move the next queued SCI into the host readable buffer and notify the
 * host with an interrupt pulse.
 */
static int acpi_sci_hdl(void) {
    int ret = 0;
    sci_t sci;

    // Check sci_en status for drop data
    if (sci_en == false) {
        // Drop all data in queue
        sci_flush();
        return 0;
    }

    // Check power status for drop data
    if (sci_pwr_allow() == false) {
        sci_flush();
        return 0;
    }

    // Previous event is still waiting to be read, its timeout decides retry/drop
    if (sci_buf != SCI_NONE) {
        LOG_DBG("SCI 0x%02x still pending", sci_buf);
        return 0;
    }

    // Get SCI from queue
    ret = k_msgq_get(&sci_queue, &sci, K_NO_WAIT);
    if (ret < 0) {
        // Nothing left to notify
        return 0;
    }

    // Put SCI data to buffer
    sci_buf = sci;

    // Pulse the ACPI interrupt pin, the line is not held asserted while
    // waiting for the host to read cmd 0x05
    acpi_int_pulse();

    // Set up timeout, host has to read the event before it expires
    k_timer_start(&sci_tmr, K_MSEC(CONFIG_ACPI_SCI_TIMEOUT_MS), K_NO_WAIT);

    LOG_INF("Notify SCI 0x%02x (%u queued)", sci_buf,
            k_msgq_num_used_get(&sci_queue));

    return 0;
}

/*
 * @brief Host did not read the notification in time, drop it and check whether
 * a next one takes over.
 */
static int acpi_sci_tmo_hdl(void) {
    if (sci_buf == SCI_NONE) {
        // Host read it just before the timeout expired
        return 0;
    }

    LOG_WRN("SCI 0x%02x timeout, dropped", sci_buf);

    sci_buf = SCI_NONE;

    // Check whether a next event exists to take over
    return acpi_sci_hdl();
}

static void service(void) {
    uint32_t evt = 0;

    while (1) {

        // Wait for event (ACPI cmd, SCI, SCI timeout or power state change)
        evt = k_event_wait(&event,
                           (ACPI_CMD | ACPI_SCI | ACPI_SCI_TMO |
                            SYS_PWR_STA_CHG),
                           false, K_FOREVER);

        if (evt & ACPI_CMD) {
            k_event_clear(&event, ACPI_CMD);
            acpi_cmd_hdl();
        }

        if (evt & ACPI_SCI_TMO) {
            k_event_clear(&event, ACPI_SCI_TMO);
            acpi_sci_tmo_hdl();
        }

        if (evt & (ACPI_SCI | SYS_PWR_STA_CHG)) {
            // A power state change either flushes the pending events or lets
            // them resume, both handled by the SCI handler
            k_event_clear(&event, (ACPI_SCI | SYS_PWR_STA_CHG));
            acpi_sci_hdl();
        }
    }
}

K_THREAD_DEFINE(acpi_id, APP_STACK_MIN, service, NULL, NULL, NULL, APP_PRIO_M,
                0, 0);

/*
 * @brief Host reads back the notified SCI event.
 */
static int acpi_active_cooling_sci_event(const acpi_cmd_t *cmd_info,
                                         uint8_t *cmd, uint16_t cmd_len,
                                         uint8_t *resp, uint16_t resp_len) {
    sci_t sci;
    ARG_UNUSED(cmd);
    ARG_UNUSED(cmd_len);
    ACPI_CHECK_OUT(cmd_info, resp, resp_len);

    acpi_sci_get(&sci);
    resp[0] = sci;
    return 0;
}

// clang-format off
ACPI_CMD_SUBSCRIBE(sci, EC_ACTIVE_COOLING_SCI_EVENT,               acpi_active_cooling_sci_event, 1, 0, 1); // Page 37
// clang-format on

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

static int cmd_acpi_sci_en(const struct shell *sh, size_t argc, char **argv) {
    bool en = (bool)strtoul(argv[1], NULL, 0);
    acpi_sci_enable_set(en);
    shell_info(sh, "SCI functionality %s", en ? "enabled" : "disabled");
    return 0;
}

static int cmd_acpi_sci_put(const struct shell *sh, size_t argc, char **argv) {
    sci_t sci = (sci_t)strtoul(argv[1], NULL, 16);
    int ret = sci_enque(sci);
    if (ret < 0) {
        shell_error(sh, "Failed to put SCI: %d", ret);
    } else {
        // Note: an accepted call may still drop the event on purpose,
        // the reported pending/queued state shows what really happened
        shell_info(sh, "SCI 0x%02x submitted (pending 0x%02x, %u queued)", sci,
                   sci_buf, k_msgq_num_used_get(&sci_queue));
    }
    return ret;
}

static int cmd_acpi_sci_sta(const struct shell *sh, size_t argc, char **argv) {
    pwr_sta_t state = 0;

    pwr_state_get(&state);

    shell_print(sh, "SCI enable  : %d", sci_en);
    shell_print(sh, "Power state : %d (allow: %d)", state, sci_pwr_allow());
    shell_print(sh, "Pending SCI : 0x%02x", sci_buf);
    shell_print(sh, "Queued      : %u/%u", k_msgq_num_used_get(&sci_queue),
                SCI_LEN);
    shell_print(sh, "Timeout     : %d ms", CONFIG_ACPI_SCI_TIMEOUT_MS);

    return 0;
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
    SHELL_CMD_ARG(sta, NULL, "Show SCI enable/power/queue status", cmd_acpi_sci_sta, 1, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sci, &sub_sci, "SCI control commands", NULL);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_acpi,
	SHELL_CMD_ARG(write, NULL,
		"Write ACPI command and optional data bytes (hex)", cmd_acpi_write, 2, 64),
	// SHELL_CMD_ARG(read, NULL,
	// 	"Read the last ACPI response buffer", cmd_acpi_read, 1, 0),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(acpi, &sub_acpi, "ACPI commands", NULL);
#endif
