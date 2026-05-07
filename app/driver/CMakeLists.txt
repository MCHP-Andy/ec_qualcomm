
if(CONFIG_QEMU_TARGET)
    target_sources(app PRIVATE simulator.c)
else()
    target_sources(app PRIVATE btn.c)
    target_sources(app PRIVATE fan.c)
    target_sources(app PRIVATE thermal.c)
    target_sources(app PRIVATE power_limit.c)
    target_sources(app PRIVATE acpi.c)
    target_sources_ifdef(CONFIG_PM app PRIVATE power.c)
endif()

target_sources(app PRIVATE fan_profile_tbl.c)
