/*
 * @Author: andy.chang
 * @Date: 2026-05-12
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-05-13 16:00:35
 */

#include <string.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/class/hid.h>

#include <interface/hidi2c.h>
#include <interface/keyboard.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(hidi2c, LOG_LEVEL_INF);

/* I2C 狀態定義 */
enum {
    HIDI2C_STATE_IDLE = 0,
    HIDI2C_STATE_WRITE,
    HIDI2C_STATE_READ,
};

static uint8_t hidi2c_state = HIDI2C_STATE_IDLE;
static uint16_t hidi2c_reg_addr = 0;
static uint16_t hidi2c_idx = 0;

#define HIDI2C_RX_BUF_SIZE 16
#define HIDI2C_TX_BUF_SIZE 64 /* 需大於等於 Report Descriptor 長度 */
static uint8_t hidi2c_rx_buf[HIDI2C_RX_BUF_SIZE];
static uint8_t hidi2c_tx_buf[HIDI2C_TX_BUF_SIZE];

/* 定義 INT 引腳 (需在 DeviceTree 設定 alias "hidi2c_int") */
static const struct gpio_dt_spec hidi2c_int_gpio =
    GPIO_DT_SPEC_GET_OR(DT_ALIAS(hidi2c_int), gpios, {0});

/* 用於追蹤 RESET 指令狀態 */
static bool reset_in_progress = false;

/* -------------------------------------------------------------------------
 * Report pipeline: post -> msgq + INT + 200ms timer
 *                  read 0x0003 -> dequeue into cache -> return to Host
 * ------------------------------------------------------------------------- */

#define HIDI2C_REPORT_Q_DEPTH 8
#define HIDI2C_REPORT_LATCH_MS 200

K_MSGQ_DEFINE(hidi2c_report_msgq, sizeof(struct keyboard_report),
              HIDI2C_REPORT_Q_DEPTH, 4);

/* cached_report 會被 I2C read callback (可能是 ISR context) 與 timeout
 * workqueue handler 同時存取，所以用 spinlock 保護。 */
static struct keyboard_report cached_report = {
    .length = sizeof(struct keyboard_report),
};
static struct k_spinlock cached_report_lock;

static void hidi2c_report_timeout_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(hidi2c_report_timeout_work,
                               hidi2c_report_timeout_handler);

/**
 * @brief 拉低 INT 引腳通知 Host (內部使用)
 */
static void hidi2c_notify_host(void) {
    if (hidi2c_int_gpio.port != NULL && device_is_ready(hidi2c_int_gpio.port)) {
        /* DT 通常設 Active Low；設 1 代表 assert (拉低) */
        gpio_pin_set_dt(&hidi2c_int_gpio, 1);
    }
}

/**
 * @brief 解除 INT (拉高) (內部使用)
 */
static void hidi2c_deassert_host(void) {
    if (hidi2c_int_gpio.port != NULL) {
        gpio_pin_set_dt(&hidi2c_int_gpio, 0);
    }
}

/**
 * @brief 通知 Host 並啟動 (或重置) 200ms 保留計時 (內部使用)
 *
 * 在有新 report 可供讀取時呼叫：
 *   - service push 新 report 後 (hidi2c_post_report)
 *   - Host 讀完 0x0003 後，若 queue 還有後續 report
 */
static void hidi2c_arm_notify(void) {
    hidi2c_notify_host();
    k_work_reschedule(&hidi2c_report_timeout_work,
                      K_MSEC(HIDI2C_REPORT_LATCH_MS));
}

/**
 * @brief 200ms 內 Host 未讀走 report 時的 timeout
 *
 * 清空 cached_report + queue，並解除 INT 訊號，避免 Host 稍後讀到過期 report。
 */
static void hidi2c_report_timeout_handler(struct k_work *work) {
    ARG_UNUSED(work);

    k_msgq_purge(&hidi2c_report_msgq);

    k_spinlock_key_t key = k_spin_lock(&cached_report_lock);
    memset(&cached_report, 0, sizeof(cached_report));
    cached_report.length = sizeof(struct keyboard_report);
    k_spin_unlock(&cached_report_lock, key);

    hidi2c_deassert_host();
}

/**
 * @brief Service 層 push 最新 report 到 HIDI2C driver
 *
 * 進 queue 後立刻通知 Host 並啟動 (或重置) 200ms 保留計時。
 * 實際把 report 從 queue 拉進 cache 的動作延後到 Host 讀 0x0003 時再做。
 */
int hidi2c_post_report(const struct keyboard_report *report) {
    if (report == NULL) {
        return -EINVAL;
    }

    int ret = k_msgq_put(&hidi2c_report_msgq, report, K_NO_WAIT);

    /* 即使 queue 滿 (舊事件還沒被讀走)，仍然重新拉 INT 與重置計時：
     * Host 早晚會進來讀，屆時再從 queue 頭依序取出即可。 */
    hidi2c_arm_notify();

    return ret;
}

/* -------------------------------------------------------------------------
 * HID descriptor / report descriptor 已搬到 interface/hidi2c.h
 * (與 keyboard_report struct 並列，方便同步修改)
 * ------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------
 * I2C write / read dispatchers
 * ------------------------------------------------------------------------- */

/**
 * @brief Host 寫入 wCommandRegister (0x0005) 時觸發
 */
static int hidi2c_dispatch_write(uint16_t reg_addr, const uint8_t *data,
                                 uint16_t len) {
    if (reg_addr == HIDI2C_REG_COMMAND) {
        /* Host 發送 RESET 指令：0x01 (OpCode) + 0x00 (Reserved) */
        if (len >= 2 && data[0] == 0x01) {
            /* 清 service 端 6KRO 狀態 */
            keyboard_service_reset();

            /* 清 driver 自身的 queue / cache / timer */
            k_msgq_purge(&hidi2c_report_msgq);
            k_work_cancel_delayable(&hidi2c_report_timeout_work);

            k_spinlock_key_t key = k_spin_lock(&cached_report_lock);
            memset(&cached_report, 0, sizeof(cached_report));
            cached_report.length = sizeof(struct keyboard_report);
            k_spin_unlock(&cached_report_lock, key);

            reset_in_progress = true;

            /* 重設完成，主動拉低 INT 讓 Host 讀 0x0005 取 reset ack */
            hidi2c_notify_host();
            return 0;
        }
    }

    return -ENOTSUP;
}

/**
 * @brief Host 讀取特定 Register Address 時觸發
 */
static int hidi2c_dispatch_read(uint16_t reg_addr, uint8_t *out_buf,
                                uint16_t max_len) {
    switch (reg_addr) {
    case HIDI2C_REG_HID_DESCRIPTOR:
        /* HID descriptor */
        memcpy(out_buf, hidi2c_hid_descriptor,
               MIN(max_len, sizeof(hidi2c_hid_descriptor)));
        return 0;

    case HIDI2C_REG_REPORT_DESCRIPTOR:
        /* Report descriptor */
        memcpy(out_buf, hidi2c_report_descriptor,
               MIN(max_len, sizeof(hidi2c_report_descriptor)));
        return 0;

    case HIDI2C_REG_INPUT: {
        /* Input register：被 Host 讀取時才把最新一筆從 queue 拉進 cache。
         * Queue 為空時維持上一筆 cache (例如重複讀取)。 */
        struct keyboard_report rpt;
        if (k_msgq_get(&hidi2c_report_msgq, &rpt, K_NO_WAIT) == 0) {
            k_spinlock_key_t key = k_spin_lock(&cached_report_lock);
            memcpy(&cached_report, &rpt, sizeof(cached_report));
            k_spin_unlock(&cached_report_lock, key);
        }

        k_spinlock_key_t key = k_spin_lock(&cached_report_lock);
        memcpy(out_buf, &cached_report, MIN(max_len, sizeof(cached_report)));
        k_spin_unlock(&cached_report_lock, key);

        // 解除 INT、取消計時
        hidi2c_deassert_host();
        k_work_cancel_delayable(&hidi2c_report_timeout_work);

        /* 讀取完成後：若 queue 還有後續 report，再觸發一次 INT + 重置 200ms
         * 計時 */
        if (k_msgq_num_used_get(&hidi2c_report_msgq) > 0) {
            hidi2c_arm_notify();
        }
        return 0;
    }

    case HIDI2C_REG_COMMAND:
        /* Command register：讀取時回傳 Reset Completion 狀態 */
        if (reset_in_progress) {
            if (max_len >= 2) {
                out_buf[0] = 0x00;
                out_buf[1] = 0x00;
                reset_in_progress = false;
                /* Reset ack 已交付，解除 INT */
                hidi2c_deassert_host();
                return 0;
            }
        }
        return 0;
    }

    return -ENOTSUP;
}

/* -------------------------------------------------------------------------
 * I2C Target Callbacks (參考 acpi.c)
 * ------------------------------------------------------------------------- */

static int hidi2c_target_write_requested_cb(struct i2c_target_config *config) {
    hidi2c_state = HIDI2C_STATE_WRITE;
    hidi2c_idx = 0;
    hidi2c_reg_addr = 0;
    memset(hidi2c_rx_buf, 0, sizeof(hidi2c_rx_buf));
    return 0;
}

static int hidi2c_target_write_received_cb(struct i2c_target_config *config,
                                           uint8_t val) {
    /* 前兩個 Byte 為 Register Address (Little Endian) */
    if (hidi2c_idx == 0) {
        hidi2c_reg_addr = val;
    } else if (hidi2c_idx == 1) {
        hidi2c_reg_addr |= ((uint16_t)val << 8);
        LOG_INF("Host select REG 0x%04x", hidi2c_reg_addr);
    } else {
        /* 之後的資料為指令 Payload */
        uint16_t payload_idx = hidi2c_idx - 2;
        if (payload_idx < sizeof(hidi2c_rx_buf)) {
            hidi2c_rx_buf[payload_idx] = val;
        }
    }
    hidi2c_idx++;
    return 0;
}

static int hidi2c_target_read_cb(struct i2c_target_config *config,
                                 uint8_t *val) {
    if (hidi2c_state != HIDI2C_STATE_READ) {
        /* 第一次讀取：執行分流並填充發送緩衝區。INT/timer 狀態由 dispatch_read
         * 依各 register 自行決定 (例如 0x0003 讀完後依 queue 狀態再次 notify)。
         */
        memset(hidi2c_tx_buf, 0, sizeof(hidi2c_tx_buf));
        hidi2c_dispatch_read(hidi2c_reg_addr, hidi2c_tx_buf,
                             sizeof(hidi2c_tx_buf));
        hidi2c_idx = 0;
        hidi2c_state = HIDI2C_STATE_READ;
    }

    if (hidi2c_idx < sizeof(hidi2c_tx_buf)) {
        *val = hidi2c_tx_buf[hidi2c_idx++];
    } else {
        *val = 0xFF; /* 緩衝區溢位保護 */
    }
    return 0;
}

static int hidi2c_target_stop_cb(struct i2c_target_config *config) {
    /* 如果是寫入操作結束 (idx > 2 代表除了暫存器地址還有資料) */
    if (hidi2c_state == HIDI2C_STATE_WRITE && hidi2c_idx > 2) {
        hidi2c_dispatch_write(hidi2c_reg_addr, hidi2c_rx_buf, hidi2c_idx - 2);
    }

    hidi2c_state = HIDI2C_STATE_IDLE;
    hidi2c_idx = 0;
    return 0;
}

static struct i2c_target_callbacks hidi2c_target_callbacks = {
    .write_requested = hidi2c_target_write_requested_cb,
    .write_received = hidi2c_target_write_received_cb,
    .read_requested = hidi2c_target_read_cb,
    .read_processed = hidi2c_target_read_cb,
    .stop = hidi2c_target_stop_cb,
};

#define HIDI2C_ADDR 0x50 /* 假設 Keyboard HID I2C 位址 */

static struct i2c_target_config target_cfg = {
    .address = HIDI2C_ADDR,
    .callbacks = &hidi2c_target_callbacks,
};

/* -------------------------------------------------------------------------
 * Init
 * ------------------------------------------------------------------------- */

static int hidi2c_init(void) {
    /* 假設鍵盤使用與 ACPI 不同的 I2C Alias，或依需求調整 */
    const struct device *bus = DEVICE_DT_GET_OR_NULL(DT_ALIAS(keyboard_i2c));

    if (!bus) {
        /* 備援：若無特定 alias 則嘗試獲取 acpi_i2c 同一條 Bus */
        bus = DEVICE_DT_GET_OR_NULL(DT_ALIAS(acpi_i2c));
    }

    if (!bus || !device_is_ready(bus)) {
        LOG_ERR("I2C bus not ready");
        return -ENODEV;
    }

    /* 初始化 INT 引腳為輸出，預設 Inactive (拉高) */
    if (hidi2c_int_gpio.port != NULL && device_is_ready(hidi2c_int_gpio.port)) {
        gpio_pin_configure_dt(&hidi2c_int_gpio, GPIO_OUTPUT_INACTIVE);
    }

    if (i2c_target_register(bus, &target_cfg) < 0) {
        LOG_ERR("Failed to register target");
        return -EIO;
    }

    return 0;
}

SYS_INIT(hidi2c_init, APPLICATION, 90);
