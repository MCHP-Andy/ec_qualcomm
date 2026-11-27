/*
 * @Description: SoC-CP I2C target (address 0x0C)
 *  Shares the I2C controller with the ACPI target (driver/acpi.c); the I2C
 *  driver selects this target by the address byte.
 */

#include <zephyr/drivers/i2c.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <string.h>

#include <interface/system.h>
#include <interface/soccp.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(soccp_i2c, LOG_LEVEL_INF);

// Read data is provided through buf_read_requested only
BUILD_ASSERT(IS_ENABLED(CONFIG_I2C_TARGET_BUFFER_MODE),
             "soccp.c requires CONFIG_I2C_TARGET_BUFFER_MODE=y");

#define SOCCP_ADDR (0x0C)

enum {
    I2C_STATE_IDLE = 0,
    I2C_STATE_START_WRITE,
    I2C_STATE_WRITE,
    I2C_STATE_READ,
};

// soccp-i2c alias, same I2C controller as the ACPI target (acpi-i2c)
static const struct device *bus = DEVICE_DT_GET_OR_NULL(DT_ALIAS(soccp_i2c));

static uint8_t i2c_state = I2C_STATE_IDLE;
static uint8_t resp_buf[SOCCP_RESP_LEN] = {0};

int soccp_resp_set(uint8_t *pdata, uint16_t len) {
    memcpy(resp_buf, pdata, (len <= sizeof(resp_buf)) ? len : sizeof(resp_buf));
    return 0;
}

/*
 * @brief Callback which is called when a write request is received from the
 * master.
 * @param config Pointer to the target configuration.
 */
static int soccp_target_write_requested_cb(struct i2c_target_config *config) {
    ARG_UNUSED(config);
    LOG_DBG("soccp target write requested");

    i2c_state = I2C_STATE_START_WRITE;

    return 0;
}

/*
 * @brief Callback which is called when a write is received from the master.
 * Every byte is passed to the SoC-CP service, the first one as the opcode.
 * @param config Pointer to the target configuration.
 * @param val The byte received from the master.
 */
static int soccp_target_write_received_cb(struct i2c_target_config *config,
                                          uint8_t val) {
    ARG_UNUSED(config);
    LOG_DBG("soccp target write received: 0x%02x", val);

    switch (i2c_state) {
    case I2C_STATE_START_WRITE:
        soccp_buf_set(SOCCP_TYPE_CMD, val);
        i2c_state = I2C_STATE_WRITE;
        break;
    case I2C_STATE_WRITE:
        soccp_buf_set(SOCCP_TYPE_DATA, val);
        break;
    default:
        LOG_WRN("Unexpected I2C state: %d", i2c_state);
        break;
    }

    return 0;
}

/*
 * @brief Callback which is called once when a read request is received from
 * the master (buffer mode). The driver sends the bytes straight from resp_buf.
 * Always return -EAGAIN: the driver stretches SCL so the service has
 * CONFIG_I2C_XEC_V2_CUSTOM_STRETCH_DELAY_MS to write the response, then sends
 * from resp_buf without calling this again.
 * @param config Pointer to the target configuration.
 * @param ptr Set to the buffer to be sent to the master.
 * @param len Set to the length of the buffer.
 */
static int soccp_target_buf_read_requested_cb(struct i2c_target_config *config,
                                              uint8_t **ptr, uint32_t *len) {
    ARG_UNUSED(config);

    i2c_state = I2C_STATE_READ;

    *ptr = resp_buf;
    *len = sizeof(resp_buf);

    return -EAGAIN;
}

/*
 * @brief Callback which is called when the master sends a stop condition.
 * @param config Pointer to the target configuration.
 */
static int soccp_target_stop_cb(struct i2c_target_config *config) {
    ARG_UNUSED(config);
    LOG_DBG("soccp target stop callback");

    i2c_state = I2C_STATE_IDLE;

    return 0;
}

static struct i2c_target_callbacks soccp_target_callbacks = {
    .write_requested = soccp_target_write_requested_cb,
    .write_received = soccp_target_write_received_cb,
    .buf_read_requested = soccp_target_buf_read_requested_cb,
    .stop = soccp_target_stop_cb,
};

static struct i2c_target_config soccp_target_cfg = {
    .address = SOCCP_ADDR,
    .callbacks = &soccp_target_callbacks,
};

static int init_config(void) {
    if (i2c_target_register(bus, &soccp_target_cfg) < 0) {
        LOG_ERR("Failed to register SoCCP target 0x%02x", SOCCP_ADDR);
        return -1;
    }

    return 0;
}

SYS_INIT(init_config, APPLICATION, SYS_INIT_NORMAL);
