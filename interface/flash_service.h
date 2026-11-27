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
 * @brief Completion callback of flash_service_submit().
 *
 * Runs in the flash service thread after the operation finished, before the
 * EC status leaves BUSY.
 *
 * @param command   Operation that finished.
 * @param result    0 on success, or a negative error code.
 * @param crc       CRC8 result (zero-extended) for FLASH_SERVICE_CRC, else 0.
 * @param user_data flash_service_msg_t::user_data.
 */
typedef void (*flash_service_done_cb_t)(flash_service_command_t command,
                                        int result, uint32_t crc,
                                        void *user_data);

/**
 * @brief Flash service request, filled in by the caller.
 *
 * The service copies it into its queue, so the caller's copy may live on the
 * stack. The buffers it points to must stay valid until the operation is done.
 */
typedef struct flash_service_msg_t {
    flash_service_command_t command;
    off_t flash_address;           /**< Flash address used by the operation */
    size_t size;                   /**< Number of bytes to process */
    void *ram_address;             /**< Destination RAM address for LOAD_TO_RAM */
    void *data_pointer;            /**< Source for WRITE, destination for READ */
    flash_service_done_cb_t done;  /**< Async completion callback, may be NULL */
    void *user_data;               /**< Passed to @ref done */
} flash_service_msg_t;

/**
 * @brief Queue a flash operation without waiting for it.
 *
 * Returns as soon as the request is queued. The EC status (boot info, read by
 * ACPI 0xEC) is EC_STATE_BUSY from this call until every queued operation has
 * finished, then EC_STATE_BOOTLOADER_IDLE_WP_OFF, also when one of them
 * failed (a failure is only logged and passed to msg->done). msg->done (if
 * set) is called when this one finished.
 *
 * @note Implemented in service (service/flash), called by service
 *       (service/acpi/ec_fw_update.c).
 *
 * @param msg Request, copied into the queue.
 * @retval 0 queued.
 * @retval -EINVAL invalid request.
 * @retval -ENOMEM request queue is full.
 */
int flash_service_submit(const flash_service_msg_t *msg);

/**
 * @brief Queue a flash operation and wait for its completion.
 *
 * msg->done / msg->user_data are ignored. Must not be called from the flash
 * service thread (including a done callback).
 *
 * @note Implemented in service (service/flash), called by driver
 *       (driver/storage.c).
 *
 * @param msg        Request.
 * @param crc_result Receives the CRC8 result (zero-extended) for
 *                   FLASH_SERVICE_CRC, may be NULL for other commands.
 * @return 0 on success, or a negative error code.
 */
int flash_service_request(const flash_service_msg_t *msg, uint32_t *crc_result);

/**
 * @brief Whether a flash operation is queued or running.
 *
 * @note Implemented in service (service/flash), called by service
 *       (service/acpi/ec_fw_update.c).
 *
 * @retval true  at least one operation has not finished yet.
 * @retval false the flash service is idle.
 */
bool flash_service_busy(void);
