/*
 * @Author: andy.chang 
 * @Date: 2026-04-23 00:26:28 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-23 00:28:53
 */

/*
 * @Description: Simulator driver for Fan and Thermal sensors
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>
#include <stdlib.h>

#include <interface/acpi.h>

LOG_MODULE_REGISTER(sim_dev, LOG_LEVEL_DBG);

int acpi_resp_set(uint8_t *pdata, uint16_t len) {
    LOG_INF("[SIM] ACPI data len: %d", len);

    // Dump acpi data
    LOG_HEXDUMP_INF(pdata, len, "[SIM] ACPI data :");

    return 0;
}

#if 0
static const struct device *flash_dev = DEVICE_DT_GET(DT_NODELABEL(flash0));

int qc_flash_write(off_t offset, const void *data, size_t len) {
    if (!device_is_ready(flash_dev)) {
        return -ENODEV;
    }
    return flash_write(flash_dev, offset, data, len);
}

int qc_flash_read(off_t offset, void *data, size_t len) {
    if (!device_is_ready(flash_dev)) {
        return -ENODEV;
    }
    return flash_read(flash_dev, offset, data, len);
}

int qc_flash_erase(off_t offset, size_t size) {
    if (!device_is_ready(flash_dev)) {
        return -ENODEV;
    }
    return flash_erase(flash_dev, offset, size);
}

#else
static uint8_t f_data[0x2000];

int qc_flash_write(off_t offset, const void *data, size_t len) {
    LOG_INF("Flash write: offset=%ld, len=%ld", (long)offset, (long)len);
    memcpy(&f_data[offset%0x2000], data, len);
    return 0;
}

int qc_flash_read(off_t offset, void *data, size_t len) {
    LOG_INF("Flash read: offset=%ld, len=%ld", (long)offset, (long)len);
    memcpy(data, &f_data[offset%0x2000], len);
    return 0;
}

int qc_flash_erase(off_t offset, size_t size) {
    LOG_INF("Flash erase: offset=%ld, len=%ld", (long)offset, (long)size);
    memset(&f_data[offset%0x2000], 0xFF, size);
    return 0;
}
#endif
