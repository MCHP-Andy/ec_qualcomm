#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/sys/reboot.h>
#include <zephyr/arch/cpu.h>
#include <cmsis_core.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

// Get from DTS
#define APP_START_ADDR (DT_REG_ADDR(DT_NODELABEL(flash0))+DT_REG_ADDR(DT_NODELABEL(app_partition)))

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

int main(void) {
    printk("Hello from bootloader\n");

    printk("App address in: 0x%08x\n", APP_START_ADDR);

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
