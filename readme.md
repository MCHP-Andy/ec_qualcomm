# EC Qualcomm

## Zephyr toolchain
Windows Minimal: [link](https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.8/zephyr-sdk-0.16.8_windows-x86_64_minimal.7z)

arm-zephyr-eabi: [link](https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.8/toolchain_windows-x86_64_arm-zephyr-eabi.7z)


## SDK install
```
tools\script\install.bat
``` 

## Setup
```
tools\script\setup.bat
```

## Build
```
west build -p -b qual_module/mec172x_nsz app

west build -p -b qual_module/mec172x_nsz app -- -DOVERLAY_CONFIG=release.conf

west build -p -b qual_module/mec172x_nsz --sysbuild app -- -DOVERLAY_CONFIG=release.conf
source tools/script/release.sh
```

## QEMU test
```
set PATH=%PATH%;C:\Program Files\qemu
export PATH=$PATH:/c/Program\ Files/qemu

qemu-system-arm --version

west build -p -b qemu_cortex_m3 app
west build -t run

west build -p -b qemu_cortex_m3 app -t run

west build -p -b qemu_cortex_m3 --sysbuild app -- -DOVERLAY_CONFIG=release.conf
source tools/script/release.sh
```

## Flash
TDB
```
cd tools\KF_JLINK_Flash_Utility_L0100
python kf_flsh_util.py -w -f ..\..\build\zephyr\spi_image.bin
```


## Framework


