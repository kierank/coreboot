/* SPDX-License-Identifier: GPL-2.0-only */

#include <device/device.h>
#include "gpio.h"

struct chip_operations mainboard_ops = {
	.init = program_gpio_pads,
};
