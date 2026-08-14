#ifndef SOCCP_HANDLER_H__
#define SOCCP_HANDLER_H__

#include <stdint.h>
#include <interface/soccp.h>

/* SoCCP Command Opcodes per Spec */
#define SOCCP_CMD_POWER_STATE_FEATURE  0x03
#define SOCCP_CMD_WHO_AM_I             0x43

/* Register IDs of the Power State Feature command (0x03) */
#define SOCCP_REG_SOC_PWR_STATE        0x11 // EC SoC Power state message, page 57
#define SOCCP_REG_FAN_CONSTRAINTS      0x20 // EC FAN Constraints Message, page 55
#define SOCCP_REG_OOB_STATE            0x25 // EC Off mode / OOB State Message, page 56

/* Who-am-i values (Ref: page 54) */
#define SOCCP_WHO_AM_I_INVALID         0x00
#define SOCCP_WHO_AM_I_VALUE           0x05

/* Fan constraint states (Ref: page 55) */
#define SOCCP_FAN_CONSTRAINT_OFF       0x00 // FAN(s) should remain OFF (default)
#define SOCCP_FAN_CONSTRAINT_ON        0x01 // FAN(s) can turn ON

/* Handler function prototypes */
int soccp_who_am_i(const soccp_cmd_t *cmd_info, uint8_t *cmd, uint16_t cmd_len,
                   uint8_t *resp, uint16_t resp_len);

int soccp_power_state_ctrl(const soccp_cmd_t *cmd_info, uint8_t *cmd,
                           uint16_t cmd_len, uint8_t *resp, uint16_t resp_len);

#endif /* SOCCP_HANDLER_H__ */
