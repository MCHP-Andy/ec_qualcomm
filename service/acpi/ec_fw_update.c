
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

/* Bootloader address (Get from DTS) */
#define BOOT_START_ADDR (DT_REG_ADDR(DT_NODELABEL(flash0))+DT_REG_ADDR(DT_NODELABEL(boot_partition)))

/* Delay the jump so the Host I2C transaction completes first */
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

/* Internal state variables */
#define EC_FLASH_PATH_INT                   0x4F
static bool     in_bootloader_mode = false;
static bool     flash_wp_enabled   = false;
static uint8_t  active_control_path = EC_FLASH_PATH_INT;

/**
 * @brief 0xEC: State of EC and Write Protection Status
 * SoC queries whether EC is currently in Main or Bootloader, plus WP status and Flash path
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
 * App currently supports Enter / Exit Bootloader
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
        /* Write boot info; after the jump, Bootloader stays and waits for update */
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
// clang-format on
