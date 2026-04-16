/*
 * @Author: andy.chang 
 * @Date: 2026-04-16 15:22:05 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-16 18:35:33
 */

#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <interface/acpi.h>

enum {
	I2C_STATE_IDLE = 0,
	I2C_STATE_START_WRITE,
	I2C_STATE_WRITE,
	I2C_STATE_READ,
};

static const struct device *bus = DEVICE_DT_GET(DT_ALIAS(acpi_i2c));

static uint8_t i2c_state = I2C_STATE_IDLE;

static uint8_t idx = 0;
static uint8_t rece_cmd[ACPI_RECE_LEN] = {0};
static uint8_t resp_buf[ACPI_RESP_LEN] = {0};

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

    rece_cmd[idx++] = val;

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
    case I2C_STATE_IDLE:
        printk("acpi target read request: 0x%02x\n", *val);

        // Copy/Read data to resp_buf
        // acpi_read(resp_buf, sizeof(resp_buf));

        idx = 0;
        *val = resp_buf[idx++];

        i2c_state = I2C_STATE_READ;
        break;
    case I2C_STATE_READ:
        printk("acpi target read processed: 0x%02x\n", *val);
        *val = resp_buf[idx++];
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
        // acpi_write(rece_cmd, idx);
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
    // TODO: support multiple addresses e.g. SOCCP_ADDR
    .address = ACPI_ADDR,
    .callbacks = &acpi_target_callbacks,
};

#include <zephyr/init.h>

static int init_config(void) {

    printk("i2c custom target sample\n");

    if (i2c_target_register(bus, &target_cfg) < 0) {
        printk("Failed to register target\n");
        return -1;
    }

    return 0;
}

SYS_INIT(init_config, APPLICATION, 0);
