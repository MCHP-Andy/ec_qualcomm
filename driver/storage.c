#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include <zephyr/init.h>
#include <zephyr/devicetree.h>
#include <zephyr/retention/retention.h>
#include <zephyr/logging/log.h>

#include <interface/storage.h>

LOG_MODULE_REGISTER(storage, LOG_LEVEL_INF);

static ec_boot_info_t info = {0};

ec_boot_info_t *ec_boot_info_get(void) {
    return &info;
}

#if DT_NODE_EXISTS(DT_NODELABEL(boot_info0)) && defined(CONFIG_RETENTION)

static const struct device *boot_info = DEVICE_DT_GET(DT_NODELABEL(boot_info0));

int ec_boot_info_save(void) {
    int ret = retention_write(boot_info, 0, (uint8_t *)&info, sizeof(info));
    if (ret < 0) {
        LOG_ERR("Boot info write failed: %d", ret);
    }
    return ret;
}

int ec_boot_info_set_reset_reason(uint8_t reason) {
    info.reset_reason = reason;
    return ec_boot_info_save();
}

static int init_config(void) {
    if (!device_is_ready(boot_info)) {
        LOG_ERR("Boot info device not ready");
        return -EINVAL;
    }

    /* Bootloader 已於開機時更新 boot info，App 端僅讀取 */
    if (retention_is_valid(boot_info) == 1 &&
        retention_read(boot_info, 0, (uint8_t *)&info, sizeof(info)) == 0 &&
        info.version == EC_BOOT_INFO_VERSION) {
        LOG_INF("Boot info: mode=0x%02X reset=0x%02X fail=%u corrupt=0x%02X "
                "crc=0x%02X/0x%08X",
                info.boot_mode, info.last_reset_reason, info.boot_fail_count,
                info.corruption_status, info.last_crc_status,
                info.last_crc_value);
        return 0;
    }

    LOG_WRN("Retention data is invalid, initializing...");
    memset(&info, 0, sizeof(info));
    info.version = EC_BOOT_INFO_VERSION;
    info.boot_mode = EC_BOOT_MODE_NORMAL;
    info.reset_reason = EC_RESET_REASON_UNKNOWN;
    info.last_reset_reason = EC_RESET_REASON_POR;
    info.corruption_status = FW_CORRUPT_STATUS_VALID |
                             FW_CORRUPT_ACT_MAIN_INTACT |
                             FW_CORRUPT_BK_MAIN_INTACT |
                             FW_CORRUPT_ACT_BOOT_INTACT;
    info.last_crc_status = EC_CRC_STATUS_PASSED;
    ec_boot_info_save();

    return 0;
}

SYS_INIT(init_config, APPLICATION, 0);

#else /* 此 board 未配置 boot_info0 retention 區域 */

int ec_boot_info_save(void) {
    return -ENOTSUP;
}

int ec_boot_info_set_reset_reason(uint8_t reason) {
    info.reset_reason = reason;
    return -ENOTSUP;
}

#endif
