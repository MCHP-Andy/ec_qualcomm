#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/sys/reboot.h>
#include <zephyr/arch/cpu.h>
#include <cmsis_core.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);


int main(void) {
    printk("Hello from I2C test\n");

    return 0;
}
