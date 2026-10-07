
#include <zephyr/kernel.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log.h>

#include <interface/flash.h>

#include "acpi_tbl.h"

LOG_MODULE_DECLARE(acpi);
// LOG_MODULE_REGISTER(fw_update, CONFIG_FW_UPDATE_LOG_LEVEL);

/* =========================================================================
 * 80-35782-87 Rev. AC - EC Firmware Update ACPI Commands
 * ========================================================================= */
#define EC_FW_VERSION              0x0E
#define EC_FW_VERSION_LOWEST       0x0F
#define EC_FLASH_WRITE_BUF         0xA0
#define EC_FLASH_READ_BUF          0xA1
#define EC_READ_CRC_RESULT         0xA2
#define EC_FLASH_READ_SETUP        0xA7
#define EC_FLASH_WRITE_COMMIT      0xAA
#define EC_FLASH_ERASE_REGION      0xAE
#define EC_FLASH_ERASE_PARTITION   0xAF
#define EC_SPI_SERVICES            0xBB
#define EC_FW_CORRUPTION_STATUS    0xEB
#define EC_STATE_WP_STATUS         0xEC

/* Control Path (cmd) */
#define EC_FLASH_PATH_EXT                   0x0F
#define EC_FLASH_PATH_INT                   0x4F
#define EC_FLASH_PATH_AUTO                  0xEA

/* 0xBB SPI Service Sub-commands (cmd) */
/* 1. Code Mirror */
#define SPI_SUBCMD_MIRROR_DISABLE           0x11
#define SPI_SUBCMD_MIRROR_IMMEDIATE         0x12
#define SPI_SUBCMD_MIRROR_NEXT_BOOT         0x13

/* 2. Firmware Sync / Recovery */
#define SPI_SUBCMD_SYNC_INT_ACT_MAIN_TO_BK  0x15
#define SPI_SUBCMD_SYNC_INT_BK_MAIN_TO_ACT  0x16
#define SPI_SUBCMD_SYNC_INT_ACT_BOOT_TO_BK  0x17
#define SPI_SUBCMD_SYNC_INT_BK_BOOT_TO_ACT  0x18
#define SPI_SUBCMD_SYNC_EXT_ACT_MAIN_TO_BK  0x19
#define SPI_SUBCMD_SYNC_EXT_BK_MAIN_TO_ACT  0x1A
#define SPI_SUBCMD_SYNC_EXT_ACT_BOOT_TO_BK  0x1B
#define SPI_SUBCMD_SYNC_EXT_BK_BOOT_TO_ACT  0x1C

/* 3. Run CRC */
#define SPI_SUBCMD_CRC_INT_ACT_MAIN         0xF0
#define SPI_SUBCMD_CRC_INT_BK_MAIN          0xF1
#define SPI_SUBCMD_CRC_INT_ACT_ODM          0xF2
#define SPI_SUBCMD_CRC_INT_BK_ODM           0xF3
#define SPI_SUBCMD_CRC_INT_BK_BOOT          0xF4
#define SPI_SUBCMD_CRC_INT_BK_MAIN_BLK      0xF5
#define SPI_SUBCMD_CRC_EXT_ACT_BOOT         0xF6
#define SPI_SUBCMD_CRC_EXT_ACT_MAIN         0xF7
#define SPI_SUBCMD_CRC_EXT_BK_BOOT          0xF8
#define SPI_SUBCMD_CRC_EXT_BK_MAIN          0xF9

/* 4. Bootloader Mode & Reset & Crash */
#define SPI_SUBCMD_EXIT_BOOTLOADER          0x33
#define SPI_SUBCMD_ENTER_BOOTLOADER         0xDC
#define SPI_SUBCMD_RESET_EC                 0xCC
#define SPI_SUBCMD_CRASH_KERNEL_OOPS        0xC0
#define SPI_SUBCMD_CRASH_ASSERT             0xC1
#define SPI_SUBCMD_CRASH_WATCHDOG           0xC2

/* 0xEB Corruption Status Bits */
#define FW_CORRUPT_ACT_BOOT_INTACT          BIT(0)
#define FW_CORRUPT_BK_BOOT_INTACT           BIT(1)
#define FW_CORRUPT_ACT_MAIN_INTACT          BIT(2)
#define FW_CORRUPT_BK_MAIN_INTACT           BIT(3)
#define FW_CORRUPT_INT_MAIN_INTACT          BIT(4)
#define FW_CORRUPT_STATUS_VALID             BIT(7)

/* 0xEC EC State Status */
#define EC_STATE_BOOTLOADER_IDLE_WP_OFF     0x0B
#define EC_STATE_BOOTLOADER_IDLE_WP_ON      0x1B
#define EC_STATE_MAIN_IDLE_WP_OFF           0x0C
#define EC_STATE_MAIN_IDLE_WP_ON            0x1C
#define EC_STATE_SPI_OP_FAILED              0xFA
#define EC_STATE_BUSY                       0xFF

/* 0xA2 CRC Status */
#define EC_CRC_STATUS_PASSED                0xAC
#define EC_CRC_STATUS_FAILED                0xFA


/* 讀寫暫存緩衝區 (A0h 寫入最多 256B，A1h 讀出最多 512B) */
#define RAM_BUFFER_SIZE         512
static uint8_t ram_buffer[RAM_BUFFER_SIZE];
static uint16_t ram_buffer_data_len = 0;

/* 內部狀態變數 */
static uint8_t  current_corruption_status = FW_CORRUPT_STATUS_VALID | 
                                            FW_CORRUPT_ACT_MAIN_INTACT | 
                                            FW_CORRUPT_BK_MAIN_INTACT | 
                                            FW_CORRUPT_ACT_BOOT_INTACT;
static uint8_t  last_crc_status = EC_CRC_STATUS_PASSED;
static uint32_t last_crc_value  = 0xE2975390; // 範例預設 CRC32
static bool     in_bootloader_mode = false;
static bool     flash_wp_enabled   = false;
static uint8_t  active_control_path = EC_FLASH_PATH_INT;

/* =========================================================================
 * 底層抽象驅動介面 (HAL / Flash Driver Callbacks)
 * ========================================================================= */
// extern int hal_flash_erase_partition(uint8_t quick_erase_cmd);
// extern int hal_fw_run_crc(uint8_t partition_id, uint32_t *out_crc, uint8_t *out_status);
// extern int hal_fw_sync(uint8_t sync_cmd);


/* =========================================================================
 * ACPI Command Handler 實作
 * ========================================================================= */

/**
 * @brief 0xEB: EC Device Firmware Corruption Status
 * SoC 查詢目前各分割區健康損毀狀況
 */
static int acpi_ec_fw_corruption_status(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                                        uint16_t cmd_len, uint8_t *resp,
                                        uint16_t resp_len) {
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);
    ACPI_CHECK_OUT(cmd_info, resp, resp_len);

    resp[0] = 1;                         // Byte count = 1
    resp[1] = current_corruption_status; // Corruption status bitfield

    LOG_INF("EC FW Corruption Status query: 0x%02X", resp[1]);
    return 0;
}

/**
 * @brief 0xEC: State of EC and Write Protection Status
 * SoC 查詢 EC 目前處於 Main 還是 Bootloader 區塊，以及 WP 狀態與 Flash 路徑
 */
static int acpi_ec_state_wp_status(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                                   uint16_t cmd_len, uint8_t *resp,
                                   uint16_t resp_len) {
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);
    ACPI_CHECK_OUT(cmd_info, resp, resp_len);

    uint8_t state = 0;
    if (in_bootloader_mode) {
        state = flash_wp_enabled ? EC_STATE_BOOTLOADER_IDLE_WP_ON : EC_STATE_BOOTLOADER_IDLE_WP_OFF;
    } else {
        state = flash_wp_enabled ? EC_STATE_MAIN_IDLE_WP_ON : EC_STATE_MAIN_IDLE_WP_OFF;
    }

    resp[0] = 2;                   // Byte count = 2
    resp[1] = state;               // State of EC
    resp[2] = active_control_path; // 0x4F: Internal, 0x0F: External

    LOG_DBG("EC State: 0x%02X, Control Path: 0x%02X", resp[1], resp[2]);
    return 0;
}

/**
 * @brief 0xA2: Read CRC Result
 * 讀取前次執行 Run CRC 指令後的校驗結果與 32-bit CRC 數值
 */
static int acpi_ec_read_crc_result(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                                   uint16_t cmd_len, uint8_t *resp,
                                   uint16_t resp_len) {
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);
    ACPI_CHECK_OUT(cmd_info, resp, resp_len);

    resp[0] = 5;                       // Byte count = 5
    resp[1] = last_crc_status;         // 0xAC: Passed, 0xFA: Failed
    resp[2] = last_crc_value & 0xFF;         // CRC32 LSB
    resp[3] = (last_crc_value >> 8) & 0xFF;
    resp[4] = (last_crc_value >> 16) & 0xFF;
    resp[5] = (last_crc_value >> 24) & 0xFF; // CRC32 MSB

    LOG_INF("Read CRC: Status=0x%02X, CRC=0x%08X", resp[1], last_crc_value);
    return 0;
}

/**
 * @brief 0xAE: Erase Designated Memory Region in EC
 * 抹除指定的 Flash 區塊 (以 Sector 為單位)
 */
static int acpi_ec_erase_designated_region(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                                           uint16_t cmd_len, uint8_t *resp,
                                           uint16_t resp_len) {
    ARG_UNUSED(resp);
    ARG_UNUSED(resp_len);
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);

    uint8_t  path        = cmd[2];
    uint8_t  block_count = cmd[3];
    uint32_t address     = cmd[4] | ((uint32_t)cmd[5] << 8) | ((uint32_t)cmd[6] << 16); // 24-bit Little Endian

    LOG_INF("Erase Region: Path=0x%02X, StartAddr=0x%06X, Blocks=%d", path, address, block_count);

    int ret = 0;
    ret = qc_flash_erase(address, block_count);
    if (ret < 0) {
        LOG_ERR("Failed to erase flash region: %d", ret);
        return ret;
    }

    return 0;
}

/**
 * @brief 0xAF: Erase Particular Memory Partition in EC
 * 快速抹除特定功能分區 (Main / Bootloader / Active / Backup)
 */
static int acpi_ec_erase_partition(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                                   uint16_t cmd_len, uint8_t *resp,
                                   uint16_t resp_len) {
    ARG_UNUSED(resp);
    ARG_UNUSED(resp_len);
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);

    uint8_t quick_erase_cmd = cmd[2];
    LOG_INF("Quick Erase Partition Command: 0x%02X", quick_erase_cmd);

    int ret = 0;
    // ret = hal_flash_erase_partition(quick_erase_cmd);
    if (ret < 0) {
        LOG_ERR("Quick erase failed: %d", ret);
        return ret;
    }

    /* 抹除後依分區更新健康旗標 (以 Backup Main 抹除為例) */
    if (quick_erase_cmd == 0x23 || quick_erase_cmd == 0x33) {
        current_corruption_status &= ~FW_CORRUPT_BK_MAIN_INTACT;
    } else if (quick_erase_cmd == 0x21 || quick_erase_cmd == 0x31) {
        current_corruption_status &= ~FW_CORRUPT_ACT_MAIN_INTACT;
    }

    return 0;
}

/**
 * @brief 0xA0: Write data to RAM buffer
 * 將資料寫入 EC 內部暫存緩衝區 (最多 256 bytes)
 */
static int acpi_ec_write_ram_buffer(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                                    uint16_t cmd_len, uint8_t *resp,
                                    uint16_t resp_len) {
    ARG_UNUSED(resp);
    ARG_UNUSED(resp_len);
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);

    uint16_t payload_len = cmd_len - 1; // 扣除 cmd
    if (payload_len > 256) {
        LOG_ERR("Write buffer overflow: %d bytes (max 256)", payload_len);
        return -EINVAL;
    }

    memcpy(ram_buffer, &cmd[1], payload_len);
    ram_buffer_data_len = payload_len;

    LOG_DBG("Staged %d bytes into RAM buffer", payload_len);
    return 0;
}

/**
 * @brief 0xAA: Write data from RAM buffer to Designated memory region
 * 將 RAM buffer 的資料燒錄寫入至實體 Flash
 */
static int acpi_ec_write_designated_region(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                                           uint16_t cmd_len, uint8_t *resp,
                                           uint16_t resp_len) {
    ARG_UNUSED(resp);
    ARG_UNUSED(resp_len);
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);

    uint8_t  path       = cmd[2];
    uint8_t  size_unit  = cmd[3]; // 以 32 bytes 為單位
    uint32_t address    = cmd[4] | ((uint32_t)cmd[5] << 8) | ((uint32_t)cmd[6] << 16); // 24-bit
    uint16_t bytes_to_program = size_unit * 32;

    if (bytes_to_program > ram_buffer_data_len) {
        bytes_to_program = ram_buffer_data_len;
    }

    LOG_DBG("Commit RAM buffer to Flash: Path=0x%02X, Addr=0x%06X, Len=%d bytes",
            path, address, bytes_to_program);

    int ret = 0;
    ret = qc_flash_write(address, ram_buffer, bytes_to_program);
    if (ret < 0) {
        LOG_ERR("Failed to program Flash: %d", ret);
        return ret;
    }

    return 0;
}

/**
 * @brief 0xA7: Read from Designated Memory Region Setup
 * 設定欲讀取之 Flash 起始位址與路徑，將資料載入至 RAM buffer
 */
static int acpi_ec_read_designated_region_setup(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                                                uint16_t cmd_len, uint8_t *resp,
                                                uint16_t resp_len) {
    ARG_UNUSED(resp);
    ARG_UNUSED(resp_len);
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);

    uint8_t  path    = cmd[2];
    uint32_t address = cmd[3] | ((uint32_t)cmd[4] << 8) | ((uint32_t)cmd[5] << 16);

    LOG_DBG("Setup Read Flash: Path=0x%02X, Addr=0x%06X", path, address);

    /* 預設讀取 512 bytes 至 ram_buffer */
    int ret = 0;
    ret = qc_flash_read(address, ram_buffer, RAM_BUFFER_SIZE);
    if (ret < 0) {
        LOG_ERR("Failed to read flash into buffer: %d", ret);
        return ret;
    }

    return 0;
}

/**
 * @brief 0xA1: Read data from RAM buffer
 * SoC 讀出先前由 0xA7 載入之 RAM 緩衝區資料
 */
static int acpi_ec_read_ram_buffer(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                                   uint16_t cmd_len, uint8_t *resp,
                                   uint16_t resp_len) {
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);
    ACPI_CHECK_OUT(cmd_info, resp, resp_len);

    uint16_t copy_len = (resp_len < RAM_BUFFER_SIZE) ? resp_len : RAM_BUFFER_SIZE;
    memcpy(resp, ram_buffer, copy_len);

    LOG_DBG("Read RAM Buffer: Returned %d bytes", copy_len);
    return 0;
}

/**
 * @brief 0xBB: SPI Services Multi-functional Dispatcher
 * 整合 Bootloader 切換、Reset、Force Crash、Code Mirror、Sync 與 Run CRC
 */
static int acpi_ec_spi_services(const acpi_cmd_t *cmd_info, uint8_t *cmd,
                                uint16_t cmd_len, uint8_t *resp,
                                uint16_t resp_len) {
    ARG_UNUSED(resp);
    ARG_UNUSED(resp_len);
    ACPI_CHECK_IN(cmd_info, cmd, cmd_len);

    uint8_t sub_cmd = cmd[2];
    LOG_INF("SPI Service Request, Sub-Command: 0x%02X", sub_cmd);

    switch (sub_cmd) {
    /* 1. Enter / Exit Bootloader */
    case SPI_SUBCMD_ENTER_BOOTLOADER:
        in_bootloader_mode = true;
        LOG_INF("EC entered Bootloader execution mode");
        break;

    case SPI_SUBCMD_EXIT_BOOTLOADER:
        in_bootloader_mode = false;
        LOG_INF("EC exited Bootloader, returning to Main Block");
        break;

    /* 2. EC Reset */
    case SPI_SUBCMD_RESET_EC:
        LOG_WRN("Triggering Software EC Reset...");
        sys_reboot(SYS_REBOOT_COLD);
        break;

    /* 3. Force Crash (除錯與測試用) */
    case SPI_SUBCMD_CRASH_KERNEL_OOPS:
        LOG_INF("ACPI Induced Crash: Kernel Oops");
        k_panic();
        break;
    case SPI_SUBCMD_CRASH_ASSERT:
        LOG_INF("ACPI Induced Crash: Assertion");
        __ASSERT(false, "ACPI Triggered EC Assertion");
        break;
    case SPI_SUBCMD_CRASH_WATCHDOG:
        LOG_INF("ACPI Induced Crash: Watchdog Hang");
        while (1) { /* 故意卡死進入 WDT Reset */ }
        break;

    /* 4. Code Mirror */
    case SPI_SUBCMD_MIRROR_DISABLE:
    case SPI_SUBCMD_MIRROR_IMMEDIATE:
    case SPI_SUBCMD_MIRROR_NEXT_BOOT:
        LOG_INF("Handling Code Mirror Option: 0x%02X", sub_cmd);
        break;

    /* 5. Firmware Sync / Recovery */
    case SPI_SUBCMD_SYNC_INT_ACT_MAIN_TO_BK:
    case SPI_SUBCMD_SYNC_INT_BK_MAIN_TO_ACT:
    case SPI_SUBCMD_SYNC_INT_ACT_BOOT_TO_BK:
    case SPI_SUBCMD_SYNC_INT_BK_BOOT_TO_ACT:
    case SPI_SUBCMD_SYNC_EXT_ACT_MAIN_TO_BK:
    case SPI_SUBCMD_SYNC_EXT_BK_MAIN_TO_ACT:
    case SPI_SUBCMD_SYNC_EXT_ACT_BOOT_TO_BK:
    case SPI_SUBCMD_SYNC_EXT_BK_BOOT_TO_ACT:
        LOG_INF("Executing Firmware Sync: 0x%02X", sub_cmd);
        // hal_fw_sync(sub_cmd);
        break;

    /* 6. Run CRC */
    case SPI_SUBCMD_CRC_INT_ACT_MAIN:
    case SPI_SUBCMD_CRC_INT_BK_MAIN:
    case SPI_SUBCMD_CRC_INT_ACT_ODM:
    case SPI_SUBCMD_CRC_INT_BK_ODM:
    case SPI_SUBCMD_CRC_INT_BK_BOOT:
    case SPI_SUBCMD_CRC_INT_BK_MAIN_BLK:
    case SPI_SUBCMD_CRC_EXT_ACT_BOOT:
    case SPI_SUBCMD_CRC_EXT_ACT_MAIN:
    case SPI_SUBCMD_CRC_EXT_BK_BOOT:
    case SPI_SUBCMD_CRC_EXT_BK_MAIN:
        LOG_INF("Triggering Partition CRC Calculation: 0x%02X", sub_cmd);
        // hal_fw_run_crc(sub_cmd, &last_crc_value, &last_crc_status);
        break;

    default:
        LOG_ERR("Unknown SPI Service Sub-Command: 0x%02X", sub_cmd);
        return -EINVAL;
    }

    return 0;
}

// clang-format off
ACPI_CMD_SUBSCRIBE(fw_update, EC_FW_CORRUPTION_STATUS,  acpi_ec_fw_corruption_status,         1, 0, 2);   // 0xEB (ByteCount + Status)
ACPI_CMD_SUBSCRIBE(fw_update, EC_STATE_WP_STATUS,       acpi_ec_state_wp_status,              1, 0, 3);   // 0xEC (ByteCount + State + Path)
ACPI_CMD_SUBSCRIBE(fw_update, EC_READ_CRC_RESULT,       acpi_ec_read_crc_result,              1, 0, 6);   // 0xA2 (ByteCount + Status + CRC4B)
ACPI_CMD_SUBSCRIBE(fw_update, EC_FLASH_ERASE_REGION,    acpi_ec_erase_designated_region,      7, 0, 0);   // 0xAE (Cmd + BCNT=5 + Path + Blk + Addr(3))
ACPI_CMD_SUBSCRIBE(fw_update, EC_FLASH_ERASE_PARTITION, acpi_ec_erase_partition,              3, 0, 0);   // 0xAF (Cmd + BCNT=1 + SubCmd)
ACPI_CMD_SUBSCRIBE(fw_update, EC_FLASH_WRITE_BUF,       acpi_ec_write_ram_buffer,             1, 256, 0); // 0xA0 (Cmd + Data[1..256])
ACPI_CMD_SUBSCRIBE(fw_update, EC_FLASH_WRITE_COMMIT,    acpi_ec_write_designated_region,      7, 0, 0);   // 0xAA (Cmd + BCNT=5 + Path + Size/32 + Addr(3))
ACPI_CMD_SUBSCRIBE(fw_update, EC_FLASH_READ_SETUP,      acpi_ec_read_designated_region_setup, 6, 0, 0);   // 0xA7 (Cmd + BCNT=4 + Path + Addr(3))
ACPI_CMD_SUBSCRIBE(fw_update, EC_FLASH_READ_BUF,        acpi_ec_read_ram_buffer,              1, 0, 512); // 0xA1 (Cmd -> Data)
ACPI_CMD_SUBSCRIBE(fw_update, EC_SPI_SERVICES,          acpi_ec_spi_services,                 3, 0, 0);   // 0xBB (Cmd + BCNT=1 + SubCmd)
// clang-format on


#ifdef CONFIG_FW_UPDATE_SHELL
#include <zephyr/shell/shell.h>

static int cmd_fw_status(const struct shell *sh, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    shell_info(sh, "=== EC Firmware Update Status ===");
    shell_info(sh, "Execution Mode:    %s", in_bootloader_mode ? "Bootloader" : "Main Block");
    shell_info(sh, "Write Protection:  %s", flash_wp_enabled ? "Enabled" : "Disabled");
    shell_info(sh, "Control Path:      0x%02X (%s)", active_control_path, 
               active_control_path == EC_FLASH_PATH_INT ? "Internal Flash" : "External Flash");
    shell_info(sh, "Corruption Status: 0x%02X (Valid: %d)", current_corruption_status, 
               (current_corruption_status & FW_CORRUPT_STATUS_VALID) ? 1 : 0);
    shell_info(sh, "Last CRC Status:   0x%02X, CRC Value: 0x%08X", last_crc_status, last_crc_value);
    return 0;
}

static int cmd_fw_enter_bbk(const struct shell *sh, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    in_bootloader_mode = true;
    shell_info(sh, "Switched EC to Bootloader Mode (Simulated 0xBB 0xDC)");
    return 0;
}

static int cmd_fw_exit_bbk(const struct shell *sh, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    in_bootloader_mode = false;
    shell_info(sh, "Exited Bootloader to Main Block (Simulated 0xBB 0x33)");
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_fw,
    SHELL_CMD(status, NULL, "Dump EC FW update status", cmd_fw_status),
    SHELL_CMD(enbbk,  NULL, "Enter Bootloader mode", cmd_fw_enter_bbk),
    SHELL_CMD(disbbk, NULL, "Exit Bootloader mode", cmd_fw_exit_bbk),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(fw_update, &sub_fw, "EC Firmware Update Commands", NULL);
#endif

// clang-format off
// ACPI_CMD_SUBSCRIBE(fw, EC_DEV_FW_CORRUPTION_STATUS, NULL, 1, 0, 2); // Page 39
// ACPI_CMD_SUBSCRIBE(fw, EC_FW_CODE_MIRROR, NULL, 3, 0, 0); // Page 40 (ByteCount + CodeMirrorCmd)
// ACPI_CMD_SUBSCRIBE(fw, EC_READ_CRC, NULL, 1, 0, 6); // Page 46
// ACPI_CMD_SUBSCRIBE(fw, EC_STATE_AND_WP_STATUS, NULL, 1, 0, 3); // Page 47
// ACPI_CMD_SUBSCRIBE(fw, EC_ERASE_MEM_REGION, NULL, 7, 0, 0); // Page 48 (ByteCount + ECControlPath + BlockCount + Address(3))
// ACPI_CMD_SUBSCRIBE(fw, EC_ERASE_MEM_PARTITION, NULL, 3, 0, 0); // Page 49 (ByteCount + QuickEraseCmd)
// ACPI_CMD_SUBSCRIBE(fw, EC_READ_MEM_REGION, NULL, 6, 0, ACPI_RESP_LEN); // Page 50 (ByteCount + ECControlPath + Address(3))
// EC_READ_MEM_REGION_BUF (0xA1) is a response command, not a top-level request command.
// ACPI_CMD_SUBSCRIBE(fw, EC_WRITE_MEM_REGION, NULL, 1, ACPI_RECE_LEN - 1, 0); // Page 51 (Optional Data(variable))
// ACPI_CMD_SUBSCRIBE(fw, EC_WRITE_MEM_REGION_BUF, NULL, 7, 0, 0); // Page 51 (ByteCount + ECControlPath + Size + Address(3))
// clang-format on
