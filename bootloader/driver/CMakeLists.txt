
if(CONFIG_QEMU_TARGET)
    target_sources(app PRIVATE simulator.c)
else()
    target_sources(app PRIVATE acpi.c)
    target_sources(app PRIVATE flash.c)
endif()

target_sources(app PRIVATE storage.c)