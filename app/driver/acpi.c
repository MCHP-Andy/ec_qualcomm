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

#define ACPI_ADDR  (0x76)
#define SOCCP_ADDR (0x0C)

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

// Workaround for 2 slave address, check address to determine which command to process
static uint8_t i2c_addr = 0;

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

    // Workaround for 2 slave address, check address to determine which command to process
    #define ACPI_I2C_REG_BASE (DT_REG_ADDR_BY_IDX(DT_ALIAS(acpi_i2c), 0))
    #define I2C_DATA_REG_OFFSET 0x08
    i2c_addr = (sys_read32(ACPI_I2C_REG_BASE + I2C_DATA_REG_OFFSET) >> 1) & 0x7F;

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
        if (i2c_addr == ACPI_ADDR) {
            acpi_buf_set(ACPI_TYPE_CMD, val);
        } else if (i2c_addr == SOCCP_ADDR) {
            soccp_buf_set(SOCCP_TYPE_CMD, val);
        } else {
            LOG_WRN("Unknown I2C address: 0x%02x", i2c_addr);
        }            

        i2c_state = I2C_STATE_WRITE;
        break;
    case I2C_STATE_WRITE:
        if (i2c_addr == ACPI_ADDR) {
            acpi_buf_set(ACPI_TYPE_DATA, val);
        } else if (i2c_addr == SOCCP_ADDR) {
            soccp_buf_set(SOCCP_TYPE_DATA, val);
        } else {
            LOG_WRN("Unknown I2C address: 0x%02x", i2c_addr);
        }
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
static int acpi_target_read_processed_cb(struct i2c_target_config *config, uint8_t *val) {
    switch (i2c_state) {
    case I2C_STATE_READ:
        *val = resp_buf[resp_idx++];
        LOG_DBG("acpi target read processed: 0x%02x", *val);
        break;

    default:
        LOG_WRN("Unexpected I2C state: %d", i2c_state);
        break;
    }

    return 0;
}

static int acpi_target_read_requested_cb(struct i2c_target_config *config, uint8_t *val) {

    i2c_state = I2C_STATE_READ;
    resp_idx = 0;

    return acpi_target_read_processed_cb(config, val);
}

/*
 * @brief Callback which is called when the master sends a stop condition.
 * @param config Pointer to the target configuration.
 */
static int acpi_target_stop_cb(struct i2c_target_config *config) {
    LOG_DBG("acpi target stop callback");

    resp_idx = 0;
    i2c_state = I2C_STATE_IDLE;

    if (i2c_addr == ACPI_ADDR) {
        acpi_buf_set(ACPI_TYPE_PROCESS, 0);
    }

    return 0;
}

static struct i2c_target_callbacks acpi_target_callbacks = {
    .write_requested = acpi_target_write_requested_cb,
    .write_received = acpi_target_write_received_cb,
    .read_requested = acpi_target_read_requested_cb,
    .read_processed = acpi_target_read_processed_cb,
    .stop = acpi_target_stop_cb,
};

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
