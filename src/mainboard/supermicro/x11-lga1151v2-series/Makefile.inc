## SPDX-License-Identifier: GPL-2.0-only

bootblock-y += bootblock.c
bootblock-y += variants/$(VARIANT_DIR)/gpio.c

ramstage-y += mainboard.c
ramstage-y += variants/$(VARIANT_DIR)/gpio.c
