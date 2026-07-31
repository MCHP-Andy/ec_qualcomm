
zephyr_linker_sources(SECTIONS acpi_cmd.ld)

target_sources(app PRIVATE service.c)
target_sources(app PRIVATE ec_ver_and_cap.c)
target_sources(app PRIVATE ec_act_cool.c)
target_sources(app PRIVATE ec_fw_update.c)
