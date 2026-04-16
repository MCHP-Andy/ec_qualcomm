
#include <zephyr/kernel.h>
#include <interface/thermal.h>

// Profile6: Best performance with charger plugged in
static fan_tbl_t p6_fan1_cpu_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};
static fan_tbl_t p6_fan1_skin_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};
static fan_tbl_t p6_fan2_cpu_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};
static fan_tbl_t p6_fan2_skin_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};

// Profile7: Best performance with charger plugged out
static fan_tbl_t p7_fan1_cpu_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};
static fan_tbl_t p7_fan1_skin_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};
static fan_tbl_t p7_fan2_cpu_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};
static fan_tbl_t p7_fan2_skin_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};

int board_lut_get(uint8_t profile, uint8_t fan_id, uint8_t tmp_src,
                  fan_tbl_t **tbl, uint8_t *len) {
    if (profile == 0 || profile > 7 || fan_id == 0 || fan_id > 2 ||
        tmp_src == 0 || tmp_src > 2 || tbl == NULL || len == NULL) {
        return -EINVAL;
    }

    uint32_t codec = (profile << 16) | (fan_id << 8) | tmp_src;

    switch (codec)
    {
    case 0x060101:
        *tbl = p6_fan1_cpu_lut;
        *len = ARRAY_SIZE(p6_fan1_cpu_lut);
        break;
    case 0x060102:
        *tbl = p6_fan1_skin_lut;
        *len = ARRAY_SIZE(p6_fan1_skin_lut);
        break;
    case 0x060201:
        *tbl = p6_fan2_cpu_lut;
        *len = ARRAY_SIZE(p6_fan2_cpu_lut);
        break;
    case 0x060202:
        *tbl = p6_fan2_skin_lut;
        *len = ARRAY_SIZE(p6_fan2_skin_lut);
        break;
    case 0x070101:
        *tbl = p7_fan1_cpu_lut;
        *len = ARRAY_SIZE(p7_fan1_cpu_lut);
        break;
    case 0x070102:
        *tbl = p7_fan1_skin_lut;
        *len = ARRAY_SIZE(p7_fan1_skin_lut);
        break;
    case 0x070201:
        *tbl = p7_fan2_cpu_lut;
        *len = ARRAY_SIZE(p7_fan2_cpu_lut);
        break;
    case 0x070202:
        *tbl = p7_fan2_skin_lut;
        *len = ARRAY_SIZE(p7_fan2_skin_lut);
        break;
    
    default:
        *tbl = NULL;
        *len = 0;
        return -EINVAL;
        break;
    }

    return 0;
}