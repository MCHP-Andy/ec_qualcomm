/*
 * @Author: andy.chang
 * @Date: 2026-05-12
 * @Last Modified by:   andy.chang
 * @Last Modified time: 2026-05-13
 */

#include <zephyr/kernel.h>
#include <zephyr/usb/class/hid.h>
#include <zephyr/input/input_hid.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <string.h>

#include <interface/hidi2c.h>
#include <interface/keyboard.h>

/*
 * Service 只維護 6KRO 內部狀態；每次事件處理後，把最新 report push 到
 * hidi2c driver 的 queue，由 driver 端負責 INT GPIO 與 200ms 計時器。
 */
static struct keyboard_report current_report = {
    .length   = sizeof(struct keyboard_report),
    .modifier = 0,
    .reserved = 0,
    .keys     = {0},
};

static K_MUTEX_DEFINE(report_lock);

/**
 * @brief 3a. 接收按鍵事件並更新 6KRO report，之後 push 至 hidi2c driver
 */
void keyboard_service_process_event(uint16_t input_code, bool pressed)
{
    uint8_t modifier = input_to_hid_modifier(input_code);

    k_mutex_lock(&report_lock, K_FOREVER);

    if (modifier != HID_KBD_MODIFIER_NONE) {
        if (pressed) {
            current_report.modifier |= modifier;
        } else {
            current_report.modifier &= ~modifier;
        }
    } else {
        int16_t hid_code = input_to_hid_code(input_code);
        if (hid_code < 0) {
            k_mutex_unlock(&report_lock);
            return;
        }
        uint8_t hid_kbd_code = (uint8_t)hid_code;

        if (pressed) {
            /* 找到第一個空位並填入 */
            for (int i = 0; i < 6; i++) {
                if (current_report.keys[i] == 0) {
                    current_report.keys[i] = hid_kbd_code;
                    break;
                }
            }
        } else {
            /* 找到對應按鍵並移除，後方按鍵遞補 */
            for (int i = 0; i < 6; i++) {
                if (current_report.keys[i] == hid_kbd_code) {
                    for (int j = i; j < 5; j++) {
                        current_report.keys[j] = current_report.keys[j + 1];
                    }
                    current_report.keys[5] = 0;
                    break;
                }
            }
        }
    }

    /* 3b. Push 最新 report 至 hidi2c driver (driver 負責 notify/timer) */
    (void)hidi2c_post_report(&current_report);

    k_mutex_unlock(&report_lock);
}

/**
 * @brief 清除內部 6KRO 狀態 (HIDI2C RESET 路徑呼叫)
 *
 * 注意：driver 端的 report cache、queue、timer、INT 由 hidi2c driver 在
 * RESET handler 內自行清掉；這裡只負責 service 自己的狀態。
 */
void keyboard_service_reset(void)
{
    k_mutex_lock(&report_lock, K_FOREVER);
    current_report.modifier = 0;
    memset(current_report.keys, 0, sizeof(current_report.keys));
    k_mutex_unlock(&report_lock);
}
