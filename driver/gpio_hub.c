/*
 * @Author: andy.chang 
 * @Date: 2026-11-10 01:47:41 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-11-10 02:46:24
 */

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include <interface/system.h>

#include "gpio_hub.h"

LOG_MODULE_REGISTER(gpio_hub, LOG_LEVEL_INF);

typedef struct gpio_lst_t {
    const struct gpio_dt_spec spec;
    gpio_flags_t config;
} gpio_lst_t;

static const gpio_lst_t gpio_lst[] = {
    {
        QC_GPIO_GET(fan_max_rpm),
        GPIO_INPUT,
    },
    {
        QC_GPIO_GET(fan_12v_enable),
        GPIO_OUTPUT_INACTIVE,
    },
};

#include <zephyr/init.h>

static int gpio_hub_init(void) {

    for (uint16_t i = 0; i < ARRAY_SIZE(gpio_lst); i++) {
        if (!gpio_is_ready_dt(&gpio_lst[i].spec) ||
            gpio_pin_configure_dt(&gpio_lst[i].spec, gpio_lst[i].config) != 0) {
            LOG_ERR("Failed to configure init GPIO %u", (unsigned int)i);
        }
    }

    return 0;
}

SYS_INIT(gpio_hub_init, APPLICATION, SYS_INIT_EARLY);
