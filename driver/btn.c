/*
 * Copyright (c) 2019 Microchip Technology Inc.
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(btn, LOG_LEVEL_INF);

static const struct gpio_dt_spec btn = GPIO_DT_SPEC_GET(DT_ALIAS(wk_btn), gpios);
static struct gpio_callback btn_cb_data;

static void btn_callback(const struct device *dev, struct gpio_callback *cb,
                         uint32_t pins) {
    // TODO:
}

#include <zephyr/init.h>
static int btn_init(void) {
	int ret;

    // Initial peripherial
    if (!device_is_ready(btn.port)) {
        LOG_ERR("Error: button device %s is not ready", btn.port->name);
        return 0;
    }

    ret = gpio_pin_configure_dt(&btn, GPIO_INPUT);
    if (ret != 0) {
        LOG_ERR("Error %d: failed to configure %s pin %d", ret,
                btn.port->name, btn.pin);
        return 0;
    }

    ret = gpio_pin_interrupt_configure_dt(&btn, GPIO_INT_EDGE_BOTH);
    if (ret != 0) {
        LOG_ERR("Error %d: failed to configure interrupt on %s pin %d", ret,
                btn.port->name, btn.pin);
        return 0;
    }

    gpio_init_callback(&btn_cb_data, btn_callback, BIT(btn.pin));
    gpio_add_callback(btn.port, &btn_cb_data);
    LOG_INF("Set up button at %s pin %d, level: %d", btn.port->name, btn.pin,
            gpio_pin_get_dt(&btn));

    return 0;
}

SYS_INIT(btn_init, APPLICATION, 0);
