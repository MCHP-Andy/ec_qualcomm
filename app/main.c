#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void)
{
    LOG_INF("Build time : %s %s", __DATE__, __TIME__);
    LOG_INF("Hello: %s", CONFIG_BOARD_TARGET);


    return 0;
}
