// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright 2017-2019 NXP
 */

#include <kernel/panic.h>
#include <initcall.h>
#include <trace.h>
#include <mm/core_memprot.h>
#include <string.h>
#include <io.h>
#include <imx.h>
#include <imx_pm.h>
#ifdef CFG_DT
#include <libfdt.h>
#include <kernel/boot.h>
#include <kernel/dt.h>
#endif /* CFG_DT */

paddr_t iram_tlb_phys_addr = -1UL;

#if defined(CFG_MX7)
static paddr_t ocram_tz_start_addr = -1UL;

paddr_t imx_get_ocram_tz_start_addr(void)
{
	if (ocram_tz_start_addr != -1UL)
		return ocram_tz_start_addr;
	else
		return TRUSTZONE_OCRAM_START;
}

#ifdef CFG_DT
/*
 * List of nodes to include in the OCRAM TZ space.
 * The lowest node in the OCRAM will define the ocram_tz_start_addr.
 * The lowest node must have a 4k aligned address.
 * If this list is empty, imx_get_ocram_tz_start_addr() will return the
 * default TRUSTZONE_OCRAM_START.
 */
static const char * const tz_ocram_match[] = {
	"fsl,optee-lpm-sram",
	NULL,
};

/*
 * Find the lowest address among nodes listed in tz_ocram_match[]. The lowest
 * address will define the start address of the Trustzone protected ocram space.
 * If something goes wrong during dtb parsing, the system panics.
 * If tz_ocram_match[] is empty, we return -1UL. In that case,
 * imx_get_ocram_tz_start_addr() will return TRUSTZONE_OCRAM_START by default.
 */
static void dt_find_ocram_tz_addr(void)
{
	void *fdt;
	int offset;
	paddr_t start_addr = -1UL;
	paddr_t tmp_addr = 0;
	uint32_t idx = 0;

	/* Get device tree blob */
	fdt = get_dt();
	if (fdt == NULL)
		panic("No DTB found");

	while (tz_ocram_match[idx] != NULL) {
		/* Get node */
		offset = fdt_node_offset_by_compatible(fdt, 0,
							tz_ocram_match[idx]);
		if (offset < 0) {
			EMSG("Cannot find %s node in the device tree",
							tz_ocram_match[idx]);
			panic();
		}

		/* Get address in "reg" property */
		tmp_addr = _fdt_reg_base_address(fdt, offset);
		if (!tmp_addr) {
			EMSG("Cannot get reg property of %s",
							tz_ocram_match[idx]);
			panic();
		}

		/* Addresses must be 4Kbytes aligned to be TZ*/
		if (tmp_addr & 0xFFF) {
			EMSG("%s address is not 4Kbytes aligned",
							tz_ocram_match[idx]);
			panic();
		}

		/*
		 * Get the lowest address among nodes to protect listed in
		 * tz_ocram_match[].
		 */
		if ((tmp_addr < start_addr) || (start_addr == -1UL))
			start_addr = tmp_addr;

		idx++;
	}
	ocram_tz_start_addr = start_addr;
}
#endif /* CFG_DT */
#endif /* CFG_MX7 */

#ifdef CFG_MX7
static const paddr_t phys_addr_imx7[] = {
	AIPS1_BASE, AIPS2_BASE, AIPS3_BASE, 0
};
#endif

#if defined(CFG_MX7)
static void init_tz_ocram(void)
{
	/* Configure the Secure OCRAM granularity */
	vaddr_t  iomux_base;
	uint32_t val;
	uint32_t lock;
	uint32_t lock_val;

#ifdef CFG_DT
	/* Get low tz ocram address */
	dt_find_ocram_tz_addr();
#endif

	iomux_base = (vaddr_t)phys_to_virt(IOMUXC_GPR_BASE, MEM_AREA_IO_SEC,
					   IOMUXC_SIZE);

	val = io_read32(iomux_base + IOMUX_GPRx_OFFSET(IOMUX_GPR_OCRAM_ID));

	/* Configure the OCRAM Retention to start at offset 0 */
	val &= ~BM_IOMUX_GPR_OCRAM_S_TZ_ADDR;
	// Address is 4 Kbytes granularity
	val |= (((imx_get_ocram_tz_start_addr() >> 12) <<
			BP_IOMUX_GPR_OCRAM_S_TZ_ADDR) &
			BM_IOMUX_GPR_OCRAM_S_TZ_ADDR);
	val |= IOMUX_GPR_OCRAM_S_TZ_ENABLE;

	lock = BM_IOMUX_GPR_OCRAM_S_TZ_ADDR | IOMUX_GPR_OCRAM_S_TZ_ENABLE;

	/* Check if GPR registers for OCRAM TZ protection are locked */
	if (IOMUX_GPR_OCRAM_LOCK(lock) & val) {
		EMSG("GPR Registers for OCRAM TZ Configuration locked");
		panic();
	}

	io_write32(iomux_base + IOMUX_GPRx_OFFSET(IOMUX_GPR_OCRAM_ID), val);

	/* Then lock configuration */
	io_write32(iomux_base + IOMUX_GPRx_OFFSET(IOMUX_GPR_OCRAM_ID),
				IOMUX_GPR_OCRAM_LOCK(lock) | val);

	/*
	 * Ensure that GPR registers for OCRAM TZ protection locked
	 * match with the current configuration.
	 */
	lock_val =
		io_read32(iomux_base + IOMUX_GPRx_OFFSET(IOMUX_GPR_OCRAM_ID));
	if ((lock_val & lock) != (val & lock))
		panic("OCRAM TZ Configuration Lock Mismatch");
}

static TEE_Result init_ocram(void)
{
	struct tee_mmap_region map;
	const paddr_t *phys_addr;
	size_t size_area;
	void *iram_tlb_vaddr;

	DMSG("IRAM TLB phys addr = 0x%X", (uint32_t)iram_tlb_phys_addr);

	/* iram tlb already initialized */
	if (iram_tlb_phys_addr != (-1UL))
		return TEE_SUCCESS;

	/* Initialize the Secure OCRAM */
	init_tz_ocram();

#ifdef CFG_DT
	/* Move ocram nodes to the OCRAM for Linux */
	dt_overwrite("fsl,optee-lpm-sram", "reg", "overw_reg", 3);
	dt_overwrite("fsl,optee-lpm-sram", "clocks", "overw_clock", 3);
#endif

#ifdef CFG_MX7
	iram_tlb_phys_addr = imx_get_ocram_tz_start_addr() + IRAM_TBL_OFFSET;
	phys_addr = phys_addr_imx7;
	size_area = AIPS1_SIZE; /* 4M for AIPS1/2/3 */
#endif

	iram_tlb_vaddr =
		phys_to_virt(iram_tlb_phys_addr, MEM_AREA_TEE_COHERENT,
		16 * 1024);

	/* 16KB */
	DMSG("%x %x\n", (uint32_t)iram_tlb_phys_addr, (uint32_t)iram_tlb_vaddr);
	memset(iram_tlb_vaddr, 0, 16 * 1024);

	do {
		map.pa = *phys_addr;
		map.va = (vaddr_t)phys_to_virt(map.pa, MEM_AREA_IO_SEC,
					       CORE_MMU_PGDIR_SIZE);
		map.region_size = CORE_MMU_PGDIR_SIZE;
		map.size = size_area;
		map.type = MEM_AREA_IO_SEC;
		map.attr = TEE_MATTR_VALID_BLOCK | TEE_MATTR_PRW |
				TEE_MATTR_GLOBAL | TEE_MATTR_SECURE |
				(TEE_MATTR_MEM_TYPE_DEV <<
					TEE_MATTR_MEM_TYPE_SHIFT);
		map_memarea_sections(&map, (uint32_t *)iram_tlb_vaddr);
	} while (*(++phys_addr));

#ifdef CFG_MX7
	/* Note IRAM_S_BASE is not 1M aligned, so take care */
	map.pa = ROUNDDOWN(IRAM_S_BASE, CORE_MMU_PGDIR_SIZE);
	map.va = (vaddr_t)phys_to_virt(map.pa, MEM_AREA_TEE_COHERENT,
				       CORE_MMU_PGDIR_SIZE);
	map.region_size = CORE_MMU_PGDIR_SIZE;
	map.size = CORE_MMU_PGDIR_SIZE;
	map.type = MEM_AREA_TEE_COHERENT;
	map.attr = TEE_MATTR_VALID_BLOCK | TEE_MATTR_PRW | TEE_MATTR_GLOBAL |
				TEE_MATTR_SECURE | TEE_MATTR_PX;
	map_memarea_sections(&map, (uint32_t *)iram_tlb_vaddr);

	map.pa = GIC_BASE;
	map.va = (vaddr_t)phys_to_virt((paddr_t)GIC_BASE, MEM_AREA_IO_SEC,
				       GIC_SIZE);
	map.region_size = CORE_MMU_PGDIR_SIZE;
	map.size = CORE_MMU_PGDIR_SIZE;
	map.type = MEM_AREA_TEE_COHERENT;
	map.attr = TEE_MATTR_VALID_BLOCK | TEE_MATTR_PRW | TEE_MATTR_GLOBAL |
				TEE_MATTR_SECURE | TEE_MATTR_PX;
	map_memarea_sections(&map, (uint32_t *)iram_tlb_vaddr);

	/*
	 * Note: DRAM space is not mapped, DRAM is in auto-selfrefresh,
	 * If map DRAM in to MMU, mmu will access DRAM which
	 * hang system.
	 */
#endif

	return TEE_SUCCESS;
}
#else
static TEE_Result init_ocram(void)
{
	return TEE_SUCCESS;
}
#endif

service_init(init_ocram);
