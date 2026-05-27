/*
 * @Author: andy.chang
 * @Date: 2026-05-12
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-05-27 16:52:21
 */

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/usb/class/hid.h>
#include <zephyr/input/input_hid.h>
#include <zephyr/logging/log.h>

#include <interface/hidi2c.h>
#include <interface/keyboard.h>
#include <interface/system.h>

LOG_MODULE_REGISTER(keyboard_service, LOG_LEVEL_INF);

/* Local events (service-private) */
#define KB_EVT_RESET BIT(0)
#define KB_EVT_PROCESS BIT(1)

static K_EVENT_DEFINE(kb_event);

/* 輸入事件 (input_code + pressed) 透過 msgq 傳給 service thread */
struct kb_input_event {
    uint16_t input_code;
    bool pressed;
};

#define KB_INPUT_Q_DEPTH 16
K_MSGQ_DEFINE(kb_input_msgq, sizeof(struct kb_input_event), KB_INPUT_Q_DEPTH,
              4);

/*
 * Service 只維護 6KRO 內部狀態；每次事件處理後，把最新 report push 到
 * hidi2c driver 的 queue，由 driver 端負責 INT GPIO 與 200ms 計時器。
 */
static struct keyboard_report current_report = {
    .length = sizeof(struct keyboard_report),
    .modifier = 0,
    .reserved = 0,
    .keys = {0},
};

/* -------------------------------------------------------------------------
 * Internal handlers (在 service thread 內執行)
 * ------------------------------------------------------------------------- */

static void kb_handle_process_event(uint16_t input_code, bool pressed) {
    uint8_t modifier = input_to_hid_modifier(input_code);

    if (modifier != HID_KBD_MODIFIER_NONE) {
        if (pressed) {
            current_report.modifier |= modifier;
        } else {
            current_report.modifier &= ~modifier;
        }
    } else {
        int16_t hid_code = input_to_hid_code(input_code);
        if (hid_code < 0) {
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

    /* Push 最新 report 至 hidi2c driver (driver 負責 notify/timer) */
    (void)hidi2c_post_report(&current_report);
}

static void kb_handle_reset(void) {
    current_report.modifier = 0;
    memset(current_report.keys, 0, sizeof(current_report.keys));
}

/* -------------------------------------------------------------------------
 * Public API：只負責把事件投遞給 service thread
 * ------------------------------------------------------------------------- */

/**
 * @brief 3a. 接收按鍵事件並更新 6KRO report，之後 push 至 hidi2c driver
 */
void keyboard_service_process_event(uint16_t input_code, bool pressed) {
    struct kb_input_event evt = {
        .input_code = input_code,
        .pressed = pressed,
    };

    if (k_msgq_put(&kb_input_msgq, &evt, K_NO_WAIT) != 0) {
        LOG_WRN("kb input msgq full, drop code=0x%04x pressed=%d", input_code,
                pressed);
        return;
    }

    k_event_post(&kb_event, KB_EVT_PROCESS);
}

/**
 * @brief 清除內部 6KRO 狀態 (HIDI2C RESET 路徑呼叫)
 *
 * 注意：driver 端的 report cache、queue、timer、INT 由 hidi2c driver 在
 * RESET handler 內自行清掉；這裡只負責 service 自己的狀態。
 */
void keyboard_service_reset(void) { k_event_post(&kb_event, KB_EVT_RESET); }

/* -------------------------------------------------------------------------
 * Service thread
 * ------------------------------------------------------------------------- */

static void service(void) {
    uint32_t evt = 0;

    while (1) {
        evt = k_event_wait(&kb_event, (KB_EVT_RESET | KB_EVT_PROCESS), true,
                           K_FOREVER);

        if (evt & KB_EVT_RESET) {
            kb_handle_reset();
        }

        if (evt & KB_EVT_PROCESS) {
            struct kb_input_event input_evt;
            while (k_msgq_get(&kb_input_msgq, &input_evt, K_NO_WAIT) == 0) {
                kb_handle_process_event(input_evt.input_code,
                                        input_evt.pressed);
            }
        }
    }
}

K_THREAD_DEFINE(kb_id, APP_STACK_NML, service, NULL, NULL, NULL, APP_PRIO_M, 0,
                0);

#ifdef CONFIG_KEYBOARD_SHELL
#include <stdlib.h>
#include <zephyr/shell/shell.h>

static int cmd_kbd_press(const struct shell *sh, size_t argc, char **argv) {
    uint16_t input_code = (uint16_t)strtoul(argv[1], NULL, 0);

    keyboard_service_process_event(input_code, true);
    shell_info(sh, "Simulated press: input_code=0x%04x", input_code);
    return 0;
}

static int cmd_kbd_release(const struct shell *sh, size_t argc, char **argv) {
    uint16_t input_code = (uint16_t)strtoul(argv[1], NULL, 0);

    keyboard_service_process_event(input_code, false);
    shell_info(sh, "Simulated release: input_code=0x%04x", input_code);
    return 0;
}

static int cmd_kbd_tap(const struct shell *sh, size_t argc, char **argv) {
    uint16_t input_code = (uint16_t)strtoul(argv[1], NULL, 0);

    keyboard_service_process_event(input_code, true);
    keyboard_service_process_event(input_code, false);
    shell_info(sh, "Simulated tap: input_code=0x%04x", input_code);
    return 0;
}

static int cmd_kbd_reset(const struct shell *sh, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    keyboard_service_reset();
    shell_info(sh, "Keyboard service reset");
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_kbd,
    SHELL_CMD_ARG(press, NULL, "Simulate key press <input_code>",
                  cmd_kbd_press, 2, 0),
    SHELL_CMD_ARG(release, NULL, "Simulate key release <input_code>",
                  cmd_kbd_release, 2, 0),
    SHELL_CMD_ARG(tap, NULL, "Simulate key press+release <input_code>",
                  cmd_kbd_tap, 2, 0),
    SHELL_CMD_ARG(reset, NULL, "Reset keyboard service state",
                  cmd_kbd_reset, 1, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(kbd, &sub_kbd, "Keyboard commands", NULL);
#endif
