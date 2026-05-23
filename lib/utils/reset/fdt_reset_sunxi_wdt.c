/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Samuel Holland <samuel@sholland.org>
 */

#include <libfdt.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_bitops.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_system.h>
#include <sbi_utils/fdt/fdt_driver.h>
#include <sbi_utils/fdt/fdt_helper.h>

#define WDT_KEY_VAL			0x16aa0000

#define WDT_CTRL_REG			0x10
#define WDT_CTRL_RELOAD			(BIT(0) | (0x0a57 << 1))

#define WDT_CFG_REG			0x14
#define WDT_CFG_RESET_MASK		0x03
#define WDT_CFG_RESET_ON_TIMEOUT	0x01

#define WDT_MODE_REG			0x18
#define WDT_MODE_EN			BIT(0)

static volatile char *sunxi_wdt_base;

static int sunxi_wdt_system_reset_check(u32 type, u32 reason)
{
	switch (type) {
	case SBI_SRST_RESET_TYPE_COLD_REBOOT:
	case SBI_SRST_RESET_TYPE_WARM_REBOOT:
		return 1;
	}

	return 0;
}

static void sunxi_wdt_system_reset(u32 type, u32 reason)
{
	u32 val;

	/* Set CFG register to generate reset on watchdog timeout. */
	val = readl_relaxed(sunxi_wdt_base + WDT_CFG_REG);
	val &= ~WDT_CFG_RESET_MASK;
	val |= WDT_CFG_RESET_ON_TIMEOUT;
	val |= WDT_KEY_VAL;
	writel_relaxed(val, sunxi_wdt_base + WDT_CFG_REG);

	/* Enable watchdog with the lowest timeout interval. */
	val = readl_relaxed(sunxi_wdt_base + WDT_MODE_REG);
	val &= ~(0x0f << 4);	/* Clear timeout field = shortest timeout */
	val |= WDT_MODE_EN;
	val |= WDT_KEY_VAL;
	writel_relaxed(val, sunxi_wdt_base + WDT_MODE_REG);

	/* Reload the watchdog to start the countdown. */
	writel_relaxed(WDT_CTRL_RELOAD, sunxi_wdt_base + WDT_CTRL_REG);

	/* Wait for the watchdog to expire and reset the SoC. */
	while (1)
		;
}

static struct sbi_system_reset_device sunxi_wdt_reset = {
	.name = "sunxi-wdt-reset",
	.system_reset_check = sunxi_wdt_system_reset_check,
	.system_reset = sunxi_wdt_system_reset,
};

static int sunxi_wdt_reset_init(const void *fdt, int nodeoff,
				const struct fdt_match *match)
{
	uint64_t reg_addr;
	int rc;

	rc = fdt_get_node_addr_size(fdt, nodeoff, 0, &reg_addr, NULL);
	if (rc < 0 || !reg_addr)
		return SBI_ENODEV;

	sunxi_wdt_base = (volatile char *)(unsigned long)reg_addr;

	sbi_system_reset_add_device(&sunxi_wdt_reset);

	return 0;
}

static const struct fdt_match sunxi_wdt_reset_match[] = {
	{ .compatible = "allwinner,sun20i-d1-wdt-reset" },
	{ .compatible = "allwinner,sun20i-d1-wdt" },
	{ },
};

const struct fdt_driver fdt_reset_sunxi_wdt = {
	.match_table = sunxi_wdt_reset_match,
	.init = sunxi_wdt_reset_init,
};
