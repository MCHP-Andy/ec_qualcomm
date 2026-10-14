
#pragma once

#include <zephyr/kernel.h>
#include <stddef.h>
#include <sys/types.h>

/**
 * @brief 寫入 Flash
 * @param offset Flash 寫入偏移位址
 * @param data 要寫入的資料指標 (RAM)
 * @param len 寫入長度 (bytes)
 * @return 0 成功，負數表示錯誤碼
 */
int qc_flash_write(off_t offset, const void *data, size_t len);

/**
 * @brief 讀取 Flash
 * @param offset Flash 讀取偏移位址
 * @param data 接收資料的緩衝區指標 (RAM)
 * @param len 讀取長度 (bytes)
 * @return 0 成功，負數表示錯誤碼
 */
int qc_flash_read(off_t offset, void *data, size_t len);

/**
 * @brief 抹除 Flash 區塊
 * @param offset Flash 抹除起始偏移位址
 * @param size 抹除大小 (bytes，須對齊 Page/Sector 大小)
 * @return 0 成功，負數表示錯誤碼
 */
int qc_flash_erase(off_t offset, size_t size);
