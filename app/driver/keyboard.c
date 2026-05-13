/*
 * @Author: andy.chang 
 * @Date: 2026-05-12 12:32:24 
 * @Last Modified by:   andy.chang 
 * @Last Modified time: 2026-05-12 12:32:24 
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/kscan.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>

#include <zephyr/logging/log.h>

#include <interface/keyboard.h>

LOG_MODULE_REGISTER(keyboard, LOG_LEVEL_INF);

#define MAX_MATRIX_KEY_COLS 16
#define MAX_MATRIX_KEY_ROWS 8

/*
 * 2a. Matrix (col, row) -> Zephyr INPUT_KEY_* 映射表
 *
 * 服務層 (service.c) 會再透過 Zephyr input_hid 子系統的
 * input_to_hid_code() / input_to_hid_modifier() 轉成 HID usage code。
 * 因此這裡只負責「硬體矩陣 -> 標準輸入碼」這一層。
 */
static const uint16_t keymap[MAX_MATRIX_KEY_COLS][MAX_MATRIX_KEY_ROWS] = {
    [0][1] = INPUT_KEY_GRAVE,      /* `~ */
    [0][2] = INPUT_KEY_F1,
    [0][3] = INPUT_KEY_TAB,
    [0][4] = INPUT_KEY_1,
    [0][6] = INPUT_KEY_CAPSLOCK,
    /* ... 其他按鍵映射 (依實際硬體矩陣填入) ... */
};

/*
 * 2c. kscan callback：查表後送至服務層
 */
static void kb_callback(const struct device *dev, uint32_t row, uint32_t col,
                        bool pressed) {
    ARG_UNUSED(dev);

    if (col >= MAX_MATRIX_KEY_COLS || row >= MAX_MATRIX_KEY_ROWS) {
        LOG_WRN("Key event out of matrix range: row=%u, col=%u", row, col);
        return;
    }

    uint16_t input_code = keymap[col][row];
    if (input_code == INPUT_KEY_RESERVED) {
        LOG_DBG("Unmapped matrix position: row=%u, col=%u", row, col);
        return;
    }

    LOG_DBG("Key event: row=%u, col=%u, input_code=0x%04x, pressed=%d", row,
            col, input_code, pressed);

    keyboard_service_process_event(input_code, pressed);
}


#include <zephyr/init.h>
/*
 * 2b. 初始化 keyboard scan 周邊
 */
static int keyboard_init(void) {
    const struct device *const kscan_dev =
        DEVICE_DT_GET(DT_CHOSEN(zephyr_keyboard_scan));

    if (!device_is_ready(kscan_dev)) {
        LOG_ERR("kscan device %s is not ready", kscan_dev->name);
        return -ENODEV;
    }

    if (kscan_config(kscan_dev, kb_callback) != 0) {
        LOG_ERR("Failed to configure kscan callback on %s", kscan_dev->name);
        return -EIO;
    }

    if (kscan_enable_callback(kscan_dev) != 0) {
        LOG_ERR("Failed to enable kscan callback on %s", kscan_dev->name);
        return -EIO;
    }

    LOG_INF("Keyboard driver initialized on %s", kscan_dev->name);
    return 0;
}

SYS_INIT(keyboard_init, APPLICATION, 0);
