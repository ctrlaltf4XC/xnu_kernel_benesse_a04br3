/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 yuma shimizu
 * Redmi Note 8 Pro (begonia), MT6785 / Helio G90T.
 * 2x Cortex-A75 (big) + 6x Cortex-A55 (LITTLE), GICv3, ARMv8.2-A.
 */
#ifndef _PEXPERT_ARM64_BEGONIA_H
#define _PEXPERT_ARM64_BEGONIA_H

#define BEGONIA                     1
#define NO_MONITOR                  1
#define ARM_ARCH_TIMER              1
#define __ARM_ARCH__                8
#define __ARM_VMSA__                8
#define __ARM_VFP__                 4
#define __ARM_COHERENT_CACHE__      1
#define __ARM_DEBUG__               7
#define __ARM_16K_PG__              1
#define __ARM64_PMAP_SUBPAGE_L1__   1
#define __ARM_AMP__                 1
#define __ARM_52BIT_PA__            1
#define HAS_CLUSTER                 1 /* Has eCores and pCores in separate clusters */
#define ARM_LSE                     1
#define ARM_LSE_ATOMICS             1
#define ARM_FEAT_PAN                1
#define ARM_FEAT_UAO                1
#define ARM_FEAT_LOR                1
#define ARM_FEAT_IESB               1
#define ARM_FEAT_SSBS               1
#define ARM_FEAT_CSSC               1
#define ARM_FEAT_BTI                1
#define ARM_FEAT_MTE                0
#define ARM_FEAT_E0PD               1
#define ARM_FEAT_ECV                1
#define ARM_FEAT_FGT                1
#define ARM_FEAT_LRCPC              1
#define ARM_FEAT_FCMA               1
#define ARM_FEAT_JSCVT              1
#define ARM_FEAT_RCPC               1
#define ARM_FEAT_DPB                1
#define ARM_FEAT_LSE2               1
#define ARM_FEAT_PAC                0
#define ARM_FEAT_GICV3              1
#define ARM_FEAT_GICV4              0

#define BEGONIA_VTIMER_INTID        27
#define BEGONIA_IPI_SGI             16

#if defined(ARM_LARGE_MEMORY)
#error BEGONIA requires the normal ARM64 virtual address layout
#endif
#if defined(VM_KERNEL_LINK_ADDRESS) && (VM_KERNEL_LINK_ADDRESS < 0xffffff8000000000ULL)
#error BEGONIA kernel link address is outside the 39-bit TTBR1 range
#endif

#endif