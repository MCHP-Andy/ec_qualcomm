#pragma once

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
 * @param command Operation to execute.
 * @param flash_address Flash address used by the operation.
 * @param size Number of bytes to process.
 * @param ram_address Destination RAM address for LOAD_TO_RAM.
 * @param data_pointer Source for WRITE or destination for READ.
 * @param crc_result Receives the CRC32 result for CRC.
 * @return 0 on success, or a negative error code.
 */
int flash_service_request(flash_service_command_t command,
                          off_t flash_address, size_t size,
                          void *ram_address, void *data_pointer,
                          uint32_t *crc_result);
