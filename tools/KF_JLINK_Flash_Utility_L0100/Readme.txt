J-Link Flash Utility for Microchip KF Devices
=================================================

Overview
--------
This utility programs, erases, verifies, and reads flash memory on Microchip KF devices
using a SEGGER J-Link probe. The script uses Python and requires the pylink library.
It supports flexible configuration via a .ini file.

Files
-----
- kf_flsh_util.py : Main Python script for flash operations.
- config.ini          : Example configuration file for device selection and sectors.
- KF_FLSH_UTIL.bin: Programming engine firmware binary (must be provided).
- <your_app.bin>      : Application/data binary to program into flash.

Requirements
------------
- Python 3.x
- pylink library (`pip install pylink`)
- tqdm library  (`pip install tqdm`)
- SEGGER J-Link probe and drivers
- Device programming engine firmware (provided by Microchip)

Usage
-----
Run the script via command line with desired options:

    python ts18xx_flsh_util.py -f <app_bin> [OPTIONS]

Common Options:
---------------
  -f, --flash_bin    Binary file to program to flash
  -w                 Write binary to flash
  -e                 Erase all flash on device
  -p                 Partially erase a region (requires -d and -l)
  -s                 Sector write, using sector definitions from config.ini
  -v                 Verify flash contents against binary file
  -r                 Read flash to file (as specified by -f)
  -o                 Program only (no read/verify)
  -d, --offset       Offset address (hex), e.g. -d 0x8000
  -l, --length       Length (hex), e.g. -l 0x4000
  -i, --ini_file     Path to config.ini (for device/sector configuration) : without giving this option tool will work with default configurations(ex: INT_SPI)

Examples:
---------
1. Erase complete flash:
    python ts18xx_flsh_util.py -e

2. Write a binary file using config.ini settings:
    python ts18xx_flsh_util.py -w -f app.bin -i config.ini

3. Sector-specific programming:
    python ts18xx_flsh_util.py -s -f app.bin -i config.ini

4. Partial erase of region 0x8000-0xC000:
    python ts18xx_flsh_util.py -p -d 0x8000 -l 0x4000

5. Verify programmed flash:
    python ts18xx_flsh_util.py -v -f app.bin

6. Read flash contents to file:
    python ts18xx_flsh_util.py -r -f flash_dump.bin

INI File Reference
------------------
See config.ini for syntax:

[MBX_FLAGS]
- SPI_SEL : SPI device/interface selection (0-4, see comments)
- 18V     : true/false for 1.8V communication
- WSR     : true/false to clear SPI status register after operation
- QUAD    : true/false for Quad SPI mode

[SECTOR_LIST]
- sector[<n>] = <START_ADDRESS>, <LENGTH>
  Define start address (hex) and length (hex) for each programming region.

Troubleshooting
---------------
- Ensure J-Link probe is connected and detected.
- Firmware binary must be present.
- For sector and device settings, edit config.ini as needed.