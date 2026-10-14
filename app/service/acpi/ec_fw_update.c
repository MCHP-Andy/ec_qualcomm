
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <cmsis_core.h>

#include <interface/storage.h>

#include "acpi_tbl.h"

LOG_MODULE_DECLARE(acpi);

/* 0xBB SPI Service Sub-commands (cmd) */
#define SPI_SUBCMD_EXIT_BOOTLOADER          0x33
#define SPI_SUBCMD_ENTER_BOOTLOADER         0xDC

/* Bootloader 位址 (Get from DTS) */
#define BOOT_START_ADDR (DT_REG_ADDR(DT_NODELABEL(flash0))+DT_REG_ADDR(DT_NODELABEL(boot_partition)))

/* 延遲跳轉，讓 Host 端 I2C transaction 先完成 */
#define EC_ENTER_BBK_JUMP_DELAY_MS          (100)

typedef void (*boot_entry_t)(void);

static void jump_to_bootloader(uint32_t address) {
    // 1. Get the vector table of Bootloader
    uint32_t *vector_table = (uint32_t *)address;

    // 2. Get stack pointer(MSP) in Bootloader - First word in table (Index 0)
    uint32_t msp = vector_table[0];

    // 3. Get Reset Handler - Second word in table (Index 1)
    boot_entry_t boot_entry = (boot_entry_t)vector_table[1];

    printk("Prepare jump to  0x%08x...\n", address);
    printk("New MSP: 0x%08x, Entry: 0x%08x\n", msp, (uint32_t)boot_entry);

    // 4. Disable all IRQ
    __disable_irq();

    // 5. Config vector table offset to VTOR(Point to Bootloader)
    SCB->VTOR = address;

    // 6. Set stack and jump
    __set_MSP(msp);
    boot_entry();
}

static void enter_bootloader_work_hdl(struct k_work *work) {
    ARG_UNUSED(work);

    LOG_WRN("Jumping EC into Bootloader...");
    jump_to_bootloader(BOOT_START_ADDR);
}

static K_WORK_DELAYABLE_DEFINE(enter_bootloader_work, enter_bootloader_work_hdl);

/* 內部狀態變數 */
#define EC_FLASH_PATH_INT                   0x4F
#define EC_STATE_BOOTLOADER_IDLE_WP_OFF     0x0B
#define EC_STATE_BOOTLOADER_IDLE_WP_ON      0x1B
#define EC_STATE_MAIN_IDLE_WP_OFF           0x0C
#define EC_STATE_MAIN_IDLE_WP_ON            0x1C
static bool     in_bootloader_mode = false;
static bool     flash_wp_enabled   = false;
static uint8_t  active_control_path = EC_FLASH_PATH_INT;

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
 * @brief 0xBB: SPI Services Multi-functional Dispatcher
 * App 端目前支援 Enter / Exit Bootloader
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
    case SPI_SUBCMD_ENTER_BOOTLOADER: {
        /* 寫入 boot info，跳轉後由 Bootloader 停留等待更新 */
        ec_boot_info_t *info = ec_boot_info_get();
        info->boot_mode = EC_BOOT_MODE_STAY_IN_BBK;
        info->reset_reason = EC_RESET_REASON_ENTER_BBK;

        int ret = ec_boot_info_save();
        if (ret < 0) {
            return ret;
        }

        LOG_INF("EC entering Bootloader execution mode");
        k_work_schedule(&enter_bootloader_work, K_MSEC(EC_ENTER_BBK_JUMP_DELAY_MS));
        break;
    }

    case SPI_SUBCMD_EXIT_BOOTLOADER:
        LOG_INF("EC already in Main Block");
        break;

    default:
        LOG_ERR("Unsupported SPI Service Sub-Command: 0x%02X", sub_cmd);
        return -EINVAL;
    }

    return 0;
}

// clang-format off
ACPI_CMD_SUBSCRIBE(fw, EC_STATE_AND_WP_STATUS,       acpi_ec_state_wp_status,              1, 0, 3);   // 0xEC (ByteCount + State + Path)
ACPI_CMD_SUBSCRIBE(fw, EC_FW_CODE_MIRROR, acpi_ec_spi_services, 3, 0, 0); // 0xBB (Cmd + BCNT=1 + SubCmd)
// ACPI_CMD_SUBSCRIBE(fw, EC_DEV_FW_CORRUPTION_STATUS, NULL, 1, 0, 2); // Page 39
// ACPI_CMD_SUBSCRIBE(fw, EC_FW_CODE_MIRROR, NULL, 3, 0, 0); // Page 40 (ByteCount + CodeMirrorCmd)
// ACPI_CMD_SUBSCRIBE(fw, EC_READ_CRC, NULL, 1, 0, 6); // Page 46
// ACPI_CMD_SUBSCRIBE(fw, EC_ERASE_MEM_REGION, NULL, 7, 0, 0); // Page 48 (ByteCount + ECControlPath + BlockCount + Address(3))
// ACPI_CMD_SUBSCRIBE(fw, EC_ERASE_MEM_PARTITION, NULL, 3, 0, 0); // Page 49 (ByteCount + QuickEraseCmd)
// ACPI_CMD_SUBSCRIBE(fw, EC_READ_MEM_REGION, NULL, 6, 0, ACPI_RESP_LEN); // Page 50 (ByteCount + ECControlPath + Address(3))
// EC_READ_MEM_REGION_BUF (0xA1) is a response command, not a top-level request command.
// ACPI_CMD_SUBSCRIBE(fw, EC_WRITE_MEM_REGION, NULL, 1, ACPI_RECE_LEN - 1, 0); // Page 51 (Optional Data(variable))
// ACPI_CMD_SUBSCRIBE(fw, EC_WRITE_MEM_REGION_BUF, NULL, 7, 0, 0); // Page 51 (ByteCount + ECControlPath + Size + Address(3))
// clang-format on
