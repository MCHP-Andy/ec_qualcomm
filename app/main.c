#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

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

int main(void)
{
    LOG_INF("Build time : %s %s", __DATE__, __TIME__);
    LOG_INF("Hello: %s", CONFIG_BOARD_TARGET);


    return 0;
}


#ifdef CONFIG_SHELL
#include <stdlib.h>
#include <zephyr/shell/shell.h>
#include <interface/storage.h>

static int cmd_jump(const struct shell *sh, size_t argc, char **argv) {
    uint32_t addr = 0xC0000;
    bool stay_in_bbk = false;

    if (argc >= 2) {
        addr = (uint32_t)strtoul(argv[1], NULL, 0);
    }

    if (argc >= 3) {
        stay_in_bbk = strtoul(argv[2], NULL, 0) != 0;
    }

    // 寫入 boot info，由 Bootloader 決定是否停留
    ec_boot_info_t *info = ec_boot_info_get();
    info->boot_mode = stay_in_bbk ? EC_BOOT_MODE_STAY_IN_BBK : EC_BOOT_MODE_NORMAL;
    if (stay_in_bbk) {
        info->reset_reason = EC_RESET_REASON_ENTER_BBK;
    }
    ec_boot_info_save();

    shell_info(sh, "Jump to APP: 0x%08x, Stay in bootloader: %s", addr,
               stay_in_bbk ? "true" : "false");
    k_msleep(1000);
    jump_to_app(addr);

    return 0;
}

SHELL_CMD_REGISTER(jump, NULL, "jump to app_addr: [app_addr] [stay_in_bootloader 0/1]", cmd_jump);

#endif
