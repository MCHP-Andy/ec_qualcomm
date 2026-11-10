

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <interface/system.h>
#include <interface/flash.h>
#include <interface/flash_service.h>
#include <interface/storage.h>
#include <zephyr/retention/retention.h>
#include <zephyr/sys/crc.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(header, LOG_LEVEL_INF);

/* -------------------------------------------------------------------------
 * Constants and macro definitions
 * ------------------------------------------------------------------------- */
// #define EC_FW_MAGIC_VALUE               (0x57464345) // 'E', 'C', 'F', 'W' (Little-Endian)
#define EC_FW_MAGIC_BYTE0               'E'          // 0x45
#define EC_FW_MAGIC_BYTE1               'C'          // 0x43
#define EC_FW_MAGIC_BYTE2               'F'          // 0x46
#define EC_FW_MAGIC_BYTE3               'W'          // 0x57

/* Stay in Bootloader waiting for Host recovery after this many consecutive validation failures */
#define EC_BOOT_FAIL_LIMIT              (3)
#define FLASH_LOAD_CHUNK_SIZE           512

/* Image header / Image address (Get from DTS) */
#if defined(CONFIG_BOARD_QEMU_CORTEX_M3)
#define HDR_ADDR (DT_REG_ADDR(DT_NODELABEL(flash0))+DT_REG_ADDR(DT_NODELABEL(hdr_partition)))
#define IMG_ADDR (DT_REG_ADDR(DT_NODELABEL(flash0))+DT_REG_ADDR(DT_NODELABEL(app_partition)))
#elif defined(CONFIG_BOARD_QC_MODULE)
#define HDR_ADDR (DT_REG_ADDR(DT_NODELABEL(int_flash))+DT_REG_ADDR(DT_NODELABEL(hdr_partition)))
#define IMG_ADDR (DT_REG_ADDR(DT_NODELABEL(int_flash))+DT_REG_ADDR(DT_NODELABEL(img_partition)))
/* App execution address (RAM) */
#define RAM_ADDR (DT_REG_ADDR(DT_NODELABEL(flash0))+DT_REG_ADDR(DT_NODELABEL(app_partition)))
#endif

static const struct device *boot_info = DEVICE_DT_GET(DT_NODELABEL(boot_info0));
static ec_boot_info_t info = {0};
/* Buffer for loading image into RAM */
/* ec_cal_image_crc / ec_cal_image_checksum temporary buffer */
static uint8_t crc_buf[512] = {0};

static uint32_t cal_checksum(uint32_t checksum, const uint8_t *const buffer,
                             const uint16_t size) {
    for (size_t i = 0; i < size; ++i) {
        checksum += buffer[i];
    }
    return checksum;
}

/* Read a block of data from Flash */
static int read_flash(off_t addr, uint8_t *data, size_t len) {
#if defined(CONFIG_BOARD_QEMU_CORTEX_M3)
    memcpy(data, (const uint8_t *)addr, len);
    return 0;
#elif defined(CONFIG_BOARD_QC_MODULE)
    int ret = qc_flash_read(addr, data, len);
    if (ret < 0) {
        LOG_ERR("Flash read failed at 0x%08X: %d", (uint32_t)addr, ret);
    }
    return ret;
#else
#error "Unsupported board"
#endif
}

int ec_get_image_header(fw_hdr_t *hdr) {
    if (hdr == NULL) {
        return -EINVAL;
    }

    // Read header from flash
    return read_flash((off_t)HDR_ADDR, (uint8_t *)hdr, sizeof(fw_hdr_t));
}

int ec_cal_image_crc(off_t addr, size_t size, uint32_t *crc) {
    if (crc == NULL) {
        return -EINVAL;
    }

    *crc = 0;

    // Read image and calculate CRC8 (CCITT, poly 0x07, init 0x00)
    uint8_t crc8 = 0;
    for (size_t a = 0; a < size; a += sizeof(crc_buf)) {
        size_t chunk_size = MIN(size - a, sizeof(crc_buf));
        int ret = read_flash(addr + a, crc_buf, chunk_size);
        if (ret < 0) {
            return ret;
        }

        crc8 = crc8_ccitt(crc8, crc_buf, chunk_size);
    }

    // CRC8 is stored zero-extended in the 4-byte field
    *crc = crc8;

    return 0;
}

int ec_cal_image_checksum(off_t addr, size_t size, uint32_t *checksum) {
    if (checksum == NULL) {
        return -EINVAL;
    }

    *checksum = 0;

    // Read image and calculate checksum
    for (size_t a = 0; a < size; a += sizeof(crc_buf)) {
        size_t chunk_size = MIN(size - a, sizeof(crc_buf));
        int ret = read_flash(addr + a, crc_buf, chunk_size);
        if (ret < 0) {
            return ret;
        }

        *checksum = cal_checksum(*checksum, crc_buf, chunk_size);
    }

    return 0;
}

#if defined(CONFIG_BOARD_QC_MODULE)
/* Re-read Flash image and write it to the RAM execution address */
static int load_image_to_ram(off_t addr, size_t size) {
    uint8_t *img_addr = (uint8_t *)RAM_ADDR;

    LOG_INF("Load image: Flash 0x%08X -> RAM 0x%08X, size %u bytes",
            (uint32_t)addr, (uint32_t)img_addr, (uint32_t)size);

    for (size_t a = 0; a < size; a += FLASH_LOAD_CHUNK_SIZE) {
        size_t chunk_size = MIN(size - a, (size_t)FLASH_LOAD_CHUNK_SIZE);
        int ret = flash_service_request(FLASH_SERVICE_LOAD_TO_RAM, addr + a,
                                        chunk_size, img_addr + a, NULL, NULL);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}
#endif

ec_boot_info_t *ec_boot_info_get(void) {
    return &info;
}

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

/* Record image validation failure: update Corruption Status and failure count */
static void boot_info_mark_fail(void) {
    info.corruption_status &= ~FW_CORRUPT_ACT_MAIN_INTACT;
    if (info.boot_fail_count < UINT8_MAX) {
        info.boot_fail_count++;
    }
    if (info.boot_fail_count >= EC_BOOT_FAIL_LIMIT) {
        LOG_WRN("Boot fail count %u reached limit, stay in bootloader",
                info.boot_fail_count);
        info.boot_mode = EC_BOOT_MODE_STAY_IN_BBK;
    }
    ec_boot_info_save();
}

/**
 * @brief Validate the EC firmware header
 * @param hdr Pointer to the 32B header read from Flash
 * @return true if header is well-formed and valid; false if corrupted or invalid
 */
bool ec_validate_image_header(const fw_hdr_t *hdr)
{
    if (hdr == NULL) {
        return false;
    }

    /* 1. Check Magic Value ('E', 'C', 'F', 'W') */
    /* Compare byte by byte: a 32-bit compare against EC_FW_MAGIC_VALUE puts
     * the literal "ECFW" into the bootloader binary, which tools scanning the
     * image for the main FW header would mistake for the real header. */
    if (hdr->magic.bytes[0] != EC_FW_MAGIC_BYTE0 ||
        hdr->magic.bytes[1] != EC_FW_MAGIC_BYTE1 ||
        hdr->magic.bytes[2] != EC_FW_MAGIC_BYTE2 ||
        hdr->magic.bytes[3] != EC_FW_MAGIC_BYTE3) {
        LOG_ERR("Invalid EC FW Header Magic Value: 0x%08X", hdr->magic.u32);
        boot_info_mark_fail();
        return false;
    }

    /* 2. Check image size validity (e.g. must not be 0 or exceed the 464KB partition limit) */
    if (hdr->fw_size == 0 || hdr->fw_size > (464 * 1024)) {
        LOG_ERR("Invalid EC FW Image Size: %u bytes", hdr->fw_size);
        boot_info_mark_fail();
        return false;
    }

    /* 3. Check validation control flag */
    if (hdr->validation_ctrl > EC_FW_VALIDATION_CHECKSUM) {
        LOG_ERR("Invalid EC FW Validation Control: 0x%02X",
                hdr->validation_ctrl);
        boot_info_mark_fail();
        return false;
    }

    /* 4. Calculate CRC / Checksum on Flash first */
    LOG_INF("Flash Address: 0x%08X", (uint32_t)IMG_ADDR);
    if (hdr->validation_ctrl != EC_FW_VALIDATION_NONE) {
        bool use_checksum = (hdr->validation_ctrl & EC_FW_VALIDATION_CHECKSUM);
        uint32_t expected = use_checksum ? hdr->check.checksum : hdr->check.crc;
        uint32_t value = 0;
        int ret = use_checksum
                      ? ec_cal_image_checksum((off_t)IMG_ADDR, hdr->fw_size,
                                              &value)
                      : flash_service_request(FLASH_SERVICE_CRC,
                                              (off_t)IMG_ADDR, hdr->fw_size,
                                              NULL, NULL, &value);

        info.last_crc_value = value;
        info.last_crc_status = (ret == 0 && value == expected)
                                   ? EC_CRC_STATUS_PASSED
                                   : EC_CRC_STATUS_FAILED;

        if (ret < 0) {
            LOG_ERR("EC FW %s calculation failed: %d",
                    use_checksum ? "Checksum" : "CRC", ret);
            boot_info_mark_fail();
            return false;
        }

        if (value != expected) {
            LOG_ERR("EC FW %s Mismatch: Calculated 0x%08X, Expected 0x%08X",
                    use_checksum ? "Checksum" : "CRC", value, expected);
            boot_info_mark_fail();
            return false;
        }
    }

    // Print EC Image info
    LOG_INF("EC Load Address: 0x%06X", hdr->load_address);
    LOG_INF("EC Firmware Size: %u bytes", hdr->fw_size);
    LOG_INF("EC Firmware Version: %u.%u.%u", hdr->version.major, hdr->version.minor, hdr->version.test);
    LOG_INF("EC Validation Control: 0x%02X", hdr->validation_ctrl);
    LOG_INF("EC Lowest Supported Version: %u.%u.%u", hdr->lowest_supported_version.major, hdr->lowest_supported_version.minor, hdr->lowest_supported_version.test);

    if (info.boot_mode == EC_BOOT_MODE_STAY_IN_BBK) {
        LOG_INF("Stay in bootloader mode, skipping jump to app");
        return false;
    }

    /* 5. Only after validation passes, re-read Flash image and write to RAM */
#if defined(CONFIG_BOARD_QC_MODULE)
    if (load_image_to_ram((off_t)IMG_ADDR, hdr->fw_size) < 0) {
        LOG_ERR("Failed to load EC FW image to RAM");
        boot_info_mark_fail();
        return false;
    }
#endif

    // Load EC jump address
    info.boot_mode = EC_BOOT_MODE_NORMAL;
    info.boot_fail_count = 0;
    info.corruption_status |= FW_CORRUPT_ACT_MAIN_INTACT;
    ec_boot_info_save();
    jump_to_app((uint32_t)hdr->load_address);

    return true;
}



#include <zephyr/init.h>

static int init_config(void) {
    if (!device_is_ready(boot_info)) {
        LOG_ERR("Boot info device not ready");
        return -EINVAL;
    }

    if (retention_is_valid(boot_info) == 1 &&
        retention_read(boot_info, 0, (uint8_t *)&info, sizeof(info)) == 0 &&
        info.version == EC_BOOT_INFO_VERSION) {
        info.last_reset_reason = info.reset_reason;
        LOG_INF("Boot info: mode=0x%02X reset=0x%02X fail=%u corrupt=0x%02X "
                "crc=0x%02X/0x%08X",
                info.boot_mode, info.last_reset_reason, info.boot_fail_count,
                info.corruption_status, info.last_crc_status,
                info.last_crc_value);
    } else {
        LOG_WRN("Retention data is invalid, initializing...");
        memset(&info, 0, sizeof(info));
        info.version = EC_BOOT_INFO_VERSION;
        info.boot_mode = EC_BOOT_MODE_NORMAL;
        info.last_reset_reason = EC_RESET_REASON_POR;
        info.corruption_status = FW_CORRUPT_STATUS_VALID |
                                 FW_CORRUPT_ACT_MAIN_INTACT |
                                 FW_CORRUPT_ACT_BOOT_INTACT;
        info.last_crc_status = EC_CRC_STATUS_PASSED;
    }

    info.status = EC_STATE_BOOTLOADER_IDLE_WP_OFF;

    /* Resets not recorded by software (WDT / hardware) read as UNKNOWN on next boot */
    info.reset_reason = EC_RESET_REASON_UNKNOWN;
    ec_boot_info_save();

    return 0;
}

SYS_INIT(init_config, APPLICATION, SYS_INIT_NORMAL);
