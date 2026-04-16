
target_sources(app PRIVATE btn.c)

target_sources(app PRIVATE fan_qualcomm.c)

target_sources(app PRIVATE power_limit.c)
target_sources(app PRIVATE acpi.c)

target_sources_ifdef(CONFIG_PM app PRIVATE power.c)
