/*
 * Copyright (c) 2019 Microchip Technology Inc.
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/pm/pm.h>
#include <soc.h>
#include <zephyr/arch/cpu.h>
#include <cmsis_core.h>
// #include "device_power.h"


#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_pm, LOG_LEVEL_INF);


#define PCR_XEC_REG_BASE (DT_REG_ADDR_BY_IDX(DT_NODELABEL(pcr), 0))

#define SYS_SLP_CTRL                0x00
#define MCHP_PCR_SYS_SLP_LIGHT		BIT(3)
#define MCHP_PCR_SYS_SLP_HEAVY		(BIT(3) | BIT(0))

#define OSC_ID 						0x0C
#define MCHP_PCR_OSC_ID_PLL_LOCK	BIT(8)

#define SLP_EN 						0x30

#define CLK_RQST 					0x50
#define CLK_RQST_LEN				(5)

/*
 * Deep Sleep
 * Pros:
 * Lower power dissipation, 48MHz PLL is off
 * Cons:
 * Longer wake latency. CPU start running on ring oscillator
 * between 16 to 25 MHz. Minimum 3ms until PLL reaches lock
 * frequency of 48MHz.
 *
 * Implementation Notes:
 * We touch the Cortex-M's primary mask and base priority registers
 * because we do not want to enter an ISR immediately upon wake.
 * We must restore any hardware state that was modified upon sleep
 * entry before allowing interrupts to be serviced. Zephyr arch level
 * does not provide API's to manipulate both primary mask and base priority.
 *
 * DEBUG NOTES:
 * If a JTAG/SWD debug probe is connected driving TRST# high and
 * possibly polling the DUT then MEC1501 will not shut off its 48MHz
 * PLL. Firmware should not disable JTAG/SWD in the EC subsystem
 * while a probe is using the interface. This can leave the JTAG/SWD
 * TAP controller in a state of requesting clocks preventing the PLL
 * from being shut off.
 */

/* NOTE: Zephyr masks interrupts using BASEPRI before calling PM subsystem */
static void z_power_soc_deep_sleep(void)
{
	// struct htmr_regs *htmr0 = HTMR_0_XEC_REG_BASE;
	uint32_t temp = 0U;

	__disable_irq();

	// soc_deep_sleep_periph_save();
	// soc_deep_sleep_wake_en();
	// soc_deep_sleep_non_wake_en();

    volatile uint32_t *clk_req = (uint32_t *)(PCR_XEC_REG_BASE + CLK_RQST);
    for (int i = 0; i < CLK_RQST_LEN; i++) {
        LOG_DBG("CLK_REQ%d: 0x%08X", i, clk_req[i]);
    }

#define MEC5_TIMER5_BASE (DT_REG_ADDR_BY_IDX(DT_NODELABEL(timer5), 0))
	#define TIMER5_CTRL_OFFSET 0x10
    volatile uint32_t *timer5_ctrl = (uint32_t *)(MEC5_TIMER5_BASE + TIMER5_CTRL_OFFSET);
    timer5_ctrl[0] &= ~BIT(5); // Disable timer5

	/*
	 * Enable deep sleep mode in CM4 and MEC172x.
	 * Enable CM4 deep sleep and sleep signals assertion on WFI.
	 * Set MCHP Heavy sleep (PLL OFF when all CLK_REQ clear) and SLEEP_ALL
	 * to assert SLP_EN to all peripherals on WFI.
	 * Set PRIMASK = 1 so on wake the CPU will not vector to any ISR.
	 * Set BASEPRI = 0 to allow any priority to wake.
	 */
	SCB->SCR |= BIT(2);
	sys_write32(MCHP_PCR_SYS_SLP_HEAVY, PCR_XEC_REG_BASE + SYS_SLP_CTRL);
	temp = sys_read32(PCR_XEC_REG_BASE + SYS_SLP_CTRL);
	sys_write32(temp, PCR_XEC_REG_BASE + SYS_SLP_CTRL);
#ifdef DEBUG_DEEP_SLEEP_CLK_REQ
	soc_debug_sleep_clk_req();
#endif
	__set_BASEPRI(0);
	__DSB();
	__WFI();	/* triggers sleep hardware */
	__NOP();
	__NOP();

	/*
	 * Clear SLEEP_ALL manually since we are not vectoring to an ISR until
	 * PM post ops. This de-asserts peripheral SLP_EN signals.
	 */
	sys_write32(0U, PCR_XEC_REG_BASE + SYS_SLP_CTRL);
	SCB->SCR &= ~BIT(2);

	do {
		temp = sys_read32(PCR_XEC_REG_BASE + OSC_ID);
		if (temp & MCHP_PCR_OSC_ID_PLL_LOCK) {
			break;
		}
	} while (1);

	// soc_deep_sleep_non_wake_dis();
	// soc_deep_sleep_wake_dis();
	// soc_deep_sleep_periph_restore();
	timer5_ctrl[0] |= BIT(5); // Enable timer5
}

/*
 * Light Sleep
 * Pros:
 * Fast wake response:
 * Cons:
 * Higher power dissipation, 48MHz PLL remains on.
 *
 * When the kernel calls this it has masked interrupt by setting NVIC BASEPRI
 * equal to a value equal to the highest Zephyr ISR priority. Only NVIC
 * exceptions will be served.
 */
static void z_power_soc_sleep(void)
{
	// struct pcr_regs *pcr = PCR_XEC_REG_BASE;

	__disable_irq();

	SCB->SCR &= ~BIT(2);
	sys_write32(MCHP_PCR_SYS_SLP_HEAVY, PCR_XEC_REG_BASE + SYS_SLP_CTRL);
	__set_BASEPRI(0);
	__DSB();
	__WFI();	/* triggers sleep hardware */
	__NOP();
	__NOP();
	sys_write32(0U, PCR_XEC_REG_BASE + SYS_SLP_CTRL);
}

/*
 * Called from pm_system_suspend(int32_t ticks) in subsys/power.c
 * For deep sleep pm_system_suspend has executed all the driver
 * power management call backs.
 */
void pm_state_set(enum pm_state state, uint8_t substate_id)
{
	ARG_UNUSED(substate_id);
	LOG_DBG("%s: %d", __func__, state);

	switch (state) {
	case PM_STATE_SUSPEND_TO_IDLE:
		LOG_DBG("Suspended to idle");
		z_power_soc_sleep();
		break;
	case PM_STATE_SUSPEND_TO_RAM:
		LOG_DBG("Suspended to RAM");
		z_power_soc_deep_sleep();
		break;
	default:
		break;
	}
}

/*
 * Zephyr PM code expects us to enabled interrupt at post op exit. Zephyr used
 * arch_irq_lock() which sets BASEPRI to a non-zero value masking interrupts at
 * >= numerical priority. MCHP z_power_soc_(deep)_sleep sets PRIMASK=1 and BASEPRI=0
 * allowing wake from any enabled interrupt and prevents the CPU from entering any
 * ISR on wake except for faults. We re-enable interrupts by undoing global disable
 * and alling irq_unlock with the same value, 0 zephyr core uses.
 */
void pm_state_exit_post_ops(enum pm_state state, uint8_t substate_id)
{
	__enable_irq();
	__ISB();
	irq_unlock(0);
}

#include <zephyr/init.h>
#include <zephyr/drivers/pinctrl.h>

static int pwr_clk_init(void) {

    PINCTRL_DT_DEFINE(DT_NODELABEL(tst_clk_out));
    const struct pinctrl_dev_config *tst_clk_out_pcfg =
        PINCTRL_DT_DEV_CONFIG_GET(DT_NODELABEL(tst_clk_out));
    int ret = pinctrl_apply_state(tst_clk_out_pcfg, PINCTRL_STATE_DEFAULT);
    if (ret < 0) {
        LOG_ERR("Failed to apply pinctrl state: %d", ret);
        return ret;
    }

    return 0;
}

SYS_INIT(pwr_clk_init, APPLICATION, 0);
