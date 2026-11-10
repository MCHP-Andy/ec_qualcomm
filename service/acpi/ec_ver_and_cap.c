
#include <string.h> // For memset
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "acpi_tbl.h"

LOG_MODULE_DECLARE(acpi);

static int acpi_dev_fw_ver(const acpi_cmd_t *cmd_info, uint8_t *cmd, uint16_t cmd_len,
                    uint8_t *resp, uint16_t resp_len) {
    (void)cmd;
    (void)cmd_len;
    ARG_UNUSED(cmd_info);

    ACPI_CHECK_OUT(cmd_info, resp, resp_len);

    resp[0] = 3; // Byte count
    resp[1] = CONFIG_APP_FW_VERSION_PATCH; // Test Version
    resp[2] = CONFIG_APP_FW_VERSION_MINOR; // Sub Version
    resp[3] = CONFIG_APP_FW_VERSION_MAJOR; // Major Version

    return 0;
}

static int acpi_dev_fw_ver_and_lowest_supported(const acpi_cmd_t *cmd_info,
                                         uint8_t *cmd, uint16_t cmd_len,
                                         uint8_t *resp, uint16_t resp_len) {
    (void)cmd;
    (void)cmd_len;
    ARG_UNUSED(cmd_info);

    ACPI_CHECK_OUT(cmd_info, resp, resp_len);

    resp[0] = 7; // Byte count (Offset 0x01)
    resp[1] = CONFIG_APP_FW_VERSION_PATCH; // Test Version (Offset 0x02)
    resp[2] = CONFIG_APP_FW_VERSION_MINOR; // Sub Version (Offset 0x03)
    resp[3] = CONFIG_APP_FW_VERSION_MAJOR; // Major Version (Offset 0x04)
    resp[4] = 1; // Lowest Supported Test Version (Offset 0x05)
    resp[5] = 1; // Lowest Supported Sub Version (Offset 0x06)
    resp[6] = 1; // Lowest Supported Major Version (Offset 0x07)
    resp[7] =
        'A'; // Code Mirror Flag ('A' for Active, 'R' for Backup) (Offset 0x08)

    return 0;
}

static int acpi_dev_thermal_capabilities(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                                  uint16_t cmd_len, uint8_t *resp,
                                  uint16_t resp_len) {
    ARG_UNUSED(resp_len); // resp_len is passed to macro, not used directly here
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);
    ACPI_CHECK_OUT(cmd_info, resp, resp_len);

    // Check whether Sub Command is 0x02
    if (cmd[1] != 0x02) { // Sub-command is at cmd[1]
        LOG_ERR("Invalid sub command: 0x%02X", cmd[1]);
        return -EINVAL;
    }

    resp[0] = 2; // Byte count (Offset 0x02)

    // Thermal Capabilities (Offset 0x03 LSB)
    // Bit 0-1: count (2 fans = 0x02), Bit 7: Data Valid (0x80) -> 0x82
    resp[1] = 0x82;

    // Thermal Capabilities (Offset 0x04 MSB)
    // Bit 8-15: Thermistor 0-7 presence (0: present, 1: absent)
    // e.g. if only Thermistor 0,1 exist, value is 0xFC (11111100)
    resp[2] = 0xFC;

    return 0;
}

static int acpi_dev_active_cooling_caps(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                                 uint16_t cmd_len, uint8_t *resp,
                                 uint16_t resp_len) {
    (void)cmd;
    (void)cmd_len;
    ARG_UNUSED(cmd_info);

    ACPI_CHECK_OUT(cmd_info, resp, resp_len);
    resp[0] = 5; // Byte count (Offset 0x01), this is part of the response data

    // Capabilities (Offset 0x02 - 0x05) 32-bit (Little Endian)
    // Assume all 14 standard commands are supported (Bit 0 ~ Bit 13) -> 0x3FFF
    resp[1] = 0xFF; // LSB
    resp[2] = 0x3F;
    resp[3] = 0x00;
    resp[4] = 0x00; // MSB

    // Interface version (Offset 0x06)
    // Bit 0-3: Lowest supported (1), Bit 4-7: Highest supported (1) -> 0x11
    resp[5] = 0x11;

    return 0;
}

static int acpi_who_am_i(const acpi_cmd_t *cmd_info, uint8_t *cmd, uint16_t cmd_len,
                  uint8_t *resp, uint16_t resp_len) {
    (void)cmd;
    (void)cmd_len;
    ARG_UNUSED(cmd_info);

    ACPI_CHECK_OUT(cmd_info, resp, resp_len);
    // ACPI who-am-i value is fixed to 0x01 (Offset 0x01 is Data directly, no
    // Byte Count)
    resp[0] = 0x01;

    return 0;
}

static int acpi_dev_id(const acpi_cmd_t *cmd_info, uint8_t *cmd, uint16_t cmd_len,
                uint8_t *resp, uint16_t resp_len) {
    (void)cmd;
    (void)cmd_len;
    ARG_UNUSED(cmd_info);

    ACPI_CHECK_OUT(cmd_info, resp, resp_len);

    resp[0] = 2; // Byte count (Offset 0x01)

    // Device ID (2 Bytes)
    resp[1] = 0x34; // Device ID LSB (Offset 0x02)
    resp[2] = 0x12; // Device ID MSB (Offset 0x03) - e.g. ID is 0x1234

    return 0;
}

// clang-format off
ACPI_CMD_SUBSCRIBE(board, EC_DEV_FW_VER,                             acpi_dev_fw_ver, 1, 0, 4); // Page 8
ACPI_CMD_SUBSCRIBE(board, EC_DEV_FW_VER_AND_LOWEST_SUPPORTED_FW_VER, acpi_dev_fw_ver_and_lowest_supported, 1, 0, 8); // Page 9
// ACPI_CMD_SUBSCRIBE(board, EC_DEV_FLASHING_CAP,                       acpi_dev_flashing_capabilities, 1, 0, 65); // Page 10 (Not Supported)
ACPI_CMD_SUBSCRIBE(board, EC_DEV_THERMAL_CAP,                        acpi_dev_thermal_capabilities, 2, 0, 3); // Page 11 (SubCmd)
ACPI_CMD_SUBSCRIBE(board, EC_DEV_ACTIVE_COOLING_IF_VER_AND_CAP,      acpi_dev_active_cooling_caps, 1, 0, 6); // Page 12
ACPI_CMD_SUBSCRIBE(board, EC_ACPI_WHOAMI_IF,                         acpi_who_am_i, 1, 0, 1); // Page 13
ACPI_CMD_SUBSCRIBE(board, EC_DEV_ID,                                 acpi_dev_id, 1, 0, 3); // Page 14
// clang-format on
