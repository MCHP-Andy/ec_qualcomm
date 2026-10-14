/*
 * @Author: andy.chang
 * @Date: 2026-03-12 16:47:34
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-10-12 10:00:00
 */

/*
 * @Description: EC-AOSS interface, EC is the I2C controller and reads the
 *               AOSS registers of SoC.
 */

#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include <interface/aoss.h>

LOG_MODULE_REGISTER(aoss, LOG_LEVEL_INF);

/* 7-bit target address of SoC AOSS */
#define AOSS_I2C_ADDR 0x35

static K_MUTEX_DEFINE(aoss_lock);

static const struct i2c_dt_spec aoss = I2C_DT_SPEC_GET(DT_NODELABEL(aoss));

/*
 * @brief Read a 32-bit AOSS register.
 * Request : [0x19] [addr LSB .. MSB]
 * Response: [val LSB .. MSB]
 */
static int aoss_reg_read(uint32_t addr, uint32_t *val) {
    uint8_t req[AOSS_REQ_LEN];
    uint8_t resp[AOSS_RESP_LEN];
    int ret;

    if (!i2c_is_ready_dt(&aoss)) {
        return -ENODEV;
    }

    req[0] = AOSS_CMD_READ_4B;
    sys_put_le32(addr, &req[1]);

    k_mutex_lock(&aoss_lock, K_FOREVER);
    ret = i2c_write_read_dt(&aoss, req, sizeof(req), resp, sizeof(resp));
    k_mutex_unlock(&aoss_lock);

    if (ret < 0) {
        LOG_ERR("AOSS read 0x%04x failed: %d", addr, ret);
        return ret;
    }

    *val = sys_get_le32(resp);
    return 0;
}

int aoss_status_get(aoss_status_t *sta) {
    uint32_t raw = 0;
    int ret = 0;

    if (sta == NULL) return -EINVAL;

    ret = aoss_reg_read(AOSS_REG_SOC_STATUS, &raw);
    if (ret < 0) return ret;

    aoss_status_parse(raw, sta);
    return 0;
}

int aoss_temp_get(int16_t *temp_c, int16_t *temp_dc) {
    uint32_t raw = 0;
    int16_t dc;
    int ret;

    ret = aoss_reg_read(AOSS_REG_SOC_AVG_TEMP, &raw);
    if (ret < 0) return ret;

    dc = aoss_temp_to_dc(raw);
    if (temp_dc) *temp_dc = dc;
    if (temp_c) *temp_c = aoss_dc_to_c(dc);
    return 0;
}
