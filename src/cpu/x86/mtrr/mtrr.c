/*
 * mtrr.c: setting MTRR to decent values for cache initialization on P6
 *
 * Derived from intel_set_mtrr in intel_subr.c and mtrr.c in linux kernel
 *
 * Copyright 2000 Silicon Integrated System Corporation
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 * Reference: Intel Architecture Software Developer's Manual, Volume 3: System Programming
 */

/*
        2005.1 yhlu add NC support to spare mtrrs for 64G memory above installed
	2005.6 Eric add address bit in x86_setup_mtrrs
	2005.6 yhlu split x86_setup_var_mtrrs and x86_setup_fixed_mtrrs,
		for AMD, it will not use x86_setup_fixed_mtrrs
*/

#include <stddef.h>
#include <console/console.h>
#include <device/device.h>
#include <cpu/x86/msr.h>
#include <cpu/x86/mtrr.h>
#include <cpu/x86/cache.h>

static unsigned int mtrr_msr[] = {
	MTRRfix64K_00000_MSR, MTRRfix16K_80000_MSR, MTRRfix16K_A0000_MSR,
	MTRRfix4K_C0000_MSR, MTRRfix4K_C8000_MSR, MTRRfix4K_D0000_MSR, MTRRfix4K_D8000_MSR,
	MTRRfix4K_E0000_MSR, MTRRfix4K_E8000_MSR, MTRRfix4K_F0000_MSR, MTRRfix4K_F8000_MSR,
};


void enable_fixed_mtrr(void)
{
	msr_t msr;

	msr = rdmsr(MTRRdefType_MSR);
	msr.lo |= 0xc00;
	wrmsr(MTRRdefType_MSR, msr);
}

static void enable_var_mtrr(void)
{
	msr_t msr;

	msr = rdmsr(MTRRdefType_MSR);
	msr.lo |= 0x800;
	wrmsr(MTRRdefType_MSR, msr);
}

/* setting variable mtrr, comes from linux kernel source */
static void set_var_mtrr(
	unsigned int reg, unsigned long basek, unsigned long sizek, 
	unsigned char type, unsigned address_bits)
{
	msr_t base, mask;
	unsigned address_mask_high;

        if (reg >= 8)
                return;

        // it is recommended that we disable and enable cache when we
        // do this.
        if (sizek == 0) {
        	disable_cache();
	
                msr_t zero;
                zero.lo = zero.hi = 0;
                /* The invalid bit is kept in the mask, so we simply clear the
                   relevant mask register to disable a range. */
                wrmsr (MTRRphysMask_MSR(reg), zero);

        	enable_cache();
		return;
        }


	address_mask_high = ((1u << (address_bits - 32u)) - 1u);

	base.hi = basek >> 22;
	base.lo  = basek << 10;

	printk_spew("ADDRESS_MASK_HIGH=%#x\n", address_mask_high);

	if (sizek < 4*1024*1024) {
		mask.hi = address_mask_high;
		mask.lo = ~((sizek << 10) -1);
	}
	else {
		mask.hi = address_mask_high & (~((sizek >> 22) -1));
		mask.lo = 0;
	}

	// it is recommended that we disable and enable cache when we 
	// do this. 
	disable_cache();

	/* Bit 32-35 of MTRRphysMask should be set to 1 */
	base.lo |= type;
	mask.lo |= 0x800;
	wrmsr (MTRRphysBase_MSR(reg), base);
	wrmsr (MTRRphysMask_MSR(reg), mask);

	enable_cache();
}

/* fms: find most sigificant bit set, stolen from Linux Kernel Source. */
static inline unsigned int fms(unsigned int x)
{
	int r;

	__asm__("bsrl %1,%0\n\t"
	        "jnz 1f\n\t"
	        "movl $0,%0\n"
	        "1:" : "=r" (r) : "g" (x));
	return r;
}

/* fms: find least sigificant bit set */
static inline unsigned int fls(unsigned int x)
{
	int r;

	__asm__("bsfl %1,%0\n\t"
	        "jnz 1f\n\t"
	        "movl $32,%0\n"
	        "1:" : "=r" (r) : "g" (x));
	return r;
}

/* setting up variable and fixed mtrr
 *
 * From Intel Vol. III Section 9.12.4, the Range Size and Base Alignment has some kind of requirement:
 *	1. The range size must be 2^N byte for N >= 12 (i.e 4KB minimum).
 *	2. The base address must be 2^N aligned, where the N here is equal to the N in previous
 *	   requirement. So a 8K range must be 8K aligned not 4K aligned.
 *
 * These requirement is meet by "decompositing" the ramsize into Sum(Cn * 2^n, n = [0..N], Cn = [0, 1]).
 * For Cm = 1, there is a WB range of 2^m size at base address Sum(Cm * 2^m, m = [N..n]).
 * A 124MB (128MB - 4MB SMA) example:
 * 	ramsize = 124MB == 64MB (at 0MB) + 32MB (at 64MB) + 16MB (at 96MB ) + 8MB (at 112MB) + 4MB (120MB).
 * But this wastes a lot of MTRR registers so we use another more "aggresive" way with Uncacheable Regions.
 *
 * In the Uncacheable Region scheme, we try to cover the whole ramsize by one WB region as possible,
 * If (an only if) this can not be done we will try to decomposite the ramesize, the mathematical formula
 * whould be ramsize = Sum(Cn * 2^n, n = [0..N], Cn = [-1, 0, 1]). For Cn = -1, a Uncachable Region is used.
 * The same 124MB example:
 * 	ramsize = 124MB == 128MB WB (at 0MB) + 4MB UC (at 124MB)
 * or a 156MB (128MB + 32MB - 4MB SMA) example:
 *	ramsize = 156MB == 128MB WB (at 0MB) + 32MB WB (at 128MB) + 4MB UC (at 156MB)
 */
/* 2 MTRRS are reserved for the operating system */
#if 0
#define BIOS_MTRRS 6
#define OS_MTRRS   2
#else
#define BIOS_MTRRS 8
#define OS_MTRRS   0
#endif
#define MTRRS        (BIOS_MTRRS + OS_MTRRS)


static void set_fixed_mtrrs(unsigned int first, unsigned int last, unsigned char type)
{
	unsigned int i;
	unsigned int fixed_msr = NUM_FIXED_RANGES >> 3;
	msr_t msr;
	msr.lo = msr.hi = 0; /* Shut up gcc */
	for(i = first; i < last; i++) {
		/* When I switch to a new msr read it in */
		if (fixed_msr != i >> 3) {
			/* But first write out the old msr */
			if (fixed_msr < (NUM_FIXED_RANGES >> 3)) {
				disable_cache();
				wrmsr(mtrr_msr[fixed_msr], msr);
				enable_cache();
			}
			fixed_msr = i>>3;
			msr = rdmsr(mtrr_msr[fixed_msr]);
		}
		if ((i & 7) < 4) {
			msr.lo &= ~(0xff << ((i&3)*8));
			msr.lo |= type << ((i&3)*8);
		} else {
			msr.hi &= ~(0xff << ((i&3)*8));
			msr.hi |= type << ((i&3)*8);
		}
	}
	/* Write out the final msr */
	if (fixed_msr < (NUM_FIXED_RANGES >> 3)) {
		disable_cache();
		wrmsr(mtrr_msr[fixed_msr], msr);
		enable_cache();
	}
}

static unsigned fixed_mtrr_index(unsigned long addrk)
{
	unsigned index;
	index = (addrk - 0) >> 6;
	if (index >= 8) {
		index = ((addrk - 8*64) >> 4) + 8;
	}
	if (index >= 24) {
		index = ((addrk - (8*64 + 16*16)) >> 2) + 24;
	}
	if (index > NUM_FIXED_RANGES) {
		index = NUM_FIXED_RANGES;
	}
	return index;
}

static unsigned int range_to_mtrr(unsigned int reg, 
	unsigned long range_startk, unsigned long range_sizek,
	unsigned long next_range_startk, unsigned char type, unsigned address_bits)
{
	if (!range_sizek) {
		printk_debug("range_to_mtrr called for empty range\n");
		return reg;
	}
	if (reg >= BIOS_MTRRS) {
		printk_err("Running out of variable MTRRs!\n");
		return reg;
	}
	while(range_sizek) {
		unsigned long max_align, align;
		unsigned long sizek;
		/* Compute the maximum size I can make a range */
		max_align = fls(range_startk);
		align = fms(range_sizek); 
		if (align > max_align) {
			align = max_align;
		}
		sizek = 1 << align;
		printk_debug("Setting variable MTRR %d, base: %4ldMB, range: %4ldMB, type %s\n",
			reg, range_startk >>10, sizek >> 10,
			(type==MTRR_TYPE_UNCACHEABLE)?"UC":
			    ((type==MTRR_TYPE_WRBACK)?"WB":"Other")
			);
		set_var_mtrr(reg++, range_startk, sizek, type, address_bits);
		range_startk += sizek;
		range_sizek -= sizek;
		if (reg >= BIOS_MTRRS) {
			printk_err("Running out of variable MTRRs!\n");
			break;
		}
	}
	return reg;
}

static unsigned long resk(uint64_t value) 
{
	unsigned long resultk;
	if (value < (1ULL << 42)) {
		resultk = value >> 10;
	}
	else {
		resultk = 0xffffffff;
	}
	return resultk;
}

static void set_fixed_mtrr_resource(void *gp, struct device *dev, struct resource *res)
{
	unsigned int start_mtrr;
	unsigned int last_mtrr;
	start_mtrr = fixed_mtrr_index(resk(res->base));
	last_mtrr  = fixed_mtrr_index(resk((res->base + res->size)));
	if (start_mtrr >= NUM_FIXED_RANGES) {
		return;
	}
	printk_debug("Setting fixed MTRRs(%d-%d) Type: WB\n",
		start_mtrr, last_mtrr);
	set_fixed_mtrrs(start_mtrr, last_mtrr, MTRR_TYPE_WRBACK);
	
}

#ifndef CONFIG_VAR_MTRR_HOLE
#define CONFIG_VAR_MTRR_HOLE 1
#endif

struct var_mtrr_state {
	unsigned long range_startk, range_sizek;
	unsigned int reg;
#if CONFIG_VAR_MTRR_HOLE
	unsigned long hole_startk, hole_sizek;
#endif
	unsigned address_bits;
};

void set_var_mtrr_resource(void *gp, struct device *dev, struct resource *res)
{
	struct var_mtrr_state *state = gp;
	unsigned long basek, sizek;
	if (state->reg >= BIOS_MTRRS)
		return;
	basek = resk(res->base);
	sizek = resk(res->size);
	/* See if I can merge with the last range
	 * Either I am below 1M and the fixed mtrrs handle it, or
	 * the ranges touch.
	 */
	if ((basek <= 1024) || (state->range_startk + state->range_sizek == basek)) {
		unsigned long endk = basek + sizek;
		state->range_sizek = endk - state->range_startk;
		return;
	}
	/* Write the range mtrrs */
	if (state->range_sizek != 0) {
#if CONFIG_VAR_MTRR_HOLE
		if (state->hole_sizek == 0) {
			/* We need to put that on to hole */
			unsigned long endk = basek + sizek;
			state->hole_startk = state->range_startk + state->range_sizek;
			state->hole_sizek  = basek - state->hole_startk;
			state->range_sizek = endk - state->range_startk;
			return;
		}
#endif
		state->reg = range_to_mtrr(state->reg, state->range_startk, 
			state->range_sizek, basek, MTRR_TYPE_WRBACK, state->address_bits);
#if CONFIG_VAR_MTRR_HOLE
		state->reg = range_to_mtrr(state->reg, state->hole_startk, 
			state->hole_sizek, basek,  MTRR_TYPE_UNCACHEABLE, state->address_bits);
#endif
		state->range_startk = 0;
		state->range_sizek = 0;
#if CONFIG_VAR_MTRR_HOLE
                state->hole_startk = 0;
                state->hole_sizek = 0;
#endif
	}
	/* Allocate an msr */  
	printk_spew(" Allocate an msr - basek = %08x, sizek = %08x,\n", basek, sizek);
	state->range_startk = basek;
	state->range_sizek  = sizek;
}

void x86_setup_fixed_mtrrs(void)
{
        /* Try this the simple way of incrementally adding together
         * mtrrs.  If this doesn't work out we can get smart again 
         * and clear out the mtrrs.
         */

        printk_debug("\n");
        /* Initialized the fixed_mtrrs to uncached */
        printk_debug("Setting fixed MTRRs(%d-%d) Type: UC\n",
	        0, NUM_FIXED_RANGES);
        set_fixed_mtrrs(0, NUM_FIXED_RANGES, MTRR_TYPE_UNCACHEABLE);

        /* Now see which of the fixed mtrrs cover ram.
                 */
        search_global_resources(
		IORESOURCE_MEM | IORESOURCE_CACHEABLE, IORESOURCE_MEM | IORESOURCE_CACHEABLE,
		set_fixed_mtrr_resource, NULL);
        printk_debug("DONE fixed MTRRs\n");

        /* enable fixed MTRR */
        printk_spew("call enable_fixed_mtrr()\n");
        enable_fixed_mtrr();

}
void x86_setup_var_mtrrs(unsigned address_bits)
/* this routine needs to know how many address bits a given processor
 * supports.  CPUs get grumpy when you set too many bits in 
 * their mtrr registers :(  I would generically call cpuid here
 * and find out how many physically supported but some cpus are
 * buggy, and report more bits then they actually support.
 */
{
	/* Try this the simple way of incrementally adding together
	 * mtrrs.  If this doesn't work out we can get smart again 
	 * and clear out the mtrrs.
	 */
	struct var_mtrr_state var_state;

	/* Cache as many memory areas as possible */
	/* FIXME is there an algorithm for computing the optimal set of mtrrs? 
	 * In some cases it is definitely possible to do better.
	 */
	var_state.range_startk = 0;
	var_state.range_sizek = 0;
#if CONFIG_VAR_MTRR_HOLE
	var_state.hole_startk = 0;
	var_state.hole_sizek = 0;
#endif
	var_state.reg = 0;
	var_state.address_bits = address_bits;
	search_global_resources(
		IORESOURCE_MEM | IORESOURCE_CACHEABLE, IORESOURCE_MEM | IORESOURCE_CACHEABLE,
		set_var_mtrr_resource, &var_state);

	/* Write the last range */
	var_state.reg = range_to_mtrr(var_state.reg, var_state.range_startk, 
		var_state.range_sizek, 0, MTRR_TYPE_WRBACK, var_state.address_bits);
#if CONFIG_VAR_MTRR_HOLE
	var_state.reg = range_to_mtrr(var_state.reg, var_state.hole_startk,
		var_state.hole_sizek,  0, MTRR_TYPE_UNCACHEABLE, var_state.address_bits);
#endif
	printk_debug("DONE variable MTRRs\n");
	printk_debug("Clear out the extra MTRR's\n");
	/* Clear out the extra MTRR's */
	while(var_state.reg < MTRRS) {
		set_var_mtrr(var_state.reg++, 0, 0, 0, var_state.address_bits);
	}
	printk_spew("call enable_var_mtrr()\n");
	enable_var_mtrr();
	printk_spew("Leave %s\n", __FUNCTION__);
	post_code(0x6A);
}

void x86_setup_mtrrs(unsigned address_bits)
{
	x86_setup_fixed_mtrrs();
	x86_setup_var_mtrrs(address_bits);
}


int x86_mtrr_check(void)
{
	/* Only Pentium Pro and later have MTRR */
	msr_t msr;
	printk_debug("\nMTRR check\n");

	msr = rdmsr(0x2ff);
	msr.lo >>= 10;

	printk_debug("Fixed MTRRs   : ");
	if (msr.lo & 0x01)
		printk_debug("Enabled\n");
	else
		printk_debug("Disabled\n");

	printk_debug("Variable MTRRs: ");
	if (msr.lo & 0x02)
		printk_debug("Enabled\n");
	else
		printk_debug("Disabled\n");

	printk_debug("\n");

	post_code(0x93);
	return ((int) msr.lo);
}
