/*
 * @Author: andy.chang
 * @Date: 2026-05-12
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-05-13 16:08:44
 */

#pragma once

#include <stdint.h>

#include <zephyr/toolchain.h>
#include <zephyr/usb/class/hid.h>

/*
 * HIDI2C driver layer public API
 *
 * 由 service 層把更新後的 keyboard_report 丟到 HIDI2C driver：
 * driver 內部有 message queue 接收，收到後才對 Host 拉 INT (GPIO)
 * 並啟動 200ms 保留計時器；超時未被 Host 讀走則清空 report 並解除 INT。
 * driver 的 INT GPIO 控制完全內部化，service 端不再需要呼叫 notify_host。
 */

/*
 * HID over I2C register addresses (wire-level, Little Endian on I2C)
 *
 * Host 每筆 I2C 交易前兩個 Byte 指定這裡的 register；driver 根據 register
 * 把讀取內容轉到對應 descriptor / report / command 回應。
 */
#define HIDI2C_REG_RESERVED           0x0000 /* 保留未使用 */
#define HIDI2C_REG_HID_DESCRIPTOR     0x0001 /* HID descriptor */
#define HIDI2C_REG_REPORT_DESCRIPTOR  0x0002 /* Report descriptor */
#define HIDI2C_REG_INPUT              0x0003 /* Input report (keyboard 狀態) */
#define HIDI2C_REG_OUTPUT             0x0004 /* Output report (LED 等) */
#define HIDI2C_REG_COMMAND            0x0005 /* wCommandRegister (RESET 等) */

/* 將 16-bit register 值拆成 Little Endian 兩個 bytes，方便塞進 descriptor */
#define HIDI2C_REG_LE16(x)  ((x) & 0xFF), (((x) >> 8) & 0xFF)

/*
 * HID over I2C keyboard report 格式 (10 bytes, 含長度欄位)
 *
 * 由 service 層打包；driver/hidi2c.c 會原樣搬進 I2C TX 緩衝區送給 Host。
 *
 * !!! 此 struct 的 layout 必須與下方 hidi2c_report_descriptor[] 描述的
 *     HID Report 一致；任一邊修改時請同步修改另一邊。
 */
struct keyboard_report {
    uint16_t length;   /* 整體長度 (= sizeof(struct keyboard_report)) */
    uint8_t  modifier; /* 修飾鍵 Bitmask */
    uint8_t  reserved; /* 0x00 */
    uint8_t  keys[6];  /* 按鍵陣列 (6-key rollover) */
} __packed;

/*
 * HID descriptor (HIDI2C_REG_HID_DESCRIPTOR 讀回內容)
 *
 * 描述本裝置在 HID over I2C 通訊中使用的 register 位址、版本、Report Descriptor
 * 長度等。HID descriptor 自身的 wHIDDescLength = 30。
 */
static const uint8_t hidi2c_hid_descriptor[] __unused = {
    0x1E, 0x00,                                    /* wHIDDescLength = 30 */
    0x00, 0x01,                                    /* bcdVersion = 1.0 */
    0x3F, 0x00,                                    /* wReportDescLength (~63 bytes) */
    HIDI2C_REG_LE16(HIDI2C_REG_REPORT_DESCRIPTOR), /* wReportDescRegister */
    HIDI2C_REG_LE16(HIDI2C_REG_INPUT),             /* wInputRegister */
    0x00, 0x00,                                    /* wMaxInputLength */
    HIDI2C_REG_LE16(HIDI2C_REG_OUTPUT),            /* wOutputRegister */
    HIDI2C_REG_LE16(HIDI2C_REG_COMMAND),           /* wCommandRegister */
    /* 剩餘填充欄位 (依 HID over I2C spec 補齊到 30 bytes) */
};

/*
 * Report descriptor (HIDI2C_REG_REPORT_DESCRIPTOR 讀回內容)
 *
 * 使用 Zephyr 內建的 HID_KEYBOARD_REPORT_DESC() 巨集，描述標準 HID Boot
 * Keyboard 格式：8-bit modifier、1-byte reserved、6-byte keys，正好對應
 * 上方的 keyboard_report struct (length 欄位是 HIDI2C 框架附加，不在
 * report descriptor 範圍內)。
 *
 * 修改 keyboard_report layout 時請同步調整這裡。
 */
static const uint8_t hidi2c_report_descriptor[] __unused = HID_KEYBOARD_REPORT_DESC();

/**
 * @brief 把最新 report 丟進 HIDI2C driver 的 queue
 *
 * driver 會在 queue 收到訊息後向 Host 發出 GPIO 通知，並啟動 200ms 計時器。
 * 若 Host 在時限內讀取，計時器被取消；否則 timeout 會清空 report 並解除 INT。
 *
 * @param report 要送出的 report (以值複製進 queue)
 * @retval 0         成功
 * @retval -EAGAIN   queue 已滿 (最舊的事件可能因此遺失)
 * @retval 其他負值  底層錯誤
 */
int hidi2c_post_report(const struct keyboard_report *report);
