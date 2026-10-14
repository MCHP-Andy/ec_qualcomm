#include <interface/flash.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/init.h>

static const struct device *flash_dev = DEVICE_DT_GET(DT_NODELABEL(int_flash));

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
