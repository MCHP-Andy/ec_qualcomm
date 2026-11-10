#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/sys/util.h>
#include <zephyr/toolchain.h>

#if defined(CONFIG_APP_BOOTLOADER)
#include <stddef.h>
#include <sys/types.h>

#define EC_FW_HEADER_SIZE               (32)

/**
 * @brief Validation algorithm control flag (Validation Control)
 */
typedef enum {
    EC_FW_VALIDATION_NONE     = 0x0,  /**< Disable CRC and Checksum validation */
    EC_FW_VALIDATION_CRC      = 0x1,  /**< Enable CRC validation (default) */
    EC_FW_VALIDATION_CHECKSUM = 0x2,  /**< Enable Checksum validation */
} ec_fw_validation_ctrl_t;

/**
 * @brief Firmware version structure (3 Bytes: Major.Minor.Test)
 */
struct __attribute__((packed)) ec_fw_ver {
    uint8_t major;  /**< Major Version */
    uint8_t minor;  /**< Minor Version */
    uint8_t test;   /**< Test/patch version (Test Version) */
};

/**
 * @brief Qualcomm Glymur 32-byte main firmware image header
 * Located at the start of the Active and Backup Partitions (Offset 0x00)
 */
typedef struct __attribute__((packed)) fw_hdr_t {
    /* Offset 0x00 - 0x03: Magic Number ('E', 'C', 'F', 'W') */
    union {
        uint32_t u32;
        uint8_t  bytes[4];
    } magic;

    /* Offset 0x04 - 0x07: Target load address in EC RAM (Little-Endian) */
    uint32_t load_address;

    /* Offset 0x08 - 0x0B: Firmware length (excluding this 32B header) */
    uint32_t fw_size;

    /* Offset 0x0C - 0x0E: Firmware version (Major.Minor.Test) */
    struct ec_fw_ver version;

    /* Offset 0x0F: CRC / Checksum validation mode flag */
    uint8_t  validation_ctrl;

    /* Offset 0x10 - 0x12: Minimum supported version (anti-rollback) */
    struct ec_fw_ver lowest_supported_version;

    /* Offset 0x13: Reserved field 1 */
    uint8_t  reserved_1;

    /* Offset 0x14 - 0x17: CRC32 / Checksum of the firmware body */
    union {
        uint32_t checksum;
        uint32_t crc32;
    } check;

    /* Offset 0x18 - 0x19: Appended data size (e.g. size of Fan LUT following the firmware) */
    uint16_t aux_data_size;

    /* Offset 0x1A - 0x1F: Reserved field 2 */
    uint8_t  reserved_2[6];
} fw_hdr_t ;

/* -------------------------------------------------------------------------
 * Compile-time static size assertion (ensures the struct is exactly 32 bytes)
 * ------------------------------------------------------------------------- */
#if defined(BUILD_ASSERT)
BUILD_ASSERT(sizeof(fw_hdr_t) == EC_FW_HEADER_SIZE,
             "fw_header size must be exactly 32 bytes!");
#endif

/**
 * @brief Validate the Active Main image and jump to the App if it is valid.
 *
 * Checks magic, image size and validation control, then verifies CRC32 or
 * Checksum on Flash and records the result in boot info. On success (and
 * when not in EC_BOOT_MODE_STAY_IN_BBK) the image is loaded to RAM and the
 * function jumps to the App without returning. Each failure increments the
 * boot fail count.
 *
 * @note Implemented in driver (driver/storage_bootloader.c), called by
 *       bootloader (bootloader/main.c) and service
 *       (service/acpi/ec_fw_update_bootloader.c).
 *
 * @param hdr Pointer to the 32B header read from Flash.
 * @return false if the header or image is invalid, or when staying in
 *         Bootloader. Does not return on success.
 */
bool ec_validate_image_header(const fw_hdr_t *hdr);

/**
 * @brief Read the Active Main image header from Flash
 *
 * @note Implemented in driver (driver/storage_bootloader.c), called by
 *       bootloader (bootloader/main.c) and service
 *       (service/acpi/ec_fw_update_bootloader.c).
 *
 * @param hdr Buffer to receive the 32B header
 * @return 0 on success, negative error code on failure
 */
int ec_get_image_header(fw_hdr_t *hdr);

/**
 * @brief Calculate CRC32 over the specified Flash region
 *
 * @note Implemented in driver (driver/storage_bootloader.c), called by
 *       service (service/flash).
 *
 * @param addr Flash start address
 * @param size Length to calculate (bytes)
 * @param crc Output CRC32 value
 * @return 0 on success, negative error code on failure
 */
int ec_cal_image_crc(off_t addr, size_t size, uint32_t *crc);

/**
 * @brief Calculate Checksum over the specified Flash region (byte-wise sum)
 *
 * @note Implemented in driver (driver/storage_bootloader.c), called by
 *       driver (driver/storage_bootloader.c).
 *
 * @param addr Flash start address
 * @param size Length to calculate (bytes)
 * @param checksum Output Checksum value
 * @return 0 on success, negative error code on failure
 */
int ec_cal_image_checksum(off_t addr, size_t size, uint32_t *checksum);
#endif /* CONFIG_APP_BOOTLOADER */

/* -------------------------------------------------------------------------
 * App <-> Bootloader data exchange (stored in retention boot_info0 region)
 * Magic and Checksum are handled by the retention node prefix / checksum, not duplicated in the struct
 * ------------------------------------------------------------------------- */
#define EC_BOOT_INFO_VERSION                (1)

/* Boot Mode */
#define EC_BOOT_MODE_NORMAL                 (0x00) /**< Jump to App after self-check passes */
#define EC_BOOT_MODE_STAY_IN_BBK            (0xDC) /**< Stay in Bootloader (0xBB 0xDC) */

/* Reset Reason */
#define EC_RESET_REASON_UNKNOWN             (0x00) /**< Reset not recorded by software (WDT / hardware) */
#define EC_RESET_REASON_POR                 (0x01) /**< Power-on cold boot (retention invalid) */
#define EC_RESET_REASON_HOST_REQ            (0x02) /**< Host-requested reset (0xBB 0xCC) */
#define EC_RESET_REASON_WDT                 (0x03) /**< Watchdog timeout (0xBB 0xC2) */
#define EC_RESET_REASON_PANIC               (0x04) /**< Oops / Assert (0xBB 0xC0~C1) */
#define EC_RESET_REASON_ENTER_BBK           (0x05) /**< Reset triggered by entering Bootloader */

/* 0xEC EC State Status */
#define EC_STATE_BOOTLOADER_IDLE_WP_OFF     0x0B
#define EC_STATE_BOOTLOADER_IDLE_WP_ON      0x1B
#define EC_STATE_MAIN_IDLE_WP_OFF           0x0C
#define EC_STATE_MAIN_IDLE_WP_ON            0x1C
#define EC_STATE_SPI_OP_FAILED              0xFA
#define EC_STATE_BUSY                       0xFF

/* 0xEB Corruption Status Bits */
#define FW_CORRUPT_ACT_BOOT_INTACT          BIT(0)
#define FW_CORRUPT_BK_BOOT_INTACT           BIT(1)
#define FW_CORRUPT_ACT_MAIN_INTACT          BIT(2)
#define FW_CORRUPT_BK_MAIN_INTACT           BIT(3)
#define FW_CORRUPT_INT_MAIN_INTACT          BIT(4)
#define FW_CORRUPT_STATUS_VALID             BIT(7)

/* 0xA2 CRC Status */
#define EC_CRC_STATUS_PASSED                0xAC
#define EC_CRC_STATUS_FAILED                0xFA

typedef struct ec_boot_info_t {
    uint8_t  version;           /**< Structure version (EC_BOOT_INFO_VERSION) */
    uint8_t  boot_mode;         /**< Boot mode (EC_BOOT_MODE_*) */
    uint8_t  reset_reason;      /**< Reset reason written before reset (EC_RESET_REASON_*) */
    uint8_t  last_reset_reason; /**< Reset reason of this boot, filled in by Bootloader at boot */
    uint8_t  boot_fail_count;   /**< Consecutive image validation failure count */
    uint8_t  corruption_status; /**< 0xEB Corruption Status flags */
    uint8_t  last_crc_status;   /**< 0xA2 last CRC status */
    volatile uint8_t status;    /**< 0xEC EC status; BUSY during Flash operations */
    uint32_t last_crc_value;    /**< 0xA2 last CRC32 / Checksum value */
} ec_boot_info_t;

#if defined(BUILD_ASSERT)
BUILD_ASSERT(sizeof(ec_boot_info_t) == 12, "ec_boot_info_t layout changed!");
#endif

/**
 * @brief Get the RAM copy of boot info (call ec_boot_info_save after modifying)
 *
 * @note Implemented in driver (driver/storage.c, driver/storage_bootloader.c),
 *       called by app (app/main.c) and service (service/acpi, service/flash).
 *
 * @return Pointer to the boot info RAM copy, never NULL.
 */
ec_boot_info_t *ec_boot_info_get(void);

/**
 * @brief Write the RAM copy back to retention
 *
 * @note Implemented in driver (driver/storage.c, driver/storage_bootloader.c),
 *       called by app (app/main.c) and service (service/acpi).
 *
 * @return 0 on success, negative error code on failure
 */
int ec_boot_info_save(void);

/**
 * @brief Record reset reason and write back to retention; call before triggering reset
 *
 * @note Implemented in driver (driver/storage.c, driver/storage_bootloader.c),
 *       called by service (service/acpi).
 *
 * @param reason Reset reason (EC_RESET_REASON_*).
 * @return 0 on success, negative error code on failure
 */
int ec_boot_info_set_reset_reason(uint8_t reason);
