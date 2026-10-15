#include <stdio.h>
#include <stdalign.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/sys/reboot.h>
#include <zephyr/arch/cpu.h>
#include <cmsis_core.h>
#include <interface/flash.h>
#include <interface/storage.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

// Get from DTS
#if defined(CONFIG_BOARD_QEMU_CORTEX_M3)
#define HDR_ADDR (DT_REG_ADDR(DT_NODELABEL(flash0))+DT_REG_ADDR(DT_NODELABEL(hdr_partition)))
#define APP_START_ADDR (DT_REG_ADDR(DT_NODELABEL(flash0))+DT_REG_ADDR(DT_NODELABEL(app_partition)))
#elif defined(CONFIG_BOARD_QUAL_MODULE)
#define HDR_ADDR (DT_REG_ADDR(DT_NODELABEL(int_flash))+DT_REG_ADDR(DT_NODELABEL(hdr_partition)))
#define APP_START_ADDR (DT_REG_ADDR(DT_NODELABEL(flash0))+DT_REG_ADDR(DT_NODELABEL(app_partition)))
#endif

typedef void (*app_entry_t)(void);

void jump_to_app(uint32_t address) {
    // 1. Get the vector table of APP
    uint32_t *vector_table = (uint32_t *)address;
    
    // 2. Get stack pointer(MSP) in APP - First word in table (Index 0)
    uint32_t msp = vector_table[0];
    
    // 3. Get Reset Handler - Second word in table (Index 1)
    app_entry_t app_entry = (app_entry_t)vector_table[1];

    printk("Prepare jump to  0x%08x...\n", address);
    printk("New MSP: 0x%08x, Entry: 0x%08x\n", msp, (uint32_t)app_entry);

    // 4. Disable all IRQ
    __disable_irq();

    // 5. Config vector table offset to VTOR(Point to APP)
    SCB->VTOR = address;

    // 6. Set stack and jump
    __set_MSP(msp);
    app_entry();
}

static char hdr_str[] __attribute__((aligned(16))) = {'B', 'B', 'K', ' ', 'V', '\0'};

int main(void) {
    LOG_INF("Hello from bootloader: %s", hdr_str);

    LOG_INF("Header address in: 0x%08x", HDR_ADDR);
    LOG_INF("App address in: 0x%08x", APP_START_ADDR);

    // Get image header from flash
    fw_hdr_t hdr = {0};
    if (ec_get_image_header(&hdr) < 0) {
        LOG_ERR("Failed to read EC FW header");
    }

    ec_validate_image_header(&hdr);
    // jump_to_app(0x10000);


    return 0;
}


#ifdef CONFIG_SHELL
#include <stdlib.h>
#include <zephyr/shell/shell.h>

static int cmd_jump(const struct shell *sh, size_t argc, char **argv) {
    uint32_t addr = APP_START_ADDR;

    if (argc >= 2) {
        addr = (uint32_t)strtoul(argv[1], NULL, 0);
    }


    shell_info(sh, "Jump to APP: 0x%08x", addr);
    k_msleep(1000);
    jump_to_app(addr);

    return 0;
}

SHELL_CMD_REGISTER(jump, NULL, "jump to app_addr: <app_addr>", cmd_jump);

#endif
