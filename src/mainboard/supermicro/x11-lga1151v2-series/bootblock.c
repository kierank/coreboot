/* SPDX-License-Identifier: GPL-2.0-only */

#include <bootblock_common.h>
#include <superio/aspeed/ast2400/ast2400.h>
#include <superio/aspeed/common/aspeed.h>

#include "gpio.h"

static void early_config_superio(void)
{
	const pnp_devfn_t serial_dev = PNP_DEV(0x2e, AST2400_SUART1);
	if (CONFIG(CONSOLE_SERIAL))
		aspeed_enable_serial(serial_dev, CONFIG_TTYS0_BASE);
}

void bootblock_mainboard_early_init(void)
{
	program_early_gpio_pads();
	early_config_superio();
}
