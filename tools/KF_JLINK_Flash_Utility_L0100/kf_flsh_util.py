"""
© 2026 Microchip Technology Inc. and its subsidiaries.
You may use this software and any derivatives exclusively with
Microchip products.
THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS".
NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE,
INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY,
AND FITNESS FOR A PARTICULAR PURPOSE, OR ITS INTERACTION WITH MICROCHIP
PRODUCTS, COMBINATION WITH ANY OTHER PRODUCTS, OR USE IN ANY APPLICATION.
IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.
TO THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL
CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF
FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
MICROCHIP PROVIDES THIS SOFTWARE CONDITIONALLY UPON YOUR ACCEPTANCE
OF THESE TERMS.
"""

import os
import time
import argparse
import sys
import pylink
import configparser
from tqdm import tqdm               # Progress bar for long operations

# --- Configuration: global flags and addresses --- #
ini_flag = 0                        # Bitwise flag for device options parsed from INI
MBX_CNTRL_18V  = 1 << 18            # Use 1.8V I/O
MBX_CNTRL_WSR  = 1 << 19            # Write/clear SPI status register
MBX_CNTRL_QUAD = 1 << 20            # Use quad SPI mode

DEVICE_NAME = "Cortex-M4"           # Target MCU (update as needed)
FIRMWARE_BIN = "KF_FLSH_UTIL.bin" # PE firmware (for flash ops)

SRAM_ADDRESS_FW = 0xC8000           # SRAM start to load PE firmware
PC_START_ADDRESS = 0xC82CC          # Program Counter after load
ACK_ADDRESS = 0xE2300               # Address for mailbox synchronization
DATA_PACKET_ADDRESS = 0xE6000       # Where chunks are staged for PE

DATA_CHUNK_SIZE = 256 * 1024        # How much data to transfer per operation
FLASH_BASE_ADDRESS = 0x0            # Physical base address of flash

# These globals set after parsing arguments
FLASH_BIN = None
TOTAL_DATA_SIZE = 0

def parse_arguments():
    """
    Parses CLI arguments. Returns an argparse namespace.
    See --help for CLI syntax.
    """
    parser = argparse.ArgumentParser(description="J-Link Flash Programming Tool")
    parser.add_argument('-f', '--flash_bin', type=str, help='The binary data to program the flash')
    # Flash controlling flags
    parser.add_argument('-w', action='store_true', help='Enable write operation')
    parser.add_argument('-s', action='store_true', help='Enable Sector write operation')
    parser.add_argument('-v', action='store_true', help='Enable verify operation')
    parser.add_argument('-e', action='store_true', help='Enable erase operation')
    parser.add_argument('-p', action='store_true', help='Enable partial erase operation')
    parser.add_argument('-o', action='store_true', help='Enable program only operation')
    parser.add_argument('-r', action='store_true', help='Enable read operation')
    parser.add_argument('-l', '--length', type=lambda x: int(x, 16), help='Length of data (hex)')
    parser.add_argument('-d', '--offset', type=lambda x: int(x, 16), help='Offset address (hex)')
    parser.add_argument('-i', '--ini_file', type=str, help='.ini file for Programming configurations')
    return parser.parse_args()

def config_globals(args):
    """
    Updates global FLASH_BIN and TOTAL_DATA_SIZE based on CLI args.
    """
    global FLASH_BIN, TOTAL_DATA_SIZE
    FLASH_BIN = args.flash_bin
    TOTAL_DATA_SIZE = (
        os.path.getsize(FLASH_BIN) if FLASH_BIN and os.path.isfile(FLASH_BIN)
        else 0
    )

def process_ini_file(ini_file_path):
    """
    Parse MBX_FLAGS from the ini file.
    Sets device communication/control options for PE via ini_flag.
    """
    global ini_flag
    config = configparser.ConfigParser()
    config.read(ini_file_path)
    if "MBX_FLAGS" in config:
        section = config["MBX_FLAGS"]
        # SPI_SEL sets device selection (bits 13..15)
        spi_sel_val = section.getint("SPI_SEL", fallback=None)
        if spi_sel_val is not None and 0 <= spi_sel_val <= 6:
            ini_flag |= (spi_sel_val << 13)
        # 18V, WSR, QUAD all set individual bit flags
        if section.getboolean("18V", fallback=False):
            ini_flag |= MBX_CNTRL_18V
        if section.getboolean("WSR", fallback=False):
            ini_flag |= MBX_CNTRL_WSR
        if section.getboolean("QUAD", fallback=False):
            ini_flag |= MBX_CNTRL_QUAD
        print(f"[INFO] INI_FLAG set to: 0x{ini_flag:X}")
    else:
        print("[WARNING] MBX_FLAGS section missing in ini file.")

def load_binary_data(binary_file):
    """
    Loads binary file into bytes.
    """
    with open(binary_file, 'rb') as f:
        return f.read()

def jlink_connect():
    """
    Finds first J-Link emulator, opens connection, selects interface, and connects device.
    Returns jlink object.
    """
    jlink = pylink.JLink()
    emulators = jlink.connected_emulators()
    if not emulators:
        raise RuntimeError("No J-Link devices detected.")
    JLINK_SERIAL_NO = emulators[0].SerialNumber   # Pick first connected debugger
    jlink.open(JLINK_SERIAL_NO)
    jlink.set_tif(pylink.enums.JLinkInterfaces.SWD) # Interface may need adjustment For EVB SWD, For FPGA JTAG
    jlink.connect(DEVICE_NAME)
    return jlink

def reset_and_load_fw(jlink):
    """
    Resets the ARM core, loads PE firmware to SRAM, then starts it.
    """
    jlink.reset(halt=True)
    # Enable Reset Vector Catch (DEMCR)
    jlink.memory_write32(0xE000EDFC, [0x00000001])
    # Reset core (AIRCR register)
    jlink.memory_write32(0xE000ED0C, [0x05FA0001])
    firmware_data = load_binary_data(FIRMWARE_BIN)
    jlink.flash_write(SRAM_ADDRESS_FW, firmware_data)
    jlink.register_write(15, PC_START_ADDRESS)   # Set PC to jump to firmware
    jlink.restart()
    time.sleep(2)   # Let PE firmware startup
    jlink.halt()    # Re-halt after its init

def wait_for_ack():
    """
    Wait for mailbox acknowledgment from PE firmware.
    2 = OK, 6 = verify error
    """
    ack_received = False
    while not ack_received:
        ack_value = jlink.memory_read32(ACK_ADDRESS, 1)[0]
        if ack_value == 2:
            ack_received = True
            jlink.memory_write32(ACK_ADDRESS, [0]) # reset ack flag
        elif ack_value == 6:
            print("Verification failed")
            sys.exit()
        else:
            time.sleep(1)

def erase_operation():
    """
    PE command for CHIP Erase
    """
    command = 0x81
    if args.ini_file:
        command |= ini_flag
    if not args.ini_file and TOTAL_DATA_SIZE > 8 * 1024 * 1024:
        raise ValueError("Total Data Size Exceeds 8MB Internal Flash")
    start_time = time.time()
    jlink.memory_write32(ACK_ADDRESS, [command])
    jlink.halt()
    jlink.restart()
    print("Erase operation initiated.")
    wait_for_ack()
    elapsed_time = time.time() - start_time
    print(f"Erase operation completed in {elapsed_time:.2f} seconds.")

def load_sectors_from_cfg(cfg_path):
    """
    Reads sector definitions (start, length) from [SECTOR_LIST] section in INI config.
    Used for selective sector programming.
    """
    sectors = []
    parser = configparser.ConfigParser(allow_no_value=True, strict=False)
    parser.optionxform = str
    parser.read(cfg_path)
    if 'SECTOR_LIST' in parser:
        for key in parser['SECTOR_LIST']:
            if key.startswith("sector["):
                parts = parser['SECTOR_LIST'][key].split(',')
                sectors.append({
                    'address': int(parts[0], 0),
                    'length': int(parts[1], 0)
                })
    return sectors

def sector_write_operation():
    """
    Writes sectors as defined in the INI file using chunked binary data.
    """
    if not FLASH_BIN:
        print("No flash binary provided. Use -f to specify the binary.")
        return
    command = 0x41
    if args.ini_file:
        command |= ini_flag
    sectors = load_sectors_from_cfg(args.ini_file)
    with open(FLASH_BIN, 'rb') as f:
        for s_idx, sector in enumerate(tqdm(sectors, desc="Sector Programming", unit="sector")):
            sector_start = sector['address']
            sector_len = sector['length']
            bin_offset = sector_start - FLASH_BASE_ADDRESS
            f.seek(bin_offset)
            bytes_written = 0
            while bytes_written < sector_len:
                chunk_size = min(DATA_CHUNK_SIZE, sector_len - bytes_written)
                data_chunk = f.read(chunk_size)
                if not data_chunk:
                    break
                target_addr = sector_start + bytes_written
                # Set mailbox params: data address, flash address, length, and command
                jlink.memory_write32(0xE2300 + 8, [0xE6000])
                jlink.memory_write32(0xE2300 + 4, [target_addr])
                jlink.memory_write32(0xE2300 + 0xC, [len(data_chunk)])
                jlink.memory_write(DATA_PACKET_ADDRESS, data_chunk)
                jlink.memory_write32(0xE2300, [command])
                if bytes_written == 0 and s_idx == 0:
                    jlink.halt()
                    jlink.restart()
                wait_for_ack()
                bytes_written += chunk_size
    print("\n")

def write_operation():
    """
    Binary file chunked write to flash. INI modifies the command bits.
    """
    if not FLASH_BIN:
        print("No flash binary provided. Use -f to specify the binary.")
        return
    command = 0x41
    if args.ini_file:
        command |= ini_flag
    if not args.ini_file and TOTAL_DATA_SIZE > 8*1024*1024:
        raise ValueError("Total Data Size Exceeds 8MB Internal Flash")
    with open(FLASH_BIN, 'rb') as f:
        for i, chunk_start in enumerate(
            tqdm(range(0, TOTAL_DATA_SIZE, DATA_CHUNK_SIZE), desc="Write to Flash", unit="chunk")
        ):
            data_chunk = f.read(DATA_CHUNK_SIZE)
            if not data_chunk:
                break
            jlink.memory_write32(0xE2300 + 8, [0xE6000])
            start_sector_address = FLASH_BASE_ADDRESS + chunk_start
            jlink.memory_write32(0xE2300 + 4, [start_sector_address])
            jlink.memory_write32(0xE2300 + 0xC, [len(data_chunk)])
            jlink.memory_write(DATA_PACKET_ADDRESS, data_chunk)
            jlink.memory_write32(0xE2300, [command])
            if i == 0:
                jlink.halt()
                jlink.restart()
            wait_for_ack()
    print("\n")

def verify_operation():
    """
    Verifies content of flash against binary source file in chunks.
    """
    if not FLASH_BIN:
        print("No flash binary provided. Use -f to specify the binary.")
        return
    command = 0x101
    if args.ini_file:
        command |= ini_flag
    if not args.ini_file and TOTAL_DATA_SIZE > 4*1024*1024:
        raise ValueError("Total Data Size Exceeds 4MB Internal Flash")
    with open(FLASH_BIN, 'rb') as f:
        for i, chunk_start in enumerate(
            tqdm(range(0, TOTAL_DATA_SIZE, DATA_CHUNK_SIZE), desc="Verify to Flash", unit="chunk")
        ):
            data_chunk = f.read(DATA_CHUNK_SIZE)
            if not data_chunk:
                break
            jlink.memory_write32(0xE2300 + 8, [0xE6000])
            start_sector_address = FLASH_BASE_ADDRESS + chunk_start
            jlink.memory_write32(0xE2300 + 4, [start_sector_address])
            jlink.memory_write32(0xE2300 + 0xC, [len(data_chunk)])
            jlink.memory_write(DATA_PACKET_ADDRESS, data_chunk)
            jlink.memory_write32(0xE2300, [command])
            if i == 0:
                jlink.halt()
                jlink.restart()
            wait_for_ack()
    print("All data chunks have been successfully verified from external flash.\n")

def partial_erase_operation():
    """
    PE command for partial erase using mailbox. Requires length/offset.
    """
    command = 0x401
    if args.ini_file:
        command |= ini_flag
    if not args.length:
        print("Error: Offset (-d) and length (-l) must be specified for partial erase.")
        return
    start_time = time.time()
    jlink.memory_write32(0xE2300 + 8, [0xE6000])
    jlink.memory_write32(0xE2300 + 4, [FLASH_BASE_ADDRESS + args.offset])
    jlink.memory_write32(0xE2300 + 0xC, [args.length])
    jlink.memory_write32(0xE2300, [command])
    jlink.halt()
    jlink.restart()
    print(f"Performing partial erase at offset: 0x{args.offset:X} for length: 0x{args.length:X} bytes")
    wait_for_ack()
    elapsed_time = time.time() - start_time
    print(f"Erase operation completed in {elapsed_time:.2f} seconds.")
    print("Partial erase completed successfully.\n")

def read_operation():
    """
    Reads flash region, saving to file (if specified).
    """
    command = 0x201
    read_data = bytearray()
    if args.ini_file:
        command |= ini_flag
    offset = args.offset if args.offset is not None else 0x0
    length = args.length if args.length is not None else TOTAL_DATA_SIZE
    print(f"Reading {length} bytes from flash starting at offset 0x{offset:X}...")
    for chunk_start in tqdm(range(0, length, DATA_CHUNK_SIZE), desc="Reading Flash", unit="chunk"):
        chunk_size = min(DATA_CHUNK_SIZE, length - chunk_start)
        jlink.memory_write32(0xE2300 + 8, [0xE6000])
        start_address = FLASH_BASE_ADDRESS + offset + chunk_start
        jlink.memory_write32(0xE2300 + 4, [start_address])
        jlink.memory_write32(0xE2300 + 0xC, [chunk_size])
        jlink.memory_write32(0xE2300, [command])
        jlink.halt()
        jlink.restart()
        chunk = jlink.memory_read(0xE6000, chunk_size)
        read_data.extend(chunk)
    if args.flash_bin:
        with open(args.flash_bin, 'wb') as f:
            f.write(read_data)
        print(f"All values are read from flash and stored in {args.flash_bin}")

def program_only_operation():
    """
    PE will Programs Only no Read and Verify operations
    """
    if not FLASH_BIN:
        print("No flash binary provided. Use -f to specify the binary.")
        return
    offset = args.offset if args.offset else 0x0
    length = args.length if args.length else TOTAL_DATA_SIZE
    command = 0x801
    if args.ini_file:
        command |= ini_flag
    with open(FLASH_BIN, 'rb') as f:
        for i, chunk_start in enumerate(
            tqdm(range(0, length, DATA_CHUNK_SIZE), desc="Programming Flash", unit="chunk")
        ):
            data_chunk = f.read(DATA_CHUNK_SIZE)
            if not data_chunk:
                break
            jlink.memory_write32(0xE2300 + 8, [0xE6000])
            start_address = FLASH_BASE_ADDRESS + offset + chunk_start
            jlink.memory_write32(0xE2300 + 4, [start_address])
            jlink.memory_write32(0xE2300 + 0xC, [length])
            jlink.memory_write(DATA_PACKET_ADDRESS, data_chunk)
            jlink.memory_write32(0xE2300, [command])
            if i == 0:
                jlink.halt()
                jlink.restart()
            wait_for_ack()
    print(f"The values are written at offset {hex(offset)} of length {hex(length)}")

def main():
    """
    Script entry: parses args, sets globals, checks firmware, opens J-Link,
    performs flash operation as requested.
    """
    global args, jlink
    args = parse_arguments()
    config_globals(args)
    if args.ini_file:
        process_ini_file(args.ini_file)
    if not os.path.isfile(FIRMWARE_BIN):
        raise FileNotFoundError(f"{FIRMWARE_BIN} not found!")
    jlink = jlink_connect()
    try:
        print("KF Flash Utility version 1.0")
        reset_and_load_fw(jlink)
        # Map operation keywords to functions
        operations = {
            'erase': erase_operation,
            'write': write_operation,
            'sector_write': sector_write_operation,
            'verify': verify_operation,
            'partial_erase': partial_erase_operation,
            'read': read_operation,
            'prog_only': program_only_operation
        }
        # Dispatch as per CLI flag
        if args.e:
            operations['erase']()
        if args.w:
            operations['write']()
        if args.s:
            operations['sector_write']()
        if args.v:
            operations['verify']()
        if args.p:
            operations['partial_erase']()
        if args.r:
            operations['read']()
        if args.o:
            operations['prog_only']()
        jlink.halt()
    except pylink.errors.JLinkException as e:
        print(f"J-Link error: {e}")
    except Exception as e:
        print(f"An error occurred: {e}")
    finally:
        jlink.close()
        print("J-Link connection closed. Process completed.")

if __name__ == "__main__":
    main()