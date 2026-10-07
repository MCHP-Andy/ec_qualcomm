#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Qualcomm Glymur SC8480 - EC Main Firmware Image Packaging Tool
用於將 zephyr.bin 計算 CRC32 並附加 32-Byte Qualcomm Image Header。
"""

import argparse
import os
import struct
import sys
import zlib

# ---------------------------------------------------------------------------
# 常數與規格定義 (符合 80-79648-16 / 80-35782-87 規範)
# ---------------------------------------------------------------------------
HEADER_SIZE = 32
MAGIC_VALUE = b'ECFW'  # 0x45, 0x43, 0x46, 0x57 (Little-Endian)

# 校驗控制模式 (Validation Control)
VAL_CTRL_NONE = 0x0
VAL_CTRL_CRC = 0x1       # 預設：啟用 CRC 校驗
VAL_CTRL_CHECKSUM = 0x2  # 啟用 Checksum 校驗

def parse_version_str(ver_str: str) -> tuple[int, int, int]:
    """
    解析版本字串 'Major.Minor.Test' (例如 '0.1.13' -> (0, 1, 13))
    """
    parts = ver_str.strip().split('.')
    if len(parts) != 3:
        raise ValueError(f"版本格式錯誤: '{ver_str}'，應為 'Major.Minor.Test' (例: 0.1.13)")
    return tuple(int(part, 0) for part in parts)

def calculate_crc32(data: bytes) -> int:
    """
    計算標準 IEEE 802.3 CRC32 數值 (32-bit unsigned)
    """
    return zlib.crc32(data) & 0xFFFFFFFF

def calculate_checksum(data: bytes) -> int:
    """
    計算累加 Checksum (32-bit unsigned)
    """
    return sum(data) & 0xFFFFFFFF

def build_header(
    fw_size: int,
    load_address: int,
    fw_ver: tuple[int, int, int],
    lowest_ver: tuple[int, int, int],
    crc_or_csum: int,
    val_ctrl: int = VAL_CTRL_CRC,
    aux_size: int = 0
) -> bytes:
    """
    組裝 32 位元組的主韌體映像檔標頭 (Main Firmware Image Header)
    
    格式結構 (Little-Endian, <):
      0-3  (4B): Magic ('ECFW')
      4-7  (4B): Load Address
      8-11 (4B): Firmware Size (不含此 32B 標頭)
      12-14(3B): Firmware Version (Major, Minor, Test)
      15   (1B): Validation Control (0: None, 1: CRC, 2: Checksum)
      16-18(3B): Lowest Supported Version (Major, Minor, Test)
      19   (1B): Reserved 1 (0x00)
      20-23(4B): CRC/Checksum Signature
      24-25(2B): Aux Data Size (Fan LUT 大小)
      26-31(6B): Reserved 2 (0x00)
    """
    header_fmt = '<4s I I 3B B 3B B I H 6s'
    
    header = struct.pack(
        header_fmt,
        MAGIC_VALUE,                      # 0-3: Magic
        load_address,                     # 4-7: Load Address
        fw_size,                          # 8-11: FW Size
        *fw_ver,                         # 12-14: Version (Major, Minor, Test)
        val_ctrl,                         # 15: Validation Control
        *lowest_ver,                      # 16-18: Lowest Supported Version
        0x00,                             # 19: Reserved 1
        crc_or_csum,                      # 20-23: CRC32 / Checksum
        aux_size,                         # 24-25: Aux Data Size
        b'\x00' * 6                       # 26-31: Reserved 2
    )

    if len(header) != HEADER_SIZE:
        raise RuntimeError(f"Header 長度異常: {len(header)} Bytes (預期 32 Bytes)")

    return header

def pack_firmware(
    input_bin: str,
    output_bin: str,
    load_addr: int,
    version: str,
    lowest_ver: str,
    val_mode: str,
    aux_bin: str | None = None
) -> None:
    if not os.path.isfile(input_bin):
        print(f"[錯誤] 找不到輸入檔案: {input_bin}", file=sys.stderr)
        sys.exit(1)

    # 1. 讀取原始 zephyr.bin
    with open(input_bin, 'rb') as f:
        fw_payload = f.read()

    fw_size = len(fw_payload)
    if fw_size == 0:
        print("[錯誤] 輸入二進制檔大小為 0", file=sys.stderr)
        sys.exit(1)

    # 2. 讀取可選的 Aux Data (如 Fan LUT 資料)
    aux_payload = b''
    if aux_bin and os.path.isfile(aux_bin):
        with open(aux_bin, 'rb') as f:
            aux_payload = f.read()
    aux_size = len(aux_payload)

    # 3. 解析版本號
    major, minor, test = parse_version_str(version)
    ls_major, ls_minor, ls_test = parse_version_str(lowest_ver)

    # 4. 計算簽章 (CRC32 或 Checksum)
    if val_mode.lower() == 'crc':
        val_ctrl = VAL_CTRL_CRC
        signature = calculate_crc32(fw_payload)
        sig_type_str = "CRC32"
    elif val_mode.lower() == 'checksum':
        val_ctrl = VAL_CTRL_CHECKSUM
        signature = calculate_checksum(fw_payload)
        sig_type_str = "Checksum"
    else:
        val_ctrl = VAL_CTRL_NONE
        signature = 0x00000000
        sig_type_str = "None"

    # 5. 生成 32 位元組 Header
    header = build_header(
        fw_size=fw_size,
        load_address=load_addr,
        fw_ver=(major, minor, test),
        lowest_ver=(ls_major, ls_minor, ls_test),
        crc_or_csum=signature,
        val_ctrl=val_ctrl,
        aux_size=aux_size
    )

    # 6. 合併組裝最終 Image (Header + Zephyr Firmware + Aux Data)
    final_image = header + fw_payload + aux_payload

    # 輸出目標檔案
    os.makedirs(os.path.dirname(os.path.abspath(output_bin)), exist_ok=True)
    with open(output_bin, 'wb') as f:
        f.write(final_image)

    # 7. 列印打包資訊
    print("==================================================")
    print(" Qualcomm Glymur EC Firmware 打包完成")
    print("==================================================")
    print(f" 輸入檔案          : {input_bin} ({fw_size} Bytes)")
    if aux_size > 0:
        print(f" 附加資料 (Aux)    : {aux_bin} ({aux_size} Bytes)")
    print(f" 輸出映像檔        : {output_bin} ({len(final_image)} Bytes)")
    print("------------------ Header 內容 -------------------")
    print(f" Magic             : {MAGIC_VALUE.decode('ascii')}")
    print(f" RAM Load Address  : 0x{load_addr:08X}")
    print(f" Firmware Size     : {fw_size} Bytes (0x{fw_size:08X})")
    print(f" Firmware Version  : {major}.{minor}.{test} (Hex: {major:02X}.{minor:02X}.{test:02X})")
    print(f" Lowest Version    : {ls_major}.{ls_minor}.{ls_test} (Hex: {ls_major:02X}.{ls_minor:02X}.{ls_test:02X})")
    print(f" Validation Mode   : {sig_type_str} (Ctrl = 0x{val_ctrl:02X})")
    print(f" {sig_type_str:<17} : 0x{signature:08X}")
    print(f" Aux Data Size     : {aux_size} Bytes")
    print("==================================================")

def main():
    parser = argparse.ArgumentParser(
        description="Qualcomm SC8480 EC Main Firmware Image Header Generator"
    )
    parser.add_argument(
        "-i", "--input", default="zephyr.bin",
        help="輸入原始 Zephyr binary 路徑 (預設: zephyr.bin)"
    )
    parser.add_argument(
        "-o", "--output", default="ec_main_fw_packaged.bin",
        help="輸出打包後的二進制檔路徑 (預設: ec_main_fw_packaged.bin)"
    )
    parser.add_argument(
        "-a", "--load-addr", type=lambda x: int(x, 0), default=0x000C0000,
        help="EC RAM 載入起始位址 (預設: 0x000C0000 - DEC155x Program RAM 起始位址)"
    )
    parser.add_argument(
        "-v", "--version", default="0.1.13",
        help="韌體版本號 'Major.Minor.Test' (預設: 0.1.13)"
    )
    parser.add_argument(
        "-l", "--lowest-version", default="0.1.0",
        help="最低防回滾版本 'Major.Minor.Test' (預設: 0.1.0)"
    )
    parser.add_argument(
        "-m", "--mode", choices=["crc", "checksum", "none"], default="crc",
        help="校驗模式 (預設: crc)"
    )
    parser.add_argument(
        "--aux", default=None,
        help="選擇性附加於 Main FW 後方的 Aux binary 路徑 (例: fan_lut.bin)"
    )

    args = parser.parse_args()
    pack_firmware(
        input_bin=args.input,
        output_bin=args.output,
        load_addr=args.load_addr,
        version=args.version,
        lowest_ver=args.lowest_version,
        val_mode=args.mode,
        aux_bin=args.aux
    )

if __name__ == "__main__":
    main()


"""

```bash
# 基本用法（使用預設值：版本 0.1.13、最低版本 0.1.0、位址 0x000C0000、計算 CRC32）
python pack_ec_image.py -i build/zephyr/zephyr.bin -o build/zephyr/WoS_EC_MainFirmware_Release.bin

# 自訂版本號與載入位址
python pack_ec_image.py \
  -i build/zephyr/zephyr.bin \
  -o build/zephyr/WoS_EC_MainFirmware_Release.bin \
  -a 0x000C0000 \
  -v 0.1.15 \
  -l 0.1.10 \
  -m crc
```

```cmake
# 當 zephyr.bin 生成後自動調用 Python 打包腳本
set(EC_PACK_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/scripts/pack_ec_image.py")
set(EC_FW_VER "0.1.13")
set(EC_FW_LOWEST_VER "0.1.0")
set(EC_LOAD_ADDR "0x000C0000")
set(PACKAGED_BIN "${CMAKE_BINARY_DIR}/zephyr/WoS_EC_MainFirmware_Release.bin")

add_custom_command(
    TARGET zephyr_final
    POST_BUILD
    COMMAND ${PYTHON_EXECUTABLE} ${EC_PACK_SCRIPT}
            -i "${CMAKE_BINARY_DIR}/zephyr/zephyr.bin"
            -o "${PACKAGED_BIN}"
            -a "${EC_LOAD_ADDR}"
            -v "${EC_FW_VER}"
            -l "${EC_FW_LOWEST_VER}"
            -m "crc"
    COMMENT "Prepend 32-Byte Qualcomm Image Header and CRC32 to zephyr.bin"
)
```
"""