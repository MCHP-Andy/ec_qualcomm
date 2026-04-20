/*
 * @Author: andy.chang 
 * @Date: 2025-07-01 02:46:45 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-21 01:48:52
 */

#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <interface/system.h>
#include <interface/power.h>

LOG_MODULE_REGISTER(power, LOG_LEVEL_DBG);

static pwr_sta_t pwr_state = PWR_STA_G3;

static const char *pwr_state_str[] = {
	[PWR_STA_S0] = "S0",
	[PWR_STA_MS] = "Modern Standby",
	[PWR_STA_S3] = "S3",
	[PWR_STA_S4] = "S4",
	[PWR_STA_S5] = "S5",
	[PWR_STA_G3] = "G3",
};

static const char *pwr_state_to_str(pwr_sta_t state)
{
	if (state <= 0 || state >= PWR_STA_MAX || pwr_state_str[state] == NULL) {
		return "Unknown";
	}
	return pwr_state_str[state];
}

int pwr_state_get(pwr_sta_t *state) {
    if (state == NULL) {
        LOG_WRN("Invalid state pointer");
        return -EINVAL;
    }

    *state = pwr_state;

    LOG_DBG("Current power state: %s (%d)", pwr_state_to_str(*state), *state);

    return 0;
}

int pwr_state_set(pwr_sta_t state) {
    if (state >= PWR_STA_MAX || state < PWR_STA_S0) {
        LOG_WRN("Invalid power state: %d", state);
        return -EINVAL;
    }

    if (state == pwr_state) {
        LOG_DBG("Power state already at %s", pwr_state_to_str(state));
        return 0;
    }

    pwr_state = state;

    LOG_INF("Changed power state: %s", pwr_state_to_str(state));

    // TODO: Notify other modules of power state change

    return 0;
}

#if 0 // No need in Qualcomm
static void service(void) {
    int ret = 0;

    while (1) {
        k_sleep(K_SECONDS(5));
    }
}

K_THREAD_DEFINE(pwr_id, APP_STACK_MIN, service, NULL, NULL, NULL, APP_PRIO_M, 0,
                0);
#endif

#ifdef CONFIG_SHELL
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
    SHELL_CMD_ARG(set, NULL, "Set power state <state_id>", cmd_pwr_set, 2, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(power, &sub_pwr, "Power commands", NULL);
#endif
