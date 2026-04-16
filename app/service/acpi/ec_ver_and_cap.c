
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "ec_ver_and_cap.h"

LOG_MODULE_DECLARE(acpi);

int acpi_dev_fw_ver(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp, uint8_t resp_len) {
    (void) cmd;
    (void) cmd_len;

    if (resp == NULL) {
        LOG_ERR("Response buffer is NULL");
        return -ENOMEM;
    }

    if (resp_len < 4) {
        LOG_ERR("Response buffer length is too small: %d", resp_len);
        return -EINVAL;
    }

    resp[0] = 3; // Number of bytes following
    resp[1] = 1; // Test Version
    resp[2] = 1; // Sub Version
    resp[3] = 1; // Major Version

    return 0;
}