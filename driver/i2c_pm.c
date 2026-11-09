/*
 * I2C sleep / wakeup switching via PM notifier.
 *
 * The in-tree Zephyr v3.7 xec-i2c-v2 driver has no PM device action, so the
 * app hooks system PM state entry/exit and drives the I2C wake registers
 * directly (ported from Microchip i2c_mchp_xec_v2.c pm_action).
 *
 * Only the acpi-i2c bus (DT alias) is used as wake source:
 *   entry: arm START-bit detect (WKCR.SBEN) and its GIRQ22 clock-wake source.
 *          CFG.ENAB and pinctrl stay active so the block can detect START
 *          while clocks are gated and auto-stretch SCL until clocks return.
 *   exit:  disarm START-bit wake. The regular GIRQ13 ISR handles the match.
 */

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/pm/pm.h>
#include <zephyr/init.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/drivers/interrupt_controller/intc_mchp_xec_ecia.h>
#include <zephyr/logging/log.h>

#include <interface/system.h>

LOG_MODULE_REGISTER(i2c_pm, LOG_LEVEL_INF);

#define XEC_I2C_SMB_WK_GIRQ   22U
#define XEC_I2C_SMB_INST_SIZE 0x400U

#define XEC_I2C_WKSR_OFS      0x60U /* wake status */
#define XEC_I2C_WKSR_SB_POS   0     /* start bit detected R/W1C */

#define XEC_I2C_WKCR_OFS      0x64U /* wake control */
#define XEC_I2C_WKCR_SBEN_POS 0     /* start bit detection enable */

/* GIRQ22 source bit: bit 0 = SPI_ASYNC_WAKE, bits 1..5 = SMB controllers 0..4 */
#define XEC_I2C_WAKE_GIRQ_POS(node) \
    (uint8_t)((((DT_REG_ADDR(node)) / XEC_I2C_SMB_INST_SIZE) & 0x7U) + 1U)

#define ACPI_I2C_NODE DT_ALIAS(acpi_i2c)

#define ACPI_I2C_BASE          ((mm_reg_t)DT_REG_ADDR(ACPI_I2C_NODE))
#define ACPI_I2C_WAKE_GIRQ_POS XEC_I2C_WAKE_GIRQ_POS(ACPI_I2C_NODE)

static void i2c_pm_state_entry(enum pm_state state) {
    sys_write8(BIT(XEC_I2C_WKSR_SB_POS), ACPI_I2C_BASE + XEC_I2C_WKSR_OFS);
    sys_write8(BIT(XEC_I2C_WKCR_SBEN_POS), ACPI_I2C_BASE + XEC_I2C_WKCR_OFS);
    mchp_xec_ecia_girq_src_clr(XEC_I2C_SMB_WK_GIRQ, ACPI_I2C_WAKE_GIRQ_POS);
    mchp_xec_ecia_girq_src_en(XEC_I2C_SMB_WK_GIRQ, ACPI_I2C_WAKE_GIRQ_POS);
}

static void i2c_pm_state_exit(enum pm_state state) {
    mchp_xec_ecia_girq_src_dis(XEC_I2C_SMB_WK_GIRQ, ACPI_I2C_WAKE_GIRQ_POS);
    sys_write8(0, ACPI_I2C_BASE + XEC_I2C_WKCR_OFS);
    sys_write8(BIT(XEC_I2C_WKSR_SB_POS), ACPI_I2C_BASE + XEC_I2C_WKSR_OFS);
    mchp_xec_ecia_girq_src_clr(XEC_I2C_SMB_WK_GIRQ, ACPI_I2C_WAKE_GIRQ_POS);
}

static struct pm_notifier i2c_pm_notifier = {
    .state_entry = i2c_pm_state_entry,
    .state_exit = i2c_pm_state_exit,
};

static int i2c_pm_init(void) {
    pm_notifier_register(&i2c_pm_notifier);
    return 0;
}

SYS_INIT(i2c_pm_init, APPLICATION, SYS_INIT_NORMAL);
