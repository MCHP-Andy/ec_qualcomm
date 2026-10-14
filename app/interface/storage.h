#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/sys/util.h>
#include <zephyr/toolchain.h>

/* 與 bootloader/interface/storage.h 的 boot info 定義保持一致 */
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
