/* SPDX-License-Identifier: GPL-2.0-only */

#include <soc/cnl_memcfg_init.h>
#include <soc/romstage.h>

static const struct cnl_mb_cfg x11_lga1151v2_series_memcfg = {
	/* Access memory info through SMBUS */
	.spd[0] = {
		.read_type = READ_SMBUS,
		.spd_spec = { .spd_smbus_address = 0x50 << 1 },
	},
	.spd[1] = {
		.read_type = NOT_EXISTING,
		.spd_spec = { .spd_smbus_address = 0x51 << 1 },
	},
	.spd[2] = {
		.read_type = READ_SMBUS,
		.spd_spec = { .spd_smbus_address = 0x52 << 1 },
	},
	.spd[3] = {
		.read_type = NOT_EXISTING,
		.spd_spec = { .spd_smbus_address = 0x53 << 1 },
	},

	/* Rcomp resistors on CFL-S are located on the CPU itself */
	.rcomp_resistor = { 121, 75, 100 },

	/* CFL-S DDR4 2DPC Rcomp target values for RdOdt, WrDS, WrDSCmd, WrDSCtl, WrDSClk */
	.rcomp_targets = { 60, 26, 20, 20, 26 },

	/* DIMM slots are placed back-to-back */
	.dq_pins_interleaved = 1,

	/* Use DDR4 Vref mapping: VREF_CA controls CH0, VREF_DQ_B controls CH1 */
	.vref_ca_config = 2,

	/* Disable Early Command Training for DDR4: PDA is not stable before basic training */
	.ect = 0,
};

void mainboard_memory_init_params(FSPM_UPD *memupd)
{
	cannonlake_memcfg_init(&memupd->FspmConfig, &x11_lga1151v2_series_memcfg);
}
