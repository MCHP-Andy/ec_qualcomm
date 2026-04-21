
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "acpi_tbl.h"
#include "ec_ver_and_cap.h"

LOG_MODULE_DECLARE(acpi);

int acpi_dev_fw_ver(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp, uint8_t resp_len) {
    (void) cmd;
    (void) cmd_len;

    ACPI_CHECK_OUT(resp, resp_len, 4);

    resp[0] = 3; // Number of bytes following
    resp[1] = 1; // Test Version
    resp[2] = 1; // Sub Version
    resp[3] = 1; // Major Version

    return 0;
}

int acpi_dev_fw_ver_and_lowest_supported(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp, uint8_t resp_len) {
    (void) cmd;
    (void) cmd_len;

    ACPI_CHECK_OUT(resp, resp_len, 8);

    resp[0] = 7;   // Byte count (Offset 0x01)
    resp[1] = 1;   // Test Version (Offset 0x02)
    resp[2] = 1;   // Sub Version (Offset 0x03)
    resp[3] = 1;   // Major Version (Offset 0x04)
    resp[4] = 1;   // Lowest Supported Test Version (Offset 0x05)
    resp[5] = 1;   // Lowest Supported Sub Version (Offset 0x06)
    resp[6] = 1;   // Lowest Supported Major Version (Offset 0x07)
    resp[7] = 'A'; // Code Mirror Flag ('A' for Active, 'R' for Backup) (Offset 0x08)

    return 0;
}

// This command is currently Not Supported.
#if 0
int acpi_dev_flashing_capabilities(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp, uint8_t resp_len) {
    (void) cmd;
    (void) cmd_len;

    if (resp == NULL) {
        LOG_ERR("Response buffer is NULL");
        return -ENOMEM;
    }

    // Byte count = 64，所以 resp 至少需要 65 bytes
    if (resp_len < 65) {
        LOG_ERR("Response buffer length is too small: %d", resp_len);
        return -EINVAL;
    }

    // 初始化整個 Buffer
    memset(resp, 0, 65);

    resp = 64;  // Byte count (Offset 0x01)
    
    // 以下為範例填值，實際需依平台配置填入對應的 Binary Size, Bootloader, Main FW Address 等
    resp[9] = 0x00; // Binary Size LSB (Offset 0x02)
    resp[10] = 0x08; // Binary Size MSB (Offset 0x03)
    // ... Offset 0x04 到 0x41 預設為 0
    // 例如 resp[18] (Offset 0x20) 代表 Internal memory erase sector size in KB

    return 0;
}
#endif

int acpi_dev_thermal_capabilities(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp, uint8_t resp_len) {
    ACPI_CHECK_IN(cmd, cmd_len, 1);
    ACPI_CHECK_OUT(resp, resp_len, 3);

    // 檢查 Sub Command 是否為 0x02
    if (cmd[0] != 0x02) {
        LOG_ERR("Invalid sub command: 0x%02X", cmd[0]);
        return -EINVAL;
    }

    resp[0] = 2; // Byte count (Offset 0x02)
    
    // Thermal Capabilities (Offset 0x03 LSB)
    // Bit 0-1: 數量(2 fans = 0x02), Bit 7: Data Valid (0x80) -> 0x82
    resp[1] = 0x82; 
    
    // Thermal Capabilities (Offset 0x04 MSB)
    // Bit 8-15: Thermistor 0-7 presence (0: present, 1: absent) 
    // 例如僅存在 Thermistor 0,1，則為 0xFC (11111100)
    resp[2] = 0xFC; 

    return 0;
}

int acpi_dev_active_cooling_caps(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp, uint8_t resp_len) {
    (void) cmd;
    (void) cmd_len;

    ACPI_CHECK_OUT(resp, resp_len, 6);

    resp[0] = 5; // Byte count (Offset 0x01)
    
    // Capabilities (Offset 0x02 - 0x05) 32-bit (Little Endian)
    // 假設支援所有 14 個標準指令 (Bit 0 ~ Bit 13) -> 0x3FFF
    resp[1] = 0xFF; // LSB
    resp[2] = 0x3F;
    resp[3] = 0x00;
    resp[4] = 0x00; // MSB
    
    // Interface version (Offset 0x06)
    // Bit 0-3: Lowest supported (1), Bit 4-7: Highest supported (1) -> 0x11
    resp[5] = 0x11; 

    return 0;
}

int acpi_who_am_i(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp, uint8_t resp_len) {
    (void) cmd;
    (void) cmd_len;

    ACPI_CHECK_OUT(resp, resp_len, 1);

    // ACPI who-am-i 值固定為 0x01 (Offset 0x01 處直接是 Data，無 Byte Count)
    resp[0] = 0x01; 

    return 0;
}

int acpi_dev_id(uint8_t *cmd, uint8_t cmd_len, uint8_t *resp, uint8_t resp_len) {
    (void) cmd;
    (void) cmd_len;

    ACPI_CHECK_OUT(resp, resp_len, 3);

    resp[0] = 2;    // Byte count (Offset 0x01)
    
    // Device ID (2 Bytes)
    resp[1] = 0x34; // Device ID LSB (Offset 0x02)
    resp[2] = 0x12; // Device ID MSB (Offset 0x03) - 例如 ID 為 0x1234

    return 0;
}
