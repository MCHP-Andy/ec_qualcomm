
call .venv\Scripts\activate.bat

set ZES_ENABLE_SYSMAN=1
set ZEPHYR_TOOLCHAIN_VARIANT=zephyr

set WORKSPACE=%cd%
set ZEPHYR_SDK_INSTALL_DIR=%WORKSPACE%\zephyr-sdk-0.16.8
set MEC5_SPI_GEN=%WORKSPACE%\tools\mec175x_spi_gen.exe
set MEC5_SPI_CFG=%WORKSPACE%\app\boards\support\mec175x_spi_cfg.txt
set EC_IMG_GEN=%WORKSPACE%\tools\spi_image_trim_out\generating_binaries.py

zephyr_rtos\zephyr-env.cmd


echo ================ Zephyr ENV =======================
echo ZES_ENABLE_SYSMAN=%ZES_ENABLE_SYSMAN%
echo ZEPHYR_TOOLCHAIN_VARIANT=%ZEPHYR_TOOLCHAIN_VARIANT%
echo ZEPHYR_SDK_INSTALL_DIR=%ZEPHYR_SDK_INSTALL_DIR%
echo ================= MCHP ENV ========================
echo MEC5_SPI_GEN=%MEC5_SPI_GEN%
echo EC_IMG_GEN=%EC_IMG_GEN%
