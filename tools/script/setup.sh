
OS=$(uname -s)

if [ "$OS" = "Linux" ]; then
    source .venv/bin/activate
elif [[ "$OS" =~ MINGW* ]] || [[ "$OS" =~ CYGWIN* ]]; then
    source .venv/Scripts/activate
else
    echo "未知的系統: $OS"
fi

export ZES_ENABLE_SYSMAN=1
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr

export WORKSPACE=$(pwd)
export ZEPHYR_SDK_INSTALL_DIR=/home/andy/Desktop/toolchain/zephyr-sdk-0.16.8
# export MEC5_SPI_GEN=${WORKSPACE}/tools/mec175x_spi_gen.exe
# export MEC5_SPI_CFG=${WORKSPACE}/app/boards/support/mec175x_spi_cfg.txt
export MEC5_SPI_GEN=${WORKSPACE}/tools/kf_spi_gen_revb/mec175x_spi_gen_linux_x86_64
export MEC5_SPI_CFG=${WORKSPACE}/tools/kf_spi_gen_revb/spi_cfg.ini
export EC_IMG_GEN=${WORKSPACE}/tools/spi_image_trim_out/generating_binaries.py

echo ================ Zephyr ENV =======================
echo ZES_ENABLE_SYSMAN=$ZES_ENABLE_SYSMAN
echo ZEPHYR_TOOLCHAIN_VARIANT=$ZEPHYR_TOOLCHAIN_VARIANT
echo ZEPHYR_SDK_INSTALL_DIR=$ZEPHYR_SDK_INSTALL_DIR
echo ================= MCHP ENV ========================
echo MEC5_SPI_GEN=$MEC5_SPI_GEN
echo EC_IMG_GEN=$EC_IMG_GEN
