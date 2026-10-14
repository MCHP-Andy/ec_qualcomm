/*
 * @Author: andy.chang
 * @Date: 2026-10-12 10:00:00
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-10-12 10:00:00
 */

/*
 * @Description: Shell commands for EC-AOSS interface (real HW and simulator)
 */

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#include <interface/aoss.h>

/* --- Shell Interface --- */

static int cmd_aoss_status(const struct shell *sh, size_t argc, char **argv) {
    aoss_status_t sta;
    int ret = aoss_status_get(&sta);

    if (ret < 0) {
        shell_error(sh, "AOSS status read failed: %d", ret);
        return ret;
    }

    shell_print(sh, "SoC status: 0x%08x", sta.raw);
    shell_print(sh, "  SoC temp telemetry  : %d", sta.soc_temp_telem);
    shell_print(sh, "  Plat temp telemetry : %d", sta.plat_temp_telem);
    shell_print(sh, "  SoC energy telemetry: %d", sta.soc_engy_telem);
    shell_print(sh, "  Plat energy telemetry:%d", sta.plat_engy_telem);
    shell_print(sh, "  Limit telemetry     : %d", sta.lmt_telem);
    shell_print(sh, "  Limit capability    : %d", sta.lmt_cap);
    return 0;
}

static int cmd_aoss_temp(const struct shell *sh, size_t argc, char **argv) {
    int16_t c, dc;
    int ret = aoss_temp_get(&c, &dc);

    if (ret < 0) {
        shell_error(sh, "AOSS temp read failed: %d", ret);
        return ret;
    }

    shell_print(sh, "SoC avg temp: %d C (raw %d x0.1C)", c, dc);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_aoss,
    SHELL_CMD_ARG(status, NULL, "Read SoC status register", cmd_aoss_status, 1, 0),
    SHELL_CMD_ARG(temp, NULL, "Read SoC average temperature", cmd_aoss_temp, 1, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(aoss, &sub_aoss, "EC-AOSS interface commands", NULL);
