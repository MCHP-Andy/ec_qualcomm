
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <interface/acpi.h>

#include "acpi_tbl.h" // Include the new acpi_tbl.h for macros

LOG_MODULE_DECLARE(acpi, CONFIG_ACPI_LOG_LEVEL);

static int acpi_soc_to_ec_modern_standby_notifi(const acpi_cmd_t *cmd_info,
                                                uint8_t *cmd, uint16_t cmd_len,
                                                uint8_t *resp,
                                                uint16_t resp_len) {
    ARG_UNUSED(cmd_info);
    ARG_UNUSED(resp);
    ARG_UNUSED(resp_len);
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);

    uint8_t standby_status = cmd[1];
    LOG_DBG("SoC Modern Standby notification: %d", standby_status);
    // TODO: Implement actual handling for modern standby notification
    return 0;
}

static int acpi_func_flag(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                          uint16_t cmd_len, uint8_t *resp, uint16_t resp_len) {
    uint64_t flags = 0;
    bool sci_en;

    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);    // Check for read operation
    ACPI_CHECK_OUT(cmd_info, resp, resp_len); // Check for read operation

    // If cmd_len is mand + opt, it's a write operation
    if (cmd_len == (cmd_info->mand + cmd_info->opt)) { // 1 + 9 = 10
        ACPI_CHECK_IN(cmd_info, cmd, cmd_len); // Check for write operation
        memcpy(&flags, &cmd[2], sizeof(flags));

        sci_en = flags & 0x01;
        acpi_sci_enable_set(sci_en);
        LOG_DBG("Set EC function flags: 0x%016llx", flags);
    }

    acpi_sci_enable_get(&sci_en);
    flags = sci_en;

    resp[0] = 0x08; // Byte count
    memcpy(&resp[1], &flags, sizeof(flags));

    LOG_DBG("Get EC function flags: 0x%016llx", flags);
    return 0;
}

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
ACPI_CMD_SUBSCRIBE(fan, SOC_TO_EC_MODERN_STANDBY_NOTIFI,           acpi_soc_to_ec_modern_standby_notifi, 2, 0, 0); // Page 19 (Status)
ACPI_CMD_SUBSCRIBE(fan, EC_FUNC_FLAG,                              acpi_func_flag, 1, 9, 9); // Page 35/36 (Optional ByteCount + Flag(8))
ACPI_CMD_SUBSCRIBE(fan, EC_ACTIVE_COOLING_SCI_EVENT,               acpi_active_cooling_sci_event, 1, 0, 1); // Page 37
// clang-format on
