/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 yuma shimizu
 * Benesse TAB-A04-BR3, MT8167 / Cortex-A35. Initial CPU target only.
 */
#ifndef _PEXPERT_ARM64_A04BR3_H
#define _PEXPERT_ARM64_A04BR3_H

#define A04BR3                    1
#define NO_MONITOR                1
#define NO_ECORE                  1
#define ARM_ARCH_TIMER            1
#define __ARM_ARCH__              8
#define __ARM_VMSA__              8
#define __ARM_VFP__               4
#define __ARM_COHERENT_CACHE__    1
#define __ARM_DEBUG__             7
#define __ARM_16K_PG__            1
#define __ARM64_PMAP_SUBPAGE_L1__ 1
/* This table shape exposes 39 VA bits. The MacOSX 2-TiB layout cannot fit. */
#if defined(ARM_LARGE_MEMORY)
#error A04BR3 requires the normal ARM64 virtual address layout
#endif
#if defined(VM_KERNEL_LINK_ADDRESS) && (VM_KERNEL_LINK_ADDRESS < 0xffffff8000000000ULL)
#error A04BR3 kernel link address is outside the 39-bit TTBR1 range
#endif
/* No PAN, LSE, PAC, GICv3, Apple SMP or Apple implementation registers. */
#ifndef ASSEMBLER
#define PLATFORM_PANIC_LOG_DISABLED
#endif
#endif
