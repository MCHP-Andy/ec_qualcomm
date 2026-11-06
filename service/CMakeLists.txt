
add_subdirectory(acpi)

if(CONFIG_APP_BOOTLOADER)
    add_subdirectory(flash)
else()
    target_sources(app PRIVATE blink/service.c)

    add_subdirectory(thermal)
    add_subdirectory(fan)
    add_subdirectory(sci)
    add_subdirectory(power)
    add_subdirectory(soccp)
endif()
