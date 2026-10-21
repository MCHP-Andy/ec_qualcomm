#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <interface/acpi.h>
#include <interface/power.h>
#include <interface/sci.h>
#include <interface/system.h>

#include "../acpi/acpi_tbl.h"

LOG_MODULE_REGISTER(sci, CONFIG_SCI_LOG_LEVEL);

enum {
    SCI_EVT_NOTIFY = LOCAL_EVT_START,
    SCI_EVT_TIMEOUT,
};

#define SCI_NOTIFY BIT(SCI_EVT_NOTIFY)
#define SCI_TIMEOUT BIT(SCI_EVT_TIMEOUT)
#define SCI_LEN CONFIG_ACPI_SCI_QUEUE_LEN

static K_EVENT_DEFINE(event);
SYS_EVENT_SUBSCRIBE(sci, event);

static sci_t sci_buf;
static bool sci_en = true;

K_MSGQ_DEFINE(sci_queue, sizeof(sci_t), SCI_LEN, 4);

static void sci_tmr_hdl(struct k_timer *tmr) {
    ARG_UNUSED(tmr);
    k_event_post(&event, SCI_TIMEOUT);
}

K_TIMER_DEFINE(sci_tmr, sci_tmr_hdl, NULL);

static bool sci_pwr_allow(void) {
    pwr_sta_t state = PWR_STA_G3;

    if (pwr_state_get(&state) < 0) {
        return false;
    }

    return (state == PWR_STA_S0) || (state == PWR_STA_MS);
}

int acpi_sci_enable_set(bool en) {
    sci_en = en;
    k_event_post(&event, SCI_NOTIFY);
    return 0;
}

int acpi_sci_enable_get(bool *en) {
    if (en == NULL) {
        return -EINVAL;
    }

    *en = sci_en;
    return 0;
}

static void sci_flush(void) {
    uint32_t cnt = k_msgq_num_used_get(&sci_queue);

    k_timer_stop(&sci_tmr);
    k_msgq_purge(&sci_queue);

    if ((sci_buf != SCI_NONE) || (cnt != 0)) {
        LOG_WRN("Drop SCI 0x%02x and %u queued event(s)", sci_buf, cnt);
    }

    sci_buf = SCI_NONE;
    if (acpi_int_set(0) < 0) {
        LOG_ERR("Failed to deassert ACPI int pin while flushing SCI");
    }
}

int sci_enque(sci_t sci) {
    int ret;

    if (sci == SCI_NONE) {
        LOG_ERR("Invalid SCI: 0x%02x", sci);
        return -EINVAL;
    }

    if (sci_en == false) {
        LOG_DBG("SCI 0x%02x dropped: notification disabled", sci);
        return 0;
    }

    if (sci_pwr_allow() == false) {
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
    k_event_post(&event, SCI_NOTIFY);
    return 0;
}

static int acpi_sci_get(sci_t *psci) {
    if (psci == NULL) {
        return -EINVAL;
    }

    k_timer_stop(&sci_tmr);
    *psci = sci_buf;
    sci_buf = SCI_NONE;

    if (*psci == SCI_NONE) {
        LOG_WRN("Host read SCI while none pending");
    }

    if (acpi_int_set(0) < 0) {
        LOG_ERR("Failed to deassert ACPI int pin after SCI read");
    }

    k_event_post(&event, SCI_NOTIFY);
    return 0;
}

static int acpi_active_cooling_sci_event(const acpi_cmd_t *cmd_info,
                                         uint8_t *cmd, uint16_t cmd_len,
                                         uint8_t *resp, uint16_t resp_len) {
    sci_t sci;
    int ret;

    ARG_UNUSED(cmd);
    ARG_UNUSED(cmd_len);
    ACPI_CHECK_OUT(cmd_info, resp, resp_len);

    ret = acpi_sci_get(&sci);
    if (ret < 0) {
        return ret;
    }

    resp[0] = sci;
    return 0;
}

// clang-format off
ACPI_CMD_SUBSCRIBE(sci, EC_ACTIVE_COOLING_SCI_EVENT, acpi_active_cooling_sci_event, 1, 0, 1); // Page 37
// clang-format on

static void sci_notify_hdl(void) {
    sci_t sci;

    if ((sci_en == false) || (sci_pwr_allow() == false)) {
        sci_flush();
        return;
    }

    if (sci_buf != SCI_NONE) {
        LOG_WRN("SCI 0x%02x still pending", sci_buf);
        return;
    }

    if (k_msgq_get(&sci_queue, &sci, K_NO_WAIT) < 0) {
        return;
    }

    sci_buf = sci;

    if (acpi_int_set(1) < 0) {
        LOG_ERR("Failed to assert ACPI int pin for SCI 0x%02x", sci_buf);
    }

    k_timer_start(&sci_tmr, K_MSEC(CONFIG_ACPI_SCI_TIMEOUT_MS), K_NO_WAIT);

    LOG_INF("Notify SCI 0x%02x (%u queued)", sci_buf,
            k_msgq_num_used_get(&sci_queue));
}

static void sci_timeout_hdl(void) {
    if (sci_buf == SCI_NONE) {
        return;
    }

    LOG_WRN("SCI 0x%02x notification timeout; retaining pending event",
            sci_buf);

    if (acpi_int_set(0) < 0) {
        LOG_ERR("Failed to deassert ACPI int pin after SCI timeout");
    }
}

static void service(void) {
    uint32_t evt;

    while (1) {
        evt = k_event_wait(&event,
                           SCI_NOTIFY | SCI_TIMEOUT | SYS_PWR_STA_CHG,
                           false, K_FOREVER);

        if (evt & SCI_TIMEOUT) {
            k_event_clear(&event, SCI_TIMEOUT);
            sci_timeout_hdl();
        }

        if (evt & (SCI_NOTIFY | SYS_PWR_STA_CHG)) {
            k_event_clear(&event, (SCI_NOTIFY | SYS_PWR_STA_CHG));
            sci_notify_hdl();
        }
    }
}

K_THREAD_DEFINE(sci_id, APP_STACK_MIN, service, NULL, NULL, NULL, APP_PRIO_M,
                0, 0);

#ifdef CONFIG_SCI_SHELL
#include <zephyr/shell/shell.h>

static int cmd_sci_en(const struct shell *sh, size_t argc, char **argv) {
    bool en = (bool)strtoul(argv[1], NULL, 0);
    acpi_sci_enable_set(en);
    shell_info(sh, "SCI functionality %s", en ? "enabled" : "disabled");
    return 0;
}

static int cmd_sci_put(const struct shell *sh, size_t argc, char **argv) {
    sci_t sci = (sci_t)strtoul(argv[1], NULL, 16);
    int ret = sci_enque(sci);

    if (ret < 0) {
        shell_error(sh, "Failed to put SCI: %d", ret);
    } else {
        shell_info(sh, "SCI 0x%02x submitted (pending 0x%02x, %u queued)",
                   sci, sci_buf, k_msgq_num_used_get(&sci_queue));
    }
    return ret;
}

static int cmd_sci_sta(const struct shell *sh, size_t argc, char **argv) {
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

static int cmd_sci_get(const struct shell *sh, size_t argc, char **argv) {
    sci_t sci;
    bool en;

    acpi_sci_enable_get(&en);
    acpi_sci_get(&sci);
    shell_info(sh, "SCI Enable Status: %d", en);
    shell_info(sh, "Retrieved SCI Value: 0x%02x", sci);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_sci,
    SHELL_CMD_ARG(en, NULL, "Enable or disable SCI notifications <0|1>", cmd_sci_en, 2, 0),
    SHELL_CMD_ARG(put, NULL, "Queue a System Control Interrupt (SCI) event <hex_val>", cmd_sci_put, 2, 0),
    SHELL_CMD_ARG(get, NULL, "Read and clear the pending SCI event", cmd_sci_get, 1, 0),
    SHELL_CMD_ARG(sta, NULL, "Show SCI enable/power/queue status", cmd_sci_sta, 1, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sci, &sub_sci, "SCI control commands", NULL);
#endif
