/*
 * @Author: andy.chang
 * @Date: 2026-05-12
 * @Last Modified by:   andy.chang
 * @Last Modified time: 2026-05-13
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * Keyboard service layer public API
 *
 * 給 driver 層 (driver/keyboard.c, driver/hidi2c.c) 使用：
 *   - driver/keyboard.c 透過 process_event() 上送每筆按鍵事件，
 *     service 內部維護 6KRO 狀態並將完整 report 主動 push 給 hidi2c driver。
 *   - driver/hidi2c.c 在收到 Host 的 HIDI2C RESET 指令時呼叫 reset()
 *     清除 service 端內部狀態。
 */

/**
 * @brief 接收一筆按鍵事件 (driver/keyboard.c 呼叫)
 *
 * @param input_code Zephyr 標準輸入碼 (INPUT_KEY_*)
 * @param pressed    true: 按下, false: 放開
 */
void keyboard_service_process_event(uint16_t input_code, bool pressed);

/**
 * @brief 清除內部 6KRO 狀態 (HIDI2C RESET 時呼叫)
 */
void keyboard_service_reset(void);