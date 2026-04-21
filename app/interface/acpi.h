
#pragma once

#define ACPI_RECE_LEN 512
#define ACPI_RESP_LEN 64

#include <stdio.h>
#include <stdint.h>

typedef enum {
    SCI_NONE = 0x00,

    SCI_FAN1_STA_CHG = 0x30,
    SCI_FAN2_STA_CHG = 0x31,

    SCI_FAN1_RPM_CROSS = 0x32,
    SCI_FAN2_RPM_CROSS = 0x33,

    SCI_LUT_SET = 0x34,
    SCI_FAN_PROFILE_SW = 0x35,

    SCI_EC_THERM1_CROSS = 0x36,
    SCI_EC_THERM2_CROSS = 0x37,
    SCI_EC_THERM3_CROSS = 0x38,

    SCI_EC_RST = 0x3D,
} sci_t;

int acpi_write(uint8_t *data, uint16_t len);

int acpi_read(uint8_t *data, uint16_t len);

int acpi_sci_enable_set(bool en);
int acpi_sci_enable_get(bool *en);

int acpi_sci_put(sci_t sci);
int acpi_sci_get(sci_t * psci);

// TODO: Enqueue SCI event

