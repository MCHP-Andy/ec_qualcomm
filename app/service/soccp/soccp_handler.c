#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <interface/system.h>
#include <interface/fan.h>
#include <interface/power.h>
#include "soccp_handler.h"

LOG_MODULE_DECLARE(soccp, LOG_LEVEL_DBG);

#define SOCCP_CHECK_IN(cmd_info_ptr, c, cl)                                    \
    do {                                                                       \
        if (!(cmd_info_ptr)) {                                                 \
            LOG_ERR("Invalid cmd_info_ptr (NULL) for SOCCP_CHECK_IN");         \
            return -EINVAL;                                                    \
        }                                                                      \
        if (!(c) || (cl) < (cmd_info_ptr)->mand) {                             \
            LOG_ERR("Invalid input for cmd 0x%02x: ptr=%p, len=%u (min=%u)",   \
                    (cmd_info_ptr)->cmd, (void *)(c), (unsigned int)(cl),      \
                    (unsigned int)((cmd_info_ptr)->mand));                     \
            return -EINVAL;                                                    \
        }                                                                      \
    } while (0)

#define SOCCP_CHECK_OUT(cmd_info_ptr, r, rl)                                   \
    do {                                                                       \
        if (!(cmd_info_ptr)) {                                                 \
            LOG_ERR("Invalid cmd_info_ptr (NULL) for SOCCP_CHECK_OUT");        \
            return -EINVAL;                                                    \
        }                                                                      \
        if (!(r) || (rl) < (cmd_info_ptr)->resp_len) {                         \
            LOG_ERR("Invalid output for cmd 0x%02x: ptr=%p, len=%u (min=%u)",  \
                    (cmd_info_ptr)->cmd, (void *)(r), (unsigned int)(rl),      \
                    (unsigned int)((cmd_info_ptr)->resp_len));                 \
            return -EINVAL;                                                    \
        }                                                                      \
    } while (0)

/* Latest OffMode/OOB status reported by SoC-CP (Ref: page 56) */
static uint16_t oob_status = 0;

int soccp_oob_state_get(uint16_t *status) {

    if (status == NULL) {
        LOG_ERR("Invalid status pointer");
        return -EINVAL;
    }

    *status = oob_status;

    return 0;
}

/**
 * @brief EC SoCCP who-am-i interface (Cmd: 0x43)
 * Ref: Page 54 of Spec
 */
int soccp_who_am_i(const soccp_cmd_t *cmd_info, uint8_t *cmd, uint16_t cmd_len, uint8_t *resp, uint16_t resp_len) {
    ARG_UNUSED(cmd);
    ARG_UNUSED(cmd_len);

    SOCCP_CHECK_OUT(cmd_info, resp, resp_len);

    /* SoCCP identify value is 0x05 (Offset 0x01, no byte count) */
    resp[0] = SOCCP_WHO_AM_I_VALUE;
    LOG_INF("SoCCP Who-Am-I called, returning 0x%02x", SOCCP_WHO_AM_I_VALUE);
    return 0;
}

/**
 * @brief EC FAN Constraints Message (Cmd: 0x03, Reg: 0x20)
 *
 * SoC-CP notifies the EC about power constraints to avoid brown out of the
 * device.
 * Ref: Page 55 of Spec
 */
static int soccp_fan_constraints(uint16_t data) {
    bool allow_on;

    switch (data) {
    case SOCCP_FAN_CONSTRAINT_OFF:
        allow_on = false;
        break;

    case SOCCP_FAN_CONSTRAINT_ON:
        allow_on = true;
        break;

    default:
        LOG_ERR("SoCCP: Invalid fan state: 0x%04x", data);
        return -EINVAL;
    }

    LOG_INF("SoCCP: Fan constraint set to %s", allow_on ? "ON" : "OFF");

    return fan_constraint_set(allow_on);
}

/**
 * @brief EC Off mode / OOB State Message (Cmd: 0x03, Reg: 0x25)
 *
 * SoC-CP notifies the EC about OOB power state and Off mode charging state.
 * Ref: Page 56 of Spec
 */
static int soccp_oob_state(uint16_t data) {

    if (data & ~SOCCP_OOB_STA_MASK) {
        /* Bit 3 to 7 are reserved, keep the known bits only */
        LOG_WRN("SoCCP: Reserved bits set in OOB status: 0x%04x", data);
        data &= SOCCP_OOB_STA_MASK;
    }

    if (!(data & SOCCP_OOB_STA_ACTIVE)) {
        /* Bit 1 is always expected to be set, since SoCCP is active in all
         * states. Report it but keep the value, SoC-CP is the owner. */
        LOG_WRN("SoCCP: OOB status without active bit: 0x%04x", data);
    }

    oob_status = data;

    LOG_INF("SoCCP: OOB status updated: 0x%04x (off-mode: %d, active: %d, "
            "initialized: %d)",
            oob_status, !!(oob_status & SOCCP_OOB_STA_OFF_MODE),
            !!(oob_status & SOCCP_OOB_STA_ACTIVE),
            !!(oob_status & SOCCP_OOB_STA_INITED));

    return 0;
}

/**
 * @brief EC SoC Power state message (Cmd: 0x03, Reg: 0x11)
 *
 * SoC-CP notifies the EC the current power state of SoC.
 * Ref: Page 57 of Spec
 */
static int soccp_soc_pwr_state(uint16_t data) {

    /* Power status 1 to 6 maps 1:1 onto pwr_sta_t, 0 is reserved */
    if (data < PWR_STA_S0 || data >= PWR_STA_MAX) {
        LOG_ERR("SoCCP: Invalid power status: 0x%04x", data);
        return -EINVAL;
    }

    LOG_INF("SoCCP: SoC power state notified: %d", data);

    return pwr_state_set((pwr_sta_t)data);
}

/**
 * @brief EC Power State Feature Controller (Cmd: 0x03)
 * Handles Fan Constraints (0x20), OOB State (0x25), and Power State (0x11)
 * Ref: Page 55-57 of Spec
 */
int soccp_power_state_ctrl(const soccp_cmd_t *cmd_info, uint8_t *cmd, uint16_t cmd_len, uint8_t *resp, uint16_t resp_len) {
    ARG_UNUSED(resp);
    ARG_UNUSED(resp_len);

    /* cmd[0] is the opcode, cmd[1] is Register ID, cmd[2-3] is Data (LSB-MSB) */
    SOCCP_CHECK_IN(cmd_info, cmd, cmd_len);

    uint8_t reg_id = cmd[1];
    uint16_t data = ((uint16_t)cmd[3] << 8) | cmd[2];

    switch (reg_id) {
    case SOCCP_REG_FAN_CONSTRAINTS:
        return soccp_fan_constraints(data);

    case SOCCP_REG_OOB_STATE:
        return soccp_oob_state(data);

    case SOCCP_REG_SOC_PWR_STATE:
        return soccp_soc_pwr_state(data);

    default:
        LOG_WRN("SoCCP: Unknown Power State Register ID: 0x%02x", reg_id);
        return -EINVAL;
    }
}
