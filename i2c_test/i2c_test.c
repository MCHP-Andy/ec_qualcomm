/*
 * @Author: andy.chang 
 * @Date: 2026-04-16 15:22:05 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-23 14:50:15
 */

#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

static const struct device *bus = DEVICE_DT_GET(DT_ALIAS(tgt_i2c));

enum {
	I2C_STATE_IDLE = 0,
	I2C_STATE_START_WRITE,
	I2C_STATE_WRITE,
	I2C_STATE_READ,
};

static uint8_t i2c_state = I2C_STATE_IDLE;

static uint8_t idx = 0;
static uint8_t resp_buf[128] = {0};

/*
 * @brief Callback which is called when a write request is received from the
 * master.
 * @param config Pointer to the target configuration.
 */
static int acpi_target_write_requested_cb(struct i2c_target_config *config) {
    printk("acpi target write requested\n");

    i2c_state = I2C_STATE_START_WRITE;
    idx = 0;

    return 0;
}

/*
 * @brief Callback which is called when a write is received from the master.
 * @param config Pointer to the target configuration.
 * @param val The byte received from the master.
 */
static int acpi_target_write_received_cb(struct i2c_target_config *config,
                                        uint8_t val) {
    printk("acpi target write received: 0x%02x\n", val);

    resp_buf[idx++] = val;

    i2c_state = I2C_STATE_WRITE;
    return 0;
}

/*
 * @brief Callback which is called when a read request is received from the
 * master.
 * @param config Pointer to the target configuration.
 * @param val Pointer to the byte to be sent to the master.
 */
static int acpi_target_read_cb(struct i2c_target_config *config, uint8_t *val) {
    switch (i2c_state) {
    case I2C_STATE_WRITE:
    case I2C_STATE_IDLE:
        idx = 0;
        *val = resp_buf[idx++];
        printk("acpi target read request: 0x%02x\n", *val);

        i2c_state = I2C_STATE_READ;
        break;
    case I2C_STATE_READ:
        *val = resp_buf[idx++];
        printk("acpi target read processed: 0x%02x\n", *val);
        break;

    default:
        break;
    }

    return 0;
}

/*
 * @brief Callback which is called when the master sends a stop condition.
 * @param config Pointer to the target configuration.
 */
static int acpi_target_stop_cb(struct i2c_target_config *config) {
    printk("acpi target stop callback\n");

    if (i2c_state == I2C_STATE_WRITE) {
        // Send i2c data to ACPI if needed
        printk("write data:\n");

        for (uint8_t i = 0; i < idx; i++) {
            printk("0x%02x ", resp_buf[i]);
        }
        printk("\n");
    }

    idx = 0;
    i2c_state = I2C_STATE_IDLE;

    return 0;
}

static struct i2c_target_callbacks acpi_target_callbacks = {
    .write_requested = acpi_target_write_requested_cb,
    .write_received = acpi_target_write_received_cb,
    .read_requested = acpi_target_read_cb,
    .read_processed = acpi_target_read_cb,
    .stop = acpi_target_stop_cb,
};

#define ACPI_ADDR  (0x76)
#define SOCCP_ADDR (0x0C)

static struct i2c_target_config target_cfg = {
    // Workaround for 2 slave address
    .address = ACPI_ADDR | (SOCCP_ADDR << 8),
    .callbacks = &acpi_target_callbacks,
};

#include <zephyr/init.h>

static int init_config(void) {
    int ret;

    ret = i2c_target_register(bus, &target_cfg);

    if (ret < 0) {
        printk("Failed to register target: %d\n", ret);
        return -1;
    }

    for (uint8_t i = 0; i < sizeof(resp_buf); i++) {
        resp_buf[i] = i;
    }

    return 0;
}

SYS_INIT(init_config, APPLICATION, 0);
