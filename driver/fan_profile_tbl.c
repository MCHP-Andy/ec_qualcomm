
#include <stdio.h>
#include <stdint.h>
#include <errno.h>

#include <zephyr/sys/util.h>
#include <interface/fan.h>

// Profile1: Battery saver
static fan_tbl_t p1_fan1_cpu_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};
static fan_tbl_t p1_fan1_skin_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};
static fan_tbl_t p1_fan2_cpu_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};
static fan_tbl_t p1_fan2_skin_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};

// Profile2: Better performance with charger plugged in
static fan_tbl_t p2_fan1_cpu_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};
static fan_tbl_t p2_fan1_skin_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};
static fan_tbl_t p2_fan2_cpu_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};
static fan_tbl_t p2_fan2_skin_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};

// Profile3: Better performance with charger plugged out
static fan_tbl_t p3_fan1_cpu_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};
static fan_tbl_t p3_fan1_skin_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};
static fan_tbl_t p3_fan2_cpu_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};
static fan_tbl_t p3_fan2_skin_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};

// Profile4: Better performance with charger plugged in
static fan_tbl_t p4_fan1_cpu_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};
static fan_tbl_t p4_fan1_skin_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};
static fan_tbl_t p4_fan2_cpu_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};
static fan_tbl_t p4_fan2_skin_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};

// Profile5: Better performance with charger plugged out
static fan_tbl_t p5_fan1_cpu_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};
static fan_tbl_t p5_fan1_skin_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};
static fan_tbl_t p5_fan2_cpu_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};
static fan_tbl_t p5_fan2_skin_lut[] = {
    {.rpm = 0x00, .temp_high = 0x46, .temp_low = 0x00},
    {.rpm = 0x22, .temp_high = 0x50, .temp_low = 0x43},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0x4D},
    {.rpm = 0x34, .temp_high = 0xFF, .temp_low = 0xFF},
};

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

int fan_tbl_get(fan_id_t profile, fan_id_t fan_id, fan_id_t tmp_src,
                fan_tbl_t **tbl, uint8_t *len) {
    if (profile == 0 || profile > 7 || fan_id == 0 || fan_id > 2 ||
        tmp_src == 0 || tmp_src > 2 || tbl == NULL || len == NULL) {
        return -EINVAL;
    }

    uint32_t codec = (profile << 16) | (fan_id << 8) | tmp_src;

    switch (codec) {
    case 0x010101:
        *tbl = p1_fan1_cpu_lut;
        *len = ARRAY_SIZE(p1_fan1_cpu_lut);
        break;
    case 0x010102:
        *tbl = p1_fan1_skin_lut;
        *len = ARRAY_SIZE(p1_fan1_skin_lut);
        break;
    case 0x010201:
        *tbl = p1_fan2_cpu_lut;
        *len = ARRAY_SIZE(p1_fan2_cpu_lut);
        break;
    case 0x010202:
        *tbl = p1_fan2_skin_lut;
        *len = ARRAY_SIZE(p1_fan2_skin_lut);
        break;

    case 0x020101:
        *tbl = p2_fan1_cpu_lut;
        *len = ARRAY_SIZE(p2_fan1_cpu_lut);
        break;
    case 0x020102:
        *tbl = p2_fan1_skin_lut;
        *len = ARRAY_SIZE(p2_fan1_skin_lut);
        break;
    case 0x020201:
        *tbl = p2_fan2_cpu_lut;
        *len = ARRAY_SIZE(p2_fan2_cpu_lut);
        break;
    case 0x020202:
        *tbl = p2_fan2_skin_lut;
        *len = ARRAY_SIZE(p2_fan2_skin_lut);
        break;
    
    case 0x030101:
        *tbl = p3_fan1_cpu_lut;
        *len = ARRAY_SIZE(p3_fan1_cpu_lut);
        break;
    case 0x030102:
        *tbl = p3_fan1_skin_lut;
        *len = ARRAY_SIZE(p3_fan1_skin_lut);
        break;
    case 0x030201:
        *tbl = p3_fan2_cpu_lut;
        *len = ARRAY_SIZE(p3_fan2_cpu_lut);
        break;
    case 0x030202:
        *tbl = p3_fan2_skin_lut;
        *len = ARRAY_SIZE(p3_fan2_skin_lut);
        break;

    case 0x040101:
        *tbl = p4_fan1_cpu_lut;
        *len = ARRAY_SIZE(p4_fan1_cpu_lut);
        break;
    case 0x040102:
        *tbl = p4_fan1_skin_lut;
        *len = ARRAY_SIZE(p4_fan1_skin_lut);
        break;
    case 0x040201:
        *tbl = p4_fan2_cpu_lut;
        *len = ARRAY_SIZE(p4_fan2_cpu_lut);
        break;
    case 0x040202:
        *tbl = p4_fan2_skin_lut;
        *len = ARRAY_SIZE(p4_fan2_skin_lut);
        break;

    case 0x050101:
        *tbl = p5_fan1_cpu_lut;
        *len = ARRAY_SIZE(p5_fan1_cpu_lut);
        break;
    case 0x050102:
        *tbl = p5_fan1_skin_lut;
        *len = ARRAY_SIZE(p5_fan1_skin_lut);
        break;
    case 0x050201:
        *tbl = p5_fan2_cpu_lut;
        *len = ARRAY_SIZE(p5_fan2_cpu_lut);
        break;
    case 0x050202:
        *tbl = p5_fan2_skin_lut;
        *len = ARRAY_SIZE(p5_fan2_skin_lut);
        break;

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
