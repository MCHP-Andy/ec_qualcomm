
if(CONFIG_QEMU_TARGET)
    target_sources(app PRIVATE simulator.c)
else()
    target_sources(app PRIVATE btn.c)
    target_sources(app PRIVATE fan.c)
    target_sources(app PRIVATE thermal.c)
    target_sources(app PRIVATE aoss.c)
    target_sources(app PRIVATE acpi.c)
endif()

target_sources(app PRIVATE fan_profile_tbl.c)

target_sources_ifdef(CONFIG_PM app PRIVATE power.c)
target_sources(app PRIVATE storage.c)
target_sources_ifdef(CONFIG_SHELL app PRIVATE aoss_shell.c)
