#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/sys/reboot.h>
#include <zephyr/arch/cpu.h>
#include <cmsis_core.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

// TODO: Get from DTS
#define APP_START_ADDR 0x20000

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
    printk("Hello from bootloader\n");

    k_msleep(2000);

	printk("Jump to APP\n");
    k_msleep(10);
    jump_to_app(APP_START_ADDR);

    return 0;
}
