
zephyr_linker_sources(SECTIONS acpi_cmd.ld)

target_sources(app PRIVATE service.c)
# FW update commands, CONFIG_APP_BOOTLOADER selects the bootloader part
target_sources(app PRIVATE ec_fw_update.c)

if(NOT CONFIG_APP_BOOTLOADER)
    target_sources(app PRIVATE ec_ver_and_cap.c)
endif()
