/*
 * @Author: andy.chang
 * @Date: 2026-05-12
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-05-13 16:08:44
 */

#pragma once

#include <stdint.h>

#include <zephyr/toolchain.h>

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
 */
struct keyboard_report {
    uint16_t length;   /* 整體長度 (= sizeof(struct keyboard_report)) */
    uint8_t  modifier; /* 修飾鍵 Bitmask */
    uint8_t  reserved; /* 0x00 */
    uint8_t  keys[6];  /* 按鍵陣列 (6-key rollover) */
} __packed;

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
