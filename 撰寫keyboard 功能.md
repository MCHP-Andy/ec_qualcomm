撰寫keyboard 功能

1. 在app/driver/hidi2c.c實作i2c轉發功能
 a. 實作 HID descriptor 
    ```
    static const uint8_t hid_descriptor[] = {
        0x1E, 0x00,      // Length = 30
        0x00, 0x01,      // Version 1.0
        0x4B, 0x00,      // Report Desc Length
        0x02, 0x00,      // Report Desc Register (0x0002)
        0x03, 0x00,      // Input Register (0x0003)
        0x00, 0x00,      // Max Input Length
        0x04, 0x00,      // Output Register (0x0004)
        0x05, 0x00,      // <--- wCommandRegister 定義在這裡為 0x0005
        /* ... 剩餘欄位 ... */
    };
    ```
 b. 實作 report descriptor (HID_KEYBOARD_REPORT_DESC in include/zephyr/usb/class/hid.h) 
 c. 實作i2c 接收cmd的分流
    - 0x0001 讀取HID descriptor
    - 0x0002 讀取HID report descriptor
    - 0x0005 讀取HID report


2. 在app/driver/keyboard.c實作keyboard轉發功能
 a. 實作 key 轉譯成硬體keyboard的hid_kbd_code(include/zephyr/usb/class/hid.h) 
 b. 初始化 key scan 周邊(zephyr-rtos\samples\drivers\kscan\src\main.c)
 c. 當key callback時，查詢對應keyboard的hid_kbd_code

3. 在app/service/keyboard/service.c實作服務
 a. 提供API在keyboard.c收到按鍵後輸入，2個input分別為hid_kbd_code(include/zephyr/usb/class/hid.h) 與 press/release
 b. 提供API給hidi2c.c讀取hid report
 c. 收到資料後trigger event, thread 執行打包成 hid report格式
    hid report格式:
        Modifier Keys Byte是一個 Bitmask，每個 bit 的定義如下：
        Bit 0: 左 Ctrl (0x01)
        Bit 1: 左 Shift (0x02)
        Bit 2: 左 Alt (0x04)
        Bit 3: 左 GUI (Win/Command 鍵) (0x08)
        Bit 4: 右 Ctrl (0x10)
        Bit 5: 右 Shift (0x20)
        Bit 6: 右 Alt (0x40)
        Bit 7: 右 GUI (0x80)
        ```
        struct keyboard_report {
            uint16_t length;     // 設定為 10
            uint8_t  modifier;   // 修飾鍵
            uint8_t  reserved;   // 0x00
            uint8_t  keys[6];    // 按鍵陣列
        } __packed;
        ```
 