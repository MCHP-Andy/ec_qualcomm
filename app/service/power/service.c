/*
 * @Author: andy.chang 
 * @Date: 2025-07-01 02:46:45 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-23 00:13:18
 */

#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <interface/system.h>
#include <interface/power.h>

LOG_MODULE_REGISTER(power, CONFIG_POWER_LOG_LEVEL);

enum {
    PWR_EVT_STA_UPDATE = LOCAL_EVT_START,
};

#define PWR_STA_UPDATE BIT(PWR_EVT_STA_UPDATE)

static K_EVENT_DEFINE(event);
SYS_EVENT_SUBSCRIBE(power, event);
static pwr_sta_t pwr_state = PWR_STA_G3;
static pwr_sta_t next_pwr_state = PWR_STA_G3;

static const char *pwr_state_str[] = {
    [PWR_STA_S0] = "S0", [PWR_STA_MS] = "Modern Standby",
    [PWR_STA_S3] = "S3", [PWR_STA_S4] = "S4",
    [PWR_STA_S5] = "S5", [PWR_STA_G3] = "G3",
};

static const char *pwr_state_to_str(pwr_sta_t state) {
    if (state <= 0 || state >= PWR_STA_MAX || pwr_state_str[state] == NULL) {
        return "Unknown";
    }
    return pwr_state_str[state];
}

int pwr_state_get(pwr_sta_t *state) {
    if (state == NULL) {
        LOG_ERR("Invalid state pointer");
        return -EINVAL;
    }

    *state = pwr_state;

    LOG_DBG("Current power state: %s (%d)", pwr_state_to_str(*state), *state);

    return 0;
}

int pwr_state_set(pwr_sta_t state) {
    if (state >= PWR_STA_MAX || state < PWR_STA_S0) {
        LOG_ERR("Invalid power state: %d", state);
        return -EINVAL;
    }

    if (state == pwr_state) {
        LOG_WRN("Power state already at %s", pwr_state_to_str(state));
        return 0;
    }

    next_pwr_state = state;

    // Notify service thread to process power state change
    k_event_post(&event, PWR_STA_UPDATE);

    return 0;
}

static void service(void) {
    uint32_t evt = 0;

    while (1) {
        // Get event
        evt = k_event_wait(&event, (SYS_EVT_MASK | PWR_STA_UPDATE), true,
                           K_FOREVER);

        if (evt & PWR_STA_UPDATE) {
            pwr_sta_t state = 0;
            pwr_state_get(&state);

            LOG_INF("Prepare to change power state: %s -> %s",
                    pwr_state_to_str(state), pwr_state_to_str(next_pwr_state));

            // TODO: power process depend on platform
            // ...

            pwr_state = next_pwr_state;
            pwr_state_get(&state);
            LOG_INF("Power state: %s", pwr_state_to_str(state));

            // Notify other modules of power state change
            SYS_EVENT_SUBMIT(SYS_PWR_STA_CHG);
        }
    }
}

K_THREAD_DEFINE(pwr_id, APP_STACK_MIN, service, NULL, NULL, NULL, APP_PRIO_M, 0,
                0);

#ifdef CONFIG_POWER_SHELL
#include <zephyr/shell/shell.h>

static int cmd_pwr_get(const struct shell *sh, size_t argc, char **argv) {
    pwr_sta_t state;

    pwr_state_get(&state);
    shell_info(sh, "Current power state: %s (%d)", 
               pwr_state_to_str(state), state);
    return 0;
}

static int cmd_pwr_set(const struct shell *sh, size_t argc, char **argv) {
    pwr_sta_t state = (pwr_sta_t)strtoul(argv[1], NULL, 0);
    int ret = pwr_state_set(state);

    if (ret < 0) {
        shell_error(sh, "Failed to set power state: %d", ret);
    } else {
        shell_info(sh, "Power state set to: %s (%d)", pwr_state_to_str(state), state);
    }

    return ret;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_pwr,
    SHELL_CMD_ARG(get, NULL, "Get current power state", cmd_pwr_get, 1, 0),
    SHELL_CMD_ARG(set, NULL, "Set power state <state_id>\n"
                             "1: S0, 2: Modern Standby, 3: S3, 4: S4, 5: S5, 6: G3", cmd_pwr_set, 2, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(power, &sub_pwr, "Power commands", NULL);
#endif
