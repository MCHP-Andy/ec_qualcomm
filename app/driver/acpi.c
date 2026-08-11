/*
 * @Author: andy.chang 
 * @Date: 2026-04-16 15:22:05 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-23 23:44:13
 */

#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <interface/acpi.h>
#include <interface/soccp.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(acpi_i2c, LOG_LEVEL_INF);

enum {
	I2C_STATE_IDLE = 0,
	I2C_STATE_START_WRITE,
	I2C_STATE_WRITE,
	I2C_STATE_READ,
};

enum {
    CMD_UNKNOWN = 0,
    CMD_ACPI,
    CMD_SOCCP,
};

static const struct device *bus = DEVICE_DT_GET_OR_NULL(DT_ALIAS(acpi_i2c));

static uint8_t i2c_state = I2C_STATE_IDLE;

static uint16_t resp_idx = 0;
static uint8_t resp_buf[ACPI_RESP_LEN] = {0};

int acpi_resp_set(uint8_t *pdata, uint16_t len) {
    memcpy(resp_buf, pdata, (len <= sizeof(resp_buf)) ? len : sizeof(resp_buf));
    return 0;
}

int soccp_resp_set(uint8_t *pdata, uint16_t len) {
    memcpy(resp_buf, pdata, (len <= sizeof(resp_buf)) ? len : sizeof(resp_buf));
    return 0;
}

/*
 * @brief Callback which is called when a write request is received from the
 * master.
 * @param config Pointer to the target configuration.
 */
static int acpi_target_write_requested_cb(struct i2c_target_config *config) {
    LOG_DBG("acpi target write requested");

    i2c_state = I2C_STATE_START_WRITE;

    return 0;
}

/*
 * @brief Callback which is called when a write is received from the master.
 * @param config Pointer to the target configuration.
 * @param val The byte received from the master.
 */
static int acpi_target_write_received_cb(struct i2c_target_config *config,
                                         uint8_t val) {
    LOG_DBG("acpi target write received: 0x%02x", val);

    switch (i2c_state) {
    case I2C_STATE_START_WRITE:
        acpi_buf_set(ACPI_TYPE_CMD, val);
        soccp_buf_set(SOCCP_TYPE_CMD, val);

        i2c_state = I2C_STATE_WRITE;
        break;
    case I2C_STATE_WRITE:
        acpi_buf_set(ACPI_TYPE_DATA, val);
        soccp_buf_set(SOCCP_TYPE_DATA, val);
        break;
    default:
        LOG_WRN("Unexpected I2C state: %d", i2c_state);
        break;
    }

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
    
    // prevent from restart
    case I2C_STATE_WRITE:
    case I2C_STATE_IDLE:
        LOG_DBG("acpi target read request: 0x%02x", *val);

        resp_idx = 0;
        *val = resp_buf[resp_idx++];

        i2c_state = I2C_STATE_READ;
        break;
    case I2C_STATE_READ:
        LOG_DBG("acpi target read processed: 0x%02x", *val);
        *val = resp_buf[resp_idx++];
        break;

    default:
        LOG_WRN("Unexpected I2C state: %d", i2c_state);
        break;
    }

    return 0;
}

/*
 * @brief Callback which is called when the master sends a stop condition.
 * @param config Pointer to the target configuration.
 */
static int acpi_target_stop_cb(struct i2c_target_config *config) {
    LOG_DBG("acpi target stop callback");

    resp_idx = 0;
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

    if (i2c_target_register(bus, &target_cfg) < 0) {
        LOG_ERR("Failed to register target");
        return -1;
    }

    return 0;
}

SYS_INIT(init_config, APPLICATION, 0);
