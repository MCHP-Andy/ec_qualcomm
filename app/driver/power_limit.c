/*
 * @Author: andy.chang
 * @Date: 2026-03-12 16:47:34
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-16 18:35:24
 */

#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>


static const struct device *bus = DEVICE_DT_GET_OR_NULL(DT_ALIAS(pw_limit_i2c));
static char last_byte;

/*
 * @brief Callback which is called when a write request is received from the
 * master.
 * @param config Pointer to the target configuration.
 */
static int pwr_lmt_target_write_requested_cb(struct i2c_target_config *config) {
    printk("pwr_lmt target write requested\n");
    return 0;
}

/*
 * @brief Callback which is called when a write is received from the master.
 * @param config Pointer to the target configuration.
 * @param val The byte received from the master.
 */
static int pwr_lmt_target_write_received_cb(struct i2c_target_config *config,
                                        uint8_t val) {
    printk("pwr_lmt target write received: 0x%02x\n", val);
    last_byte = val;
    return 0;
}

/*
 * @brief Callback which is called when a read request is received from the
 * master.
 * @param config Pointer to the target configuration.
 * @param val Pointer to the byte to be sent to the master.
 */
static int pwr_lmt_target_read_requested_cb(struct i2c_target_config *config,
                                        uint8_t *val) {
    printk("pwr_lmt target read request: 0x%02x\n", *val);
    *val = 0x42;
    return 0;
}

/*
 * @brief Callback which is called when a read is processed from the master.
 * @param config Pointer to the target configuration.
 * @param val Pointer to the next byte to be sent to the master.
 */
static int pwr_lmt_target_read_processed_cb(struct i2c_target_config *config,
                                        uint8_t *val) {
    printk("pwr_lmt target read processed: 0x%02x\n", *val);
    *val = 0x43;
    return 0;
}

/*
 * @brief Callback which is called when the master sends a stop condition.
 * @param config Pointer to the target configuration.
 */
static int pwr_lmt_target_stop_cb(struct i2c_target_config *config) {
    printk("pwr_lmt target stop callback\n");
    return 0;
}

static struct i2c_target_callbacks pwr_lmt_target_callbacks = {
    .write_requested = pwr_lmt_target_write_requested_cb,
    .write_received = pwr_lmt_target_write_received_cb,
    .read_requested = pwr_lmt_target_read_requested_cb,
    .read_processed = pwr_lmt_target_read_processed_cb,
    .stop = pwr_lmt_target_stop_cb,
};

static struct i2c_target_config target_cfg = {
    .address = 0x60,
    .callbacks = &pwr_lmt_target_callbacks,
};

#include <zephyr/init.h>

static int init_config(void) {

#if DT_NODE_HAS_STATUS(DT_ALIAS(pw_limit_i2c), okay)
    if (i2c_target_register(bus, &target_cfg) < 0) {
        printk("Failed to register target\n");
        return -1;
    }
#endif

    return 0;
}

SYS_INIT(init_config, APPLICATION, 0);
