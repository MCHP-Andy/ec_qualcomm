/*
 * @Author: andy.chang 
 * @Date: 2026-11-10 01:47:09 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-11-10 01:47:30
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <zephyr/drivers/gpio.h>

#define QC_GPIO_GET(name) GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), name##_gpios)

#define QC_GPIO_DEF(name)                                                      \
    static const struct gpio_dt_spec name = QC_GPIO_GET(name)

typedef struct gpio_cb_t {
    const struct gpio_dt_spec spec;
    gpio_flags_t flags;
    struct gpio_callback cb_data;
    char *name;
} gpio_cb_t;
