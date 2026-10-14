
add_subdirectory(acpi)

if(NOT CONFIG_EC_BOOTLOADER)
    target_sources(app PRIVATE blink/service.c)

    add_subdirectory(sci)
    add_subdirectory(thermal)
    add_subdirectory(fan)
    add_subdirectory(power)
    add_subdirectory(soccp)
endif()
