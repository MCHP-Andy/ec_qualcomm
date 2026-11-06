#pragma once

#include <stdbool.h>
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

#define EC_ACTIVE_COOLING_SCI_EVENT 0x05

int acpi_sci_enable_set(bool en);
int acpi_sci_enable_get(bool *en);

/**
 * @brief Queue an SCI event to be notified to the host.
 *
 * The event is stored in the SCI queue and the ACPI interrupt line is asserted
 * so the host reads it back with cmd 0x05. Events are dropped (not queued)
 * when SCI notification is disabled by the host (cmd 0x35 bit 0) or when the
 * current power state cannot service them. If the host does not read an event
 * before the notification timeout, its interrupt line is deasserted but the
 * event remains pending.
 *
 * @param sci SCI notification code.
 *
 * @retval 0 on success, or when the event is intentionally dropped.
 * @retval -EINVAL invalid SCI code.
 * @retval -ENOMSG SCI queue is full.
 */
int sci_enque(sci_t sci);
