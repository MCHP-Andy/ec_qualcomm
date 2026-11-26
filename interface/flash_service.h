#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef enum {
    FLASH_SERVICE_WRITE,
    FLASH_SERVICE_READ,
    FLASH_SERVICE_ERASE,
    FLASH_SERVICE_CRC,
    FLASH_SERVICE_LOAD_TO_RAM,
} flash_service_command_t;

/**
 * @brief Queue a flash operation and wait for its completion.
 *
 * Must not be called from the flash service thread (including a done
 * callback).
 *
 * @note Implemented in service (service/flash), called by driver
 *       (driver/storage_bootloader.c).
 *
 * @param command Operation to execute.
 * @param flash_address Flash address used by the operation.
 * @param size Number of bytes to process.
 * @param ram_address Destination RAM address for LOAD_TO_RAM.
 * @param data_pointer Source for WRITE or destination for READ.
 * @param crc_result Receives the CRC8 result (zero-extended) for CRC.
 * @return 0 on success, or a negative error code.
 */
int flash_service_request(flash_service_command_t command,
                          off_t flash_address, size_t size,
                          void *ram_address, void *data_pointer,
                          uint32_t *crc_result);

/**
 * @brief Completion callback of flash_service_submit().
 *
 * Runs in the flash service thread after the operation finished, before the
 * EC status leaves BUSY.
 *
 * @param command   Operation that finished.
 * @param result    0 on success, or a negative error code.
 * @param crc       CRC8 result (zero-extended) for FLASH_SERVICE_CRC, else 0.
 * @param user_data Pointer passed to flash_service_submit().
 */
typedef void (*flash_service_done_cb_t)(flash_service_command_t command,
                                        int result, uint32_t crc,
                                        void *user_data);

/**
 * @brief Queue a flash operation without waiting for it.
 *
 * Returns as soon as the request is queued. The EC status (boot info, read by
 * ACPI 0xEC) is EC_STATE_BUSY from this call until every queued operation has
 * finished, then EC_STATE_BOOTLOADER_IDLE_WP_OFF, or EC_STATE_SPI_OP_FAILED if
 * one of them failed. The caller keeps @p ram_address / @p data_pointer valid
 * until @p done is called (or the status is no longer busy).
 *
 * @note Implemented in service (service/flash), called by service
 *       (service/acpi/ec_fw_update_bootloader.c).
 *
 * @param command       Operation to execute.
 * @param flash_address Flash address used by the operation.
 * @param size          Number of bytes to process.
 * @param ram_address   Destination RAM address for LOAD_TO_RAM.
 * @param data_pointer  Source for WRITE or destination for READ.
 * @param done          Completion callback, may be NULL.
 * @param user_data     Passed to @p done.
 * @retval 0 queued.
 * @retval -EINVAL invalid parameters.
 * @retval -ENOMEM request queue is full.
 */
int flash_service_submit(flash_service_command_t command, off_t flash_address,
                         size_t size, void *ram_address, void *data_pointer,
                         flash_service_done_cb_t done, void *user_data);

/**
 * @brief Whether a flash operation is queued or running.
 *
 * @note Implemented in service (service/flash), called by service
 *       (service/acpi/ec_fw_update_bootloader.c).
 *
 * @retval true  at least one operation has not finished yet.
 * @retval false the flash service is idle.
 */
bool flash_service_busy(void);
