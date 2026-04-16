
#pragma once

#define ACPI_RECE_LEN 512
#define ACPI_RESP_LEN 64

#include <stdio.h>
#include <stdint.h>

int acpi_write(uint8_t *data, uint16_t len);

int acpi_read(uint8_t *data, uint16_t len);


// TODO: Enqueue SCI event

