
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include <zephyr/sys/util.h>
#include <zephyr/toolchain.h>

#define EC_FW_HEADER_SIZE               (32)

/**
 * @brief 校驗演算法控制旗標 (Validation Control)
 */
typedef enum {
    EC_FW_VALIDATION_NONE     = 0x0,  /**< 停用 CRC 與 Checksum 校驗 */
    EC_FW_VALIDATION_CRC      = 0x1,  /**< 啟用 CRC 校驗 (預設) */
    EC_FW_VALIDATION_CHECKSUM = 0x2,  /**< 啟用 Checksum 校驗 */
} ec_fw_validation_ctrl_t;

/**
 * @brief 韌體版本號結構 (3 Bytes: Major.Minor.Test)
 */
struct __attribute__((packed)) ec_fw_ver {
    uint8_t major;  /**< 主版本號 (Major Version) */
    uint8_t minor;  /**< 次版本號 (Minor Version) */
    uint8_t test;   /**< 測試/補丁版本號 (Test Version) */
};

/**
 * @brief Qualcomm Glymur 32 位元組主韌體映像檔標頭
 * 位於 Active 與 Backup Partition 的最前端 (Offset 0x00)
 */
typedef struct __attribute__((packed)) fw_hdr_t {
    /* Offset 0x00 - 0x03: Magic Number ('E', 'C', 'F', 'W') */
    union {
        uint32_t u32;
        uint8_t  bytes[4];
    } magic;

    /* Offset 0x04 - 0x07: 載入至 EC RAM 的目標位址 (Little-Endian) */
    uint32_t load_address;

    /* Offset 0x08 - 0x0B: 韌體長度 (不含此 32B 標頭) */
    uint32_t fw_size;

    /* Offset 0x0C - 0x0E: 韌體版本號 (Major.Minor.Test) */
    struct ec_fw_ver version;

    /* Offset 0x0F: CRC / Checksum 驗證模式旗標 */
    uint8_t  validation_ctrl;

    /* Offset 0x10 - 0x12: 最低支援版本 (防回滾機制) */
    struct ec_fw_ver lowest_supported_version;

    /* Offset 0x13: 保留欄位 1 */
    uint8_t  reserved_1;

    /* Offset 0x14 - 0x17: 韌體本體 CRC32 / Checksum 簽章值 */
    union {
        uint32_t checksum;
        uint32_t crc32;
    } check;

    /* Offset 0x18 - 0x19: 附加資料大小 (例如緊接於韌體後的 Fan LUT 大小) */
    uint16_t aux_data_size;

    /* Offset 0x1A - 0x1F: 保留欄位 2 */
    uint8_t  reserved_2[6];
} fw_hdr_t ;

/* -------------------------------------------------------------------------
 * 編譯期靜態大小斷言 (保證結構體長度恰為 32 位元組)
 * ------------------------------------------------------------------------- */
#if defined(BUILD_ASSERT)
BUILD_ASSERT(sizeof(fw_hdr_t) == EC_FW_HEADER_SIZE,
             "fw_header size must be exactly 32 bytes!");
#endif

bool ec_validate_image_header(const fw_hdr_t *hdr);

/**
 * @brief 從 Flash 讀取 Active Main 映像檔標頭
 * @param hdr 接收 32B 標頭的緩衝區
 * @return 0 成功，負數表示錯誤碼
 */
int ec_get_image_header(fw_hdr_t *hdr);

/**
 * @brief 計算 Flash 指定區域的 CRC32
 * @param addr Flash 起始位址
 * @param size 計算長度 (bytes)
 * @param crc 輸出 CRC32 數值
 * @return 0 成功，負數表示錯誤碼
 */
int ec_cal_image_crc(off_t addr, size_t size, uint32_t *crc);

/**
 * @brief 計算 Flash 指定區域的 Checksum (逐位元組累加)
 * @param addr Flash 起始位址
 * @param size 計算長度 (bytes)
 * @param checksum 輸出 Checksum 數值
 * @return 0 成功，負數表示錯誤碼
 */
int ec_cal_image_checksum(off_t addr, size_t size, uint32_t *checksum);

/* -------------------------------------------------------------------------
 * App <-> Bootloader 資料交換 (存放於 retention boot_info0 區域)
 * Magic 與 Checksum 由 retention 節點的 prefix / checksum 負責，結構內不重複
 * ------------------------------------------------------------------------- */
#define EC_BOOT_INFO_VERSION                (1)

/* 開機模式 (Boot Mode) */
#define EC_BOOT_MODE_NORMAL                 (0x00) /**< 自檢通過後跳轉 App */
#define EC_BOOT_MODE_STAY_IN_BBK            (0xDC) /**< 停留在 Bootloader (0xBB 0xDC) */

/* 重設原因 (Reset Reason) */
#define EC_RESET_REASON_UNKNOWN             (0x00) /**< 未經軟體記錄的重設 (WDT / 硬體) */
#define EC_RESET_REASON_POR                 (0x01) /**< 上電冷開機 (retention 無效) */
#define EC_RESET_REASON_HOST_REQ            (0x02) /**< Host 要求重設 (0xBB 0xCC) */
#define EC_RESET_REASON_WDT                 (0x03) /**< 看門狗逾時 (0xBB 0xC2) */
#define EC_RESET_REASON_PANIC               (0x04) /**< Oops / Assert (0xBB 0xC0~C1) */
#define EC_RESET_REASON_ENTER_BBK           (0x05) /**< 進入 Bootloader 所觸發的重設 */

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
    uint8_t  version;           /**< 結構版本 (EC_BOOT_INFO_VERSION) */
    uint8_t  boot_mode;         /**< 開機模式 (EC_BOOT_MODE_*) */
    uint8_t  reset_reason;      /**< 重設前寫入的重設原因 (EC_RESET_REASON_*) */
    uint8_t  last_reset_reason; /**< 本次開機的重設原因，由 Bootloader 開機時填入 */
    uint8_t  boot_fail_count;   /**< 連續映像檔驗證失敗次數 */
    uint8_t  corruption_status; /**< 0xEB Corruption Status 旗標 */
    uint8_t  last_crc_status;   /**< 0xA2 最近一次 CRC 狀態 */
    uint8_t  reserved;          /**< 保留對齊 */
    uint32_t last_crc_value;    /**< 0xA2 最近一次 CRC32 / Checksum 數值 */
} ec_boot_info_t;

#if defined(BUILD_ASSERT)
BUILD_ASSERT(sizeof(ec_boot_info_t) == 12, "ec_boot_info_t layout changed!");
#endif

/**
 * @brief 取得 boot info 的 RAM 副本 (修改後需呼叫 ec_boot_info_save)
 */
ec_boot_info_t *ec_boot_info_get(void);

/**
 * @brief 將 RAM 副本寫回 retention
 * @return 0 成功，負數表示錯誤碼
 */
int ec_boot_info_save(void);

/**
 * @brief 記錄重設原因並寫回 retention，於觸發重設前呼叫
 * @return 0 成功，負數表示錯誤碼
 */
int ec_boot_info_set_reset_reason(uint8_t reason);
