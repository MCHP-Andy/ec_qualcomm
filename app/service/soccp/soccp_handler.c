#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <interface/system.h>
#include "soccp_handler.h"

LOG_MODULE_DECLARE(soccp, LOG_LEVEL_DBG);

#define SOCCP_CHECK_IN(c, cl, min)                                             \
    do {                                                                       \
        if ((c) == NULL || (cl) < (min)) {                                     \
            LOG_ERR("Invalid input: ptr=%p, len=%u (min=%u)", (void *)(c),     \
                    (unsigned int)(cl), (unsigned int)(min));                  \
            return -EINVAL;                                                    \
        }                                                                      \
    } while (0)

#define SOCCP_CHECK_OUT(r, rl, min)                                            \
    do {                                                                       \
        if ((r) == NULL || (rl) < (min)) {                                     \
            LOG_ERR("Invalid output: ptr=%p, len=%u (min=%u)", (void *)(r),    \
                    (unsigned int)(rl), (unsigned int)(min));                  \
            return -EINVAL;                                                    \
        }                                                                      \
    } while (0)

/**
 * @brief EC SoCCP who-am-i interface (Cmd: 0x43)
 * Ref: Page 54 of Spec
 */
int soccp_who_am_i(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp, uint8_t resp_len) {
    ARG_UNUSED(cmd);
    ARG_UNUSED(cmd_len);

    SOCCP_CHECK_OUT(resp, resp_len, 1);

    /* SoCCP identify value is 0x05 */
    resp[0] = 0x05;
    LOG_INF("SoCCP Who-Am-I called, returning 0x05");
    return 0;
}

/**
 * @brief EC Power State Feature Controller (Cmd: 0x03)
 * Handles Fan Constraints (0x20), OOB State (0x25), and Power State (0x11)
 * Ref: Page 55-57 of Spec
 */
int soccp_power_state_ctrl(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp, uint8_t resp_len) {
    /* cmd[0] is Register ID, cmd[1-2] is Data (LSB-MS) */
    SOCCP_CHECK_IN(cmd, cmd_len, 3); 

    uint8_t reg_id = cmd[0];
    uint16_t status = (cmd[2] << 8) | cmd[1];

    switch (reg_id) {
    case 0x20: /* EC FAN Constraints Message */
        LOG_INF("SoCCP: Fan Constraints set to %s", (status & 0x01) ? "ON" : "OFF");
        /* TODO: Call fan_constraints_set(status & 0x01) */
        break;

    case 0x25: /* EC Off mode / OOB State Message */
        LOG_INF("SoCCP: OOB Status updated: 0x%04x", status);
        /* 
         * Bit 0: SoC on Off-mode
         * Bit 1: SoCCP active
         * Bit 2: OOB Initialized
         */
        break;

    case 0x11: /* EC SoC Power state message */
        LOG_INF("SoCCP: SoC Power State changed to: %d", status);
        /* 1:S0, 2:ModernStandby, 3:S3, 4:S4, 5:S5, 6:G3 */
        break;

    default:
        LOG_WRN("SoCCP: Unknown Power State Register ID: 0x%02x", reg_id);
        return -EINVAL;
    }

    return 0;
}
