
target_sources(app PRIVATE blink/service.c)

add_subdirectory(thermal)
add_subdirectory(fan)
add_subdirectory(acpi)
add_subdirectory(power)
