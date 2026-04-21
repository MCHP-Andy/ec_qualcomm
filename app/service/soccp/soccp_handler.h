#ifndef SOCCP_HANDLER_H__
#define SOCCP_HANDLER_H__

#include <stdint.h>

/* SoCCP Command Opcodes per Spec */
#define SOCCP_CMD_POWER_STATE_FEATURE  0x03
#define SOCCP_CMD_WHO_AM_I             0x43

/* Handler function prototypes */
int soccp_who_am_i(uint8_t *cmd, uint8_t cmd_len, 
                   uint8_t *resp, uint8_t resp_len);

int soccp_power_state_ctrl(uint8_t *cmd, uint8_t cmd_len, 
                           uint8_t *resp, uint8_t resp_len);

#endif /* SOCCP_HANDLER_H__ */
