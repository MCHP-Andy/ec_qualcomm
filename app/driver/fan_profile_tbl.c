
#include <stdio.h>
#include <stdint.h>
#include <errno.h>

#include <zephyr/sys/util.h>
#include <interface/fan.h>

// Profile1: Battery saver

// Profile2: Better performance with charger plugged in

// Profile3: Better performance with charger plugged out

// Profile4: Better performance with charger plugged in
static fan_tbl_t p4_fan1_cpu_lut[] = {
    {.rpm = 0x19, .temp_high = 0x4A, .temp_low = 0x00},
    {.rpm = 0x23, .temp_high = 0x4D, .temp_low = 0x47},
    {.rpm = 0x2D, .temp_high = 0x50, .temp_low = 0x4A},
    {.rpm = 0x37, .temp_high = 0x53, .temp_low = 0x4D},
    {.rpm = 0x41, .temp_high = 0x55, .temp_low = 0x50},
    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0x53},
    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
};
static fan_tbl_t p4_fan2_skin_lut[] = {
    {.rpm = 0x2D, .temp_high = 0x36, .temp_low = 0x00},
    {.rpm = 0x32, .temp_high = 0x39, .temp_low = 0x34},
    {.rpm = 0x37, .temp_high = 0x3B, .temp_low = 0x36},
    {.rpm = 0x3C, .temp_high = 0x3D, .temp_low = 0x38},
    {.rpm = 0x46, .temp_high = 0x3F, .temp_low = 0x3A},
    {.rpm = 0x50, .temp_high = 0x41, .temp_low = 0x3C},
    {.rpm = 0x5A, .temp_high = 0x43, .temp_low = 0x3E},
    {.rpm = 0x64, .temp_high = 0xFF, .temp_low = 0x40},
    {.rpm = 0x64, .temp_high = 0xFF, .temp_low = 0xFF},
    {.rpm = 0x64, .temp_high = 0xFF, .temp_low = 0xFF},
};

// Profile5: Better performance with charger plugged out
static fan_tbl_t p5_fan1_cpu_lut[] = {
    {.rpm = 0x19, .temp_high = 0x4A, .temp_low = 0x00},
    {.rpm = 0x23, .temp_high = 0x4D, .temp_low = 0x47},
    {.rpm = 0x2D, .temp_high = 0x50, .temp_low = 0x4A},
    {.rpm = 0x37, .temp_high = 0x53, .temp_low = 0x4D},
    {.rpm = 0x41, .temp_high = 0x55, .temp_low = 0x50},
    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0x53},
    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
};
static fan_tbl_t p5_fan2_skin_lut[] = {
    {.rpm = 0x2D, .temp_high = 0x36, .temp_low = 0x00},
    {.rpm = 0x32, .temp_high = 0x39, .temp_low = 0x34},
    {.rpm = 0x37, .temp_high = 0x3B, .temp_low = 0x36},
    {.rpm = 0x3C, .temp_high = 0x3D, .temp_low = 0x38},
    {.rpm = 0x46, .temp_high = 0x3F, .temp_low = 0x3A},
    {.rpm = 0x50, .temp_high = 0x41, .temp_low = 0x3C},
    {.rpm = 0x5A, .temp_high = 0x43, .temp_low = 0x3E},
    {.rpm = 0x64, .temp_high = 0xFF, .temp_low = 0x40},
    {.rpm = 0x64, .temp_high = 0xFF, .temp_low = 0xFF},
    {.rpm = 0x64, .temp_high = 0xFF, .temp_low = 0xFF},
};

// Profile6: Best performance with charger plugged in
static fan_tbl_t p6_fan1_cpu_lut[] = {
    {.rpm = 0x19, .temp_high = 0x4A, .temp_low = 0x00},
    {.rpm = 0x23, .temp_high = 0x4D, .temp_low = 0x47},
    {.rpm = 0x2D, .temp_high = 0x50, .temp_low = 0x4A},
    {.rpm = 0x37, .temp_high = 0x53, .temp_low = 0x4D},
    {.rpm = 0x41, .temp_high = 0x55, .temp_low = 0x50},
    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0x53},
    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
};
static fan_tbl_t p6_fan2_skin_lut[] = {
    {.rpm = 0x2D, .temp_high = 0x36, .temp_low = 0x00},
    {.rpm = 0x32, .temp_high = 0x39, .temp_low = 0x34},
    {.rpm = 0x37, .temp_high = 0x3B, .temp_low = 0x36},
    {.rpm = 0x3C, .temp_high = 0x3D, .temp_low = 0x38},
    {.rpm = 0x46, .temp_high = 0x3F, .temp_low = 0x3A},
    {.rpm = 0x50, .temp_high = 0x41, .temp_low = 0x3C},
    {.rpm = 0x5A, .temp_high = 0x43, .temp_low = 0x3E},
    {.rpm = 0x64, .temp_high = 0xFF, .temp_low = 0x40},
    {.rpm = 0x64, .temp_high = 0xFF, .temp_low = 0xFF},
    {.rpm = 0x64, .temp_high = 0xFF, .temp_low = 0xFF},
};

// Profile7: Best performance with charger plugged out
static fan_tbl_t p7_fan1_cpu_lut[] = {
    {.rpm = 0x19, .temp_high = 0x4A, .temp_low = 0x00},
    {.rpm = 0x23, .temp_high = 0x4D, .temp_low = 0x47},
    {.rpm = 0x2D, .temp_high = 0x50, .temp_low = 0x4A},
    {.rpm = 0x37, .temp_high = 0x53, .temp_low = 0x4D},
    {.rpm = 0x41, .temp_high = 0x55, .temp_low = 0x50},
    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0x53},
    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
    {.rpm = 0x4B, .temp_high = 0xFF, .temp_low = 0xFF},
};
static fan_tbl_t p7_fan2_skin_lut[] = {
    {.rpm = 0x2D, .temp_high = 0x36, .temp_low = 0x00},
    {.rpm = 0x32, .temp_high = 0x39, .temp_low = 0x34},
    {.rpm = 0x37, .temp_high = 0x3B, .temp_low = 0x36},
    {.rpm = 0x3C, .temp_high = 0x3D, .temp_low = 0x38},
    {.rpm = 0x46, .temp_high = 0x3F, .temp_low = 0x3A},
    {.rpm = 0x50, .temp_high = 0x41, .temp_low = 0x3C},
    {.rpm = 0x5A, .temp_high = 0x43, .temp_low = 0x3E},
    {.rpm = 0x64, .temp_high = 0xFF, .temp_low = 0x40},
    {.rpm = 0x64, .temp_high = 0xFF, .temp_low = 0xFF},
    {.rpm = 0x64, .temp_high = 0xFF, .temp_low = 0xFF},
};

int fan_tbl_get(fan_id_t profile, fan_id_t fan_id, fan_id_t tmp_src,
                fan_tbl_t **tbl, uint8_t *len) {
    if (profile < 4 || profile > 7 || fan_id == 0 || fan_id > 2 ||
        tmp_src == 0 || tmp_src > 2 || tbl == NULL || len == NULL) {
        return -EINVAL;
    }

    uint32_t codec = (profile << 16) | (fan_id << 8) | tmp_src;

    switch (codec) {
    case 0x040101:
        *tbl = p4_fan1_cpu_lut;
        *len = ARRAY_SIZE(p4_fan1_cpu_lut);
        break;
    case 0x040202:
        *tbl = p4_fan2_skin_lut;
        *len = ARRAY_SIZE(p4_fan2_skin_lut);
        break;

    case 0x050101:
        *tbl = p5_fan1_cpu_lut;
        *len = ARRAY_SIZE(p5_fan1_cpu_lut);
        break;
    case 0x050202:
        *tbl = p5_fan2_skin_lut;
        *len = ARRAY_SIZE(p5_fan2_skin_lut);
        break;

    case 0x060101:
        *tbl = p6_fan1_cpu_lut;
        *len = ARRAY_SIZE(p6_fan1_cpu_lut);
        break;
    case 0x060202:
        *tbl = p6_fan2_skin_lut;
        *len = ARRAY_SIZE(p6_fan2_skin_lut);
        break;

    case 0x070101:
        *tbl = p7_fan1_cpu_lut;
        *len = ARRAY_SIZE(p7_fan1_cpu_lut);
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
