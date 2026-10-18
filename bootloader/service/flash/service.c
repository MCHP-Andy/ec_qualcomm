#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <interface/flash.h>
#include <interface/flash_service.h>
#include <interface/storage.h>

LOG_MODULE_REGISTER(flash_service, LOG_LEVEL_INF);

#define FLASH_SERVICE_QUEUE_LEN 4
#define FLASH_SERVICE_STACK_SIZE 1024
#define FLASH_SERVICE_PRIORITY 5

typedef struct {
    flash_service_command_t command;
    off_t flash_address;
    size_t size;
    void *ram_address;
    void *data_pointer;
    uint32_t *crc_result;
    int *operation_result;
    struct k_sem *completion;
} flash_service_msg_t;

K_MSGQ_DEFINE(flash_service_queue, sizeof(flash_service_msg_t),
              FLASH_SERVICE_QUEUE_LEN, 4);

static int flash_service_execute(const flash_service_msg_t *msg) {
    if (msg->flash_address < 0 || msg->size == 0) {
        return -EINVAL;
    }

    switch (msg->command) {
    case FLASH_SERVICE_WRITE:
        if (msg->data_pointer == NULL) {
            return -EINVAL;
        }
        return qc_flash_write(msg->flash_address, msg->data_pointer, msg->size);

    case FLASH_SERVICE_READ:
        if (msg->data_pointer == NULL) {
            return -EINVAL;
        }
        return qc_flash_read(msg->flash_address, msg->data_pointer, msg->size);

    case FLASH_SERVICE_ERASE:
        return qc_flash_erase(msg->flash_address, msg->size);

    case FLASH_SERVICE_CRC:
        if (msg->crc_result == NULL) {
            return -EINVAL;
        }
        return ec_cal_image_crc(msg->flash_address, msg->size, msg->crc_result);

    case FLASH_SERVICE_LOAD_TO_RAM:
        if (msg->ram_address == NULL) {
            return -EINVAL;
        }
        return qc_flash_read(msg->flash_address, msg->ram_address, msg->size);

    default:
        return -EINVAL;
    }
}

static void flash_service(void) {
    flash_service_msg_t msg;

    while (true) {
        int ret = k_msgq_get(&flash_service_queue, &msg, K_FOREVER);
        if (ret < 0) {
            LOG_ERR("Failed to receive flash request: %d", ret);
            continue;
        }

        ret = flash_service_execute(&msg);
        if (ret < 0) {
            LOG_ERR("Flash command %d failed at 0x%08lx, size %u: %d",
                    msg.command, (long)msg.flash_address,
                    (unsigned int)msg.size, ret);
        }

        *msg.operation_result = ret;
        k_sem_give(msg.completion);
    }
}

K_THREAD_DEFINE(flash_service_thread, FLASH_SERVICE_STACK_SIZE, flash_service,
                NULL, NULL, NULL, FLASH_SERVICE_PRIORITY, 0, 0);

int flash_service_request(flash_service_command_t command,
                          off_t flash_address, size_t size,
                          void *ram_address, void *data_pointer,
                          uint32_t *crc_result) {
    int operation_result = -EINPROGRESS;
    uint32_t computed_crc = 0;
    struct k_sem completion;

    if (flash_address < 0 || size == 0) {
        return -EINVAL;
    }
    if ((command == FLASH_SERVICE_WRITE || command == FLASH_SERVICE_READ) &&
        data_pointer == NULL) {
        return -EINVAL;
    }
    if (command == FLASH_SERVICE_LOAD_TO_RAM && ram_address == NULL) {
        return -EINVAL;
    }
    if (command == FLASH_SERVICE_CRC && crc_result == NULL) {
        return -EINVAL;
    }
    if (command < FLASH_SERVICE_WRITE || command > FLASH_SERVICE_LOAD_TO_RAM) {
        return -EINVAL;
    }

    k_sem_init(&completion, 0, 1);

    flash_service_msg_t msg = {
        .command = command,
        .flash_address = flash_address,
        .size = size,
        .ram_address = ram_address,
        .data_pointer = data_pointer,
        .crc_result = command == FLASH_SERVICE_CRC ? &computed_crc : NULL,
        .operation_result = &operation_result,
        .completion = &completion,
    };

    int ret = k_msgq_put(&flash_service_queue, &msg, K_FOREVER);
    if (ret < 0) {
        LOG_ERR("Failed to queue flash command %d: %d", command, ret);
        return ret;
    }

    ret = k_sem_take(&completion, K_FOREVER);
    if (ret < 0) {
        LOG_ERR("Failed waiting for flash command %d: %d", command, ret);
        return ret;
    }

    if (command == FLASH_SERVICE_CRC && operation_result == 0) {
        *crc_result = computed_crc;
    }

    return operation_result;
}
