
#pragma once

#include <zephyr/kernel.h>
#include <stddef.h>
#include <sys/types.h>

/**
 * @brief Write Flash
 *
 * @note Implemented in driver (driver/flash.c), called by service (service/flash).
 *
 * @param offset Flash write offset
 * @param data Pointer to data to write (RAM)
 * @param len Write length (bytes)
 * @return 0 on success, negative error code on failure
 */
int qc_flash_write(off_t offset, const void *data, size_t len);

/**
 * @brief Read Flash
 *
 * @note Implemented in driver (driver/flash.c), called by service (service/flash)
 *       and driver (driver/storage_bootloader.c).
 *
 * @param offset Flash read offset
 * @param data Pointer to receive buffer (RAM)
 * @param len Read length (bytes)
 * @return 0 on success, negative error code on failure
 */
int qc_flash_read(off_t offset, void *data, size_t len);

/**
 * @brief Erase Flash region
 *
 * @note Implemented in driver (driver/flash.c), called by service (service/flash).
 *
 * @param offset Flash erase start offset
 * @param size Erase size (bytes, must be aligned to Page/Sector size)
 * @return 0 on success, negative error code on failure
 */
int qc_flash_erase(off_t offset, size_t size);
