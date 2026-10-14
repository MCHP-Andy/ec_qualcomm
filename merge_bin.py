import argparse
import os

TOTAL_SIZE = 0x80000  # 512 KB
OFFSET_FILE1 = 0x00
OFFSET_FILE2 = 0x11000


def merge_binaries(file1_path, file2_path, output_path, pad_byte=0xFF):
    # 初始化指定大小的 Bytearray，填滿 Padding
    image = bytearray([pad_byte] * TOTAL_SIZE)

    # 寫入 file1.bin (Offset 0x00)
    if os.path.exists(file1_path):
        with open(file1_path, "rb") as f:
            data1 = f.read()
            max_size1 = OFFSET_FILE2 - OFFSET_FILE1
            if len(data1) > max_size1:
                print(
                    f"[警告] {file1_path} 大小 ({hex(len(data1))}) 超出 offset 0x10000 限制，將截斷多餘部分。"
                )
                data1 = data1[:max_size1]
            image[OFFSET_FILE1 : OFFSET_FILE1 + len(data1)] = data1
    else:
        raise FileNotFoundError(f"找不到檔案: {file1_path}")

    # 寫入 file2.bin (Offset 0x10000)
    if os.path.exists(file2_path):
        with open(file2_path, "rb") as f:
            data2 = f.read()
            max_size2 = TOTAL_SIZE - OFFSET_FILE2
            if len(data2) > max_size2:
                print(
                    f"[警告] {file2_path} 大小 ({hex(len(data2))}) 超出剩餘空間 ({hex(max_size2)})，將截斷多餘部分。"
                )
                data2 = data2[:max_size2]
            image[OFFSET_FILE2 : OFFSET_FILE2 + len(data2)] = data2
    else:
        raise FileNotFoundError(f"找不到檔案: {file2_path}")

    # 輸出最終 .bin 檔案
    with open(output_path, "wb") as f:
        f.write(image)

    print(
        f"成功生成 {output_path} (大小: {hex(len(image))} / {len(image)} Bytes)"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="將 file1.bin 與 file2.bin 打包為 0x40000 大小的 Binary 檔案"
    )
    parser.add_argument(
        "--file1", default="file1.bin", help="file1 路徑 (寫入 Offset 0x00)"
    )
    parser.add_argument(
        "--file2", default="file2.bin", help="file2 路徑 (寫入 Offset 0x10000)"
    )
    parser.add_argument(
        "--output", default="output.bin", help="輸出的 binary 檔名"
    )
    parser.add_argument(
        "--pad",
        type=lambda x: int(x, 0),
        default=0xFF,
        help="填補使用的 Byte（預設: 0xFF，可輸入 0x00）",
    )

    args = parser.parse_args()
    merge_binaries(args.file1, args.file2, args.output, args.pad)