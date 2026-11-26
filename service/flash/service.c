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

K_MSGQ_DEFINE(flash_service_queue, sizeof(flash_service_msg_t),
              FLASH_SERVICE_QUEUE_LEN, 4);

/*
 * EC status (ACPI 0xEC) bookkeeping
 *
 * pending counts the requests that are queued OR running: +1 at submit
 * (before k_msgq_put), -1 when one finished. Only the transition to 0, i.e.
 * the last outstanding request finished, updates the status; while other
 * requests are still queued it stays BUSY.
 *
 *   submit A, B, C : pending 1 -> 2 -> 3, status BUSY
 *   A done         : pending 2, status unchanged (BUSY)
 *   B done         : pending 1, status unchanged (BUSY)
 *   C done         : pending 0, status IDLE (or SPI_OP_FAILED)
 *
 * Counting at submit (not when the thread takes the request) keeps the status
 * BUSY in the window before the lower priority flash thread runs. Both sides
 * update pending and the status under status_lock.
 */
static struct k_spinlock status_lock;
static uint32_t pending;  // Requests queued or running
static bool op_failed;    // One of the outstanding requests failed

// A request is submitted: count it and report BUSY
static void flash_service_pending_inc(void) {
    k_spinlock_key_t key = k_spin_lock(&status_lock);

    if (pending == 0) {
        // First request of a new batch: forget the previous failure
        op_failed = false;
    }
    pending++;
    ec_boot_info_get()->status = EC_STATE_BUSY;

    k_spin_unlock(&status_lock, key);
}

// A request finished (or was not queued): only the last one updates the status
static void flash_service_pending_dec(int result) {
    k_spinlock_key_t key = k_spin_lock(&status_lock);

    if (result < 0) {
        // Kept until the batch ends, a later success does not clear it
        op_failed = true;
    }
    if (pending > 0) {
        pending--;
    }
    if (pending == 0) {
        // Last outstanding request finished: leave BUSY
        ec_boot_info_get()->status = op_failed ? EC_STATE_SPI_OP_FAILED
                                               : EC_STATE_BOOTLOADER_IDLE_WP_OFF;
    }
    // pending > 0: other requests are still queued, status stays BUSY

    k_spin_unlock(&status_lock, key);
}

// Busy while any request is queued or running
bool flash_service_busy(void) {
    return pending > 0;
}

static int flash_service_execute(const flash_service_msg_t *msg,
                                 uint32_t *crc) {
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
        return ec_cal_image_crc(msg->flash_address, msg->size, crc);

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
        uint32_t crc = 0;
        int ret = k_msgq_get(&flash_service_queue, &msg, K_FOREVER);
        if (ret < 0) {
            LOG_ERR("Failed to receive flash request: %d", ret);
            continue;
        }

        ret = flash_service_execute(&msg, &crc);
        if (ret < 0) {
            LOG_ERR("Flash command %d failed at 0x%08lx, size %u: %d",
                    msg.command, (long)msg.flash_address,
                    (unsigned int)msg.size, ret);
        }

        // Report the result before leaving BUSY, so the host never sees IDLE
        // with stale results (e.g. CRC for 0xA2)
        if (msg.done != NULL) {
            msg.done(msg.command, ret, crc, msg.user_data);
        }
        flash_service_pending_dec(ret);
    }
}

K_THREAD_DEFINE(flash_service_thread, FLASH_SERVICE_STACK_SIZE, flash_service,
                NULL, NULL, NULL, FLASH_SERVICE_PRIORITY, 0, 0);

static int flash_service_check(const flash_service_msg_t *msg) {
    if (msg == NULL || msg->flash_address < 0 || msg->size == 0) {
        return -EINVAL;
    }
    if ((msg->command == FLASH_SERVICE_WRITE ||
         msg->command == FLASH_SERVICE_READ) &&
        msg->data_pointer == NULL) {
        return -EINVAL;
    }
    if (msg->command == FLASH_SERVICE_LOAD_TO_RAM && msg->ram_address == NULL) {
        return -EINVAL;
    }
    if (msg->command < FLASH_SERVICE_WRITE ||
        msg->command > FLASH_SERVICE_LOAD_TO_RAM) {
        return -EINVAL;
    }
    return 0;
}

static int flash_service_queue_put(const flash_service_msg_t *msg,
                                   k_timeout_t timeout) {
    int ret = flash_service_check(msg);
    if (ret < 0) {
        return ret;
    }

    // Count before queueing, so the request cannot finish before it is counted
    flash_service_pending_inc();

    ret = k_msgq_put(&flash_service_queue, msg, timeout);
    if (ret < 0) {
        LOG_ERR("Failed to queue flash command %d: %d", msg->command, ret);
        // Not queued: undo its count (status IDLE again if it was the only one)
        flash_service_pending_dec(0);
        return -ENOMEM;
    }

    return 0;
}

int flash_service_submit(const flash_service_msg_t *msg) {
    return flash_service_queue_put(msg, K_NO_WAIT);
}

/* Synchronous request: the context lives on the caller stack, which is safe
 * because the caller waits for the completion callback.
 */
typedef struct {
    struct k_sem completion;
    int result;
    uint32_t crc;
} flash_service_sync_t;

static void flash_service_sync_done(flash_service_command_t command, int result,
                                    uint32_t crc, void *user_data) {
    flash_service_sync_t *sync = user_data;

    ARG_UNUSED(command);
    sync->result = result;
    sync->crc = crc;
    k_sem_give(&sync->completion);
}

int flash_service_request(const flash_service_msg_t *msg, uint32_t *crc_result) {
    flash_service_sync_t sync = {.result = -EINPROGRESS};

    if (msg == NULL) {
        return -EINVAL;
    }
    if (msg->command == FLASH_SERVICE_CRC && crc_result == NULL) {
        return -EINVAL;
    }

    k_sem_init(&sync.completion, 0, 1);

    // Same request, completion routed to the semaphore
    flash_service_msg_t req = *msg;
    req.done = flash_service_sync_done;
    req.user_data = &sync;

    int ret = flash_service_queue_put(&req, K_FOREVER);
    if (ret < 0) {
        return ret;
    }

    ret = k_sem_take(&sync.completion, K_FOREVER);
    if (ret < 0) {
        LOG_ERR("Failed waiting for flash command %d: %d", msg->command, ret);
        return ret;
    }

    if (msg->command == FLASH_SERVICE_CRC && sync.result == 0) {
        *crc_result = sync.crc;
    }

    return sync.result;
}
