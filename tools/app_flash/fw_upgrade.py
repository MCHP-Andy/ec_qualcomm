import time
import struct
import sys
from pyocd.core.helpers import ConnectHelper

class pyOCDMemoryInterface:
    def __init__(self, base_addr):
        # 這裡填入您在 Zephyr DTS 中定義的 retention_ram 實際物理位址
        self.BASE_ADDR = base_addr 
        
        # 100% 對齊 C 語言 struct __packed 的偏移量 (Offset)
        self.OFS_PAYLOAD      = 0
        self.OFS_TOTAL_SIZE   = 2048
        self.OFS_SPI_START    = 2048 + 4
        self.OFS_CUR_SPI_ADDR = 2048 + 4 + 4
        self.OFS_CHUNK_SIZE   = 2048 + 4 + 4 + 4
        self.OFS_BUSY_BIT     = 2048 + 4 + 4 + 4 + 4

        # 初始化 pyOCD 連線 (標準官方 API)
        print("正在尋找偵錯器並連線至 Cortex-M 核心...")
        
        # 💡 關鍵修正：加入 "connect_mode": "attach" 
        # 這會讓 pyocd 以「掛載」而不打擾 MCU 的方式連線，不暫停也不重置核心
        options = {
            "target_override": "cortex_m",
            "connect_mode": "attach"
        }
        
        self.session = ConnectHelper.session_with_chosen_probe(options=options)
        self.session.open() 
        
        self.board = self.session.board
        self.target = self.board.target

        # 💡 雙重保險：萬一某些 J-Link 驅動仍強制暫停了，手動叫它恢復運行
        if self.target.is_halted():
            print("偵測到 MCU 被除錯器暫停，手動恢復運行 (Resume)...")
            self.target.resume()

    def close(self):
        """關閉 pyOCD 連線"""
        if self.session:
            self.session.close()

    def write_payload(self, data):
        """寫入 2KB Payload 區塊 (使用 pyOCD 區塊寫入優化速度)"""
        # 將 bytes 轉換成 pyOCD 接受的 byte 陣列 (list of ints)
        byte_list = list(data)
        self.target.write_memory_block8(self.BASE_ADDR + self.OFS_PAYLOAD, byte_list)

    def write_uint32(self, offset, val):
        """寫入 32-bit 無號整數"""
        self.target.write32(self.BASE_ADDR + offset, val)

    def write_busy_bit(self, val):
        """寫入 8-bit busy_bit"""
        print(f"寫入 busy_bit: {val} (0x{self.BASE_ADDR + self.OFS_BUSY_BIT:08X})")
        self.target.write8(self.BASE_ADDR + self.OFS_BUSY_BIT, val)

    def read_busy_bit(self):
        """讀取 8-bit busy_bit"""
        result = self.target.read8(self.BASE_ADDR + self.OFS_BUSY_BIT)
        return result


def flash_bin_via_ipc(bin_path, spi_target_start_addr, ram_base_addr):
    # 1. 讀取要傳送的二進位檔案
    try:
        with open(bin_path, "rb") as f:
            bin_data = f.read()
    except FileNotFoundError:
        print(f"錯誤: 找不到檔案 {bin_path}")
        return

    total_size = len(bin_data)
    chunk_max_size = 2048
    bytes_sent = 0
    is_first_chunk = True
    
    print(f"檔案讀取成功。總大小: {total_size} bytes")

    # 2. 初始化 pyOCD
    mcu = pyOCDMemoryInterface(base_addr=ram_base_addr)

    try:
        # 如果 MCU 目前處於暫停狀態，將其運行 (確保 Zephyr 狀態機有在跑)
        if mcu.target.is_halted():
            print("偵測到 MCU 處於暫停狀態，正在恢復運行 (Resume)...")
            mcu.target.resume()

        while bytes_sent < total_size:
            # 計算本次切塊
            run_size = min(chunk_max_size, total_size - bytes_sent)
            chunk = bin_data[bytes_sent : bytes_sent + run_size]
            current_spi_addr = spi_target_start_addr + bytes_sent

            # 進行交握：等待 MCU 將上一次的 busy_bit 清除為 0
            # 由於第一包不需要等（此時 MCU 應為閒置），所以從第二包開始嚴格檢查
            if not is_first_chunk:
                print("等待 MCU 完成上一次寫入 (等待 busy_bit == 0)...")
                while True:
                    if mcu.read_busy_bit() == 0:
                        break
                    time.sleep(0.01) # 10ms 輪詢檢查

            # 填入核心資料與元數據 (Metadata)
            mcu.write_payload(chunk)
            mcu.write_uint32(mcu.OFS_TOTAL_SIZE, total_size)
            mcu.write_uint32(mcu.OFS_SPI_START, spi_target_start_addr)
            mcu.write_uint32(mcu.OFS_CUR_SPI_ADDR, current_spi_addr)
            mcu.write_uint32(mcu.OFS_CHUNK_SIZE, run_size)

            # 提示訊息
            print(f"[{'首包' if is_first_chunk else '續傳'}] 已寫入暫存區 -> SPI 位址: 0x{current_spi_addr:08X}, 大小: {run_size} bytes")

            # 通知 MCU：立起 busy_bit = 1
            mcu.write_busy_bit(1)

            # 更新計數與旗標
            bytes_sent += run_size

            # 第一包發送後，因為 MCU 要執行「全顆外部 Flash 擦除 (Erase)」，會花比較久時間
            if is_first_chunk:
                print("⚠️ 第一包已送出，MCU 正在清除整顆外部 Flash，增加等待時間...")
                time.sleep(0.5)  # 給予 3 秒寬限期（視您的 Flash 晶片大小可自行加長）
                is_first_chunk = False
            else:
                # 題目要求：每次寫入資料後固定等待 5ms
                time.sleep(0.01)

        # 最後一包送出後，仍須等待 MCU 把最後一塊寫入完成才算真正結束
        print("等待最後一包寫入完成...")
        while mcu.read_busy_bit() == 1:
            time.sleep(0.01)

        print("\n🎉 恭喜！所有 Bin 檔案資料已成功透過 pyOCD 寫入外部 SPI Flash。")

    except Exception as e:
        print(f"\n❌ 執行過程中發生錯誤: {e}")
    finally:
        mcu.close()
        print("pyOCD 連線已安全關閉。")


if __name__ == "__main__":
    # 使用範例: python script.py app.bin 0x00000000 [可選:RAM起始位址]
    if len(sys.argv) < 3:
        print("Usage: python script.py <bin_path> <spi_start_addr_hex> [ram_base_addr_hex]")
        sys.exit(1)
        
    bin_path = sys.argv[1]
    spi_addr = int(sys.argv[2], 16)
    
    # 如果有帶第三個參數就更換 RAM 基底位址，沒帶預設為 0x20000000
    ram_addr = 0x126800
    
    flash_bin_via_ipc(bin_path, spi_addr, ram_addr)
