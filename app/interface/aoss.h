/*
 * @Author: andy.chang
 * @Date: 2026-10-12 10:00:00
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-10-12 10:00:00
 */

/*
 * @Description: EC-AOSS interface (Ref: EC-AOSS Interface Specification, page 63~66)
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/*
 * Command byte (EC -> SoC)
 *   Bit 0   : Direction (1 - Read, 0 - Write)
 *   Bit 1-2 : Reserved
 *   Bit 3-7 : Byte count (0x00 - 1 byte, 0x1F - 32 bytes)
 * 0x19 => Read, 4 bytes
 */
#define AOSS_CMD_DIR_READ      (1U << 0)
#define AOSS_CMD_BYTE_CNT(n)   ((((n) - 1U) & 0x1FU) << 3)
#define AOSS_CMD_READ_4B       (AOSS_CMD_DIR_READ | AOSS_CMD_BYTE_CNT(4)) // 0x19

#define AOSS_REQ_LEN  5 // Command (1 byte) + Address (4 bytes, LSB first)
#define AOSS_RESP_LEN 4 // Register value (4 bytes, LSB first)

/* AOSS register address */
#define AOSS_REG_SOC_STATUS   0x4700U
#define AOSS_REG_SOC_AVG_TEMP 0x8004U

/* SoC status register bits */
#define AOSS_STA_SOC_TEMP_TELEM  (1U << 0)
#define AOSS_STA_PLAT_TEMP_TELEM (1U << 1)
#define AOSS_STA_SOC_ENGY_TELEM  (1U << 2)
#define AOSS_STA_PLAT_ENGY_TELEM (1U << 3)
#define AOSS_STA_LMT_TELEM       (1U << 4)
#define AOSS_STA_LMT_CAP         (1U << 5)
#define AOSS_STA_MASK            (0x3FU) // Bit 6 to 31 are reserved

typedef struct aoss_status_t {
    uint32_t raw;            // Raw value of SoC status register

    bool soc_temp_telem;     // SoC Temperature Telemetry Status
    bool plat_temp_telem;    // Platform Temperature Telemetry Status
    bool soc_engy_telem;     // SoC Energy Telemetry Status
    bool plat_engy_telem;    // Platform Energy Telemetry Status
    bool lmt_telem;          // Limit Telemetry Status
    bool lmt_cap;            // Limit Capability Status
} aoss_status_t;

static inline void aoss_status_parse(uint32_t raw, aoss_status_t *sta) {
    sta->raw = raw;
    sta->soc_temp_telem = !!(raw & AOSS_STA_SOC_TEMP_TELEM);
    sta->plat_temp_telem = !!(raw & AOSS_STA_PLAT_TEMP_TELEM);
    sta->soc_engy_telem = !!(raw & AOSS_STA_SOC_ENGY_TELEM);
    sta->plat_engy_telem = !!(raw & AOSS_STA_PLAT_ENGY_TELEM);
    sta->lmt_telem = !!(raw & AOSS_STA_LMT_TELEM);
    sta->lmt_cap = !!(raw & AOSS_STA_LMT_CAP);
}

/*
 * SoC average temperature register
 *   Bit 0-15  : Tj_avg, 2's complement, resolution 0.1 C
 *   Bit 16-31 : Reserved
 */
static inline int16_t aoss_temp_to_dc(uint32_t raw) {
    return (int16_t)(raw & 0xFFFFU);
}

/* 0.1 C => C, rounded half away from zero */
static inline int16_t aoss_dc_to_c(int16_t dc) {
    return (int16_t)((dc >= 0) ? ((dc + 5) / 10) : ((dc - 5) / 10));
}

/**
 * @brief Read the SoC status register from AOSS.
 *
 * @param sta Output for the parsed status.
 * @retval 0 on success, negative errno on failure.
 */
int aoss_status_get(aoss_status_t *sta);

/**
 * @brief Read the SoC average temperature from AOSS.
 *
 * Caller should check aoss_status_t.soc_temp_telem before polling.
 *
 * @param temp_c  Output for temperature in C (rounded), NULL to ignore.
 * @param temp_dc Output for temperature in 0.1 C (Eg: 251 => 25.1 C), NULL to ignore.
 * @retval 0 on success, negative errno on failure.
 */
int aoss_temp_get(int16_t *temp_c, int16_t *temp_dc);
