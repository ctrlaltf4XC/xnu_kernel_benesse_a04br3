/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Original bounded implementation of MT8167 TOPRGU register contract.
 * Source: board DTS toprgu@10007000; Linux MediaTek wdt_v1/mtk_wdt.h and
 * wdt_arch_reset(mode=1); EDK Mt8167Pkg/Library/ResetSystemLib.c.
 * RESTART key1971, MODE key22000000, hardware-reset mode/exten/bypass power
 * key,100us settling, SWRST key1209. No RTC/bootloader/recovery/nonrst writes.
 * Caller selects ONLY kPERestartCPU; pre-mapped device MMIO and CPU/device
 * barriers required. Returning true means request issued, NOT reset proven.
 */
#ifndef A04BR3_RESTART_H
#define A04BR3_RESTART_H
#include <stdint.h>
struct a04_restart_io {
    void *context;
    uint32_t (*read)(void *,unsigned);
    void (*write)(void *,unsigned,uint32_t);
    void (*barrier)(void *);
    void (*delay_us)(void *,unsigned);
};
static inline bool a04_restart_request(const a04_restart_io *io)
{
    if(!io || !io->read || !io->write || !io->barrier || !io->delay_us)return false;
    io->barrier(io->context);
    io->write(io->context,8,0x1971U);io->barrier(io->context);
    uint32_t mode=io->read(io->context,0);
    /* Lower24 bits hold configuration; upper8 is the write key. Preserve
     * unrelated configuration/polarity, disable timer/IRQ/dual; bypass key
     * is normal restart (Linux default), not a recovery boot-mode flag. */
    mode=(mode&0x00ffffa6U)|0x22000014U;
    io->write(io->context,0,mode);io->barrier(io->context);
    if((io->read(io->context,0)&0x5dU)!=0x14U)return false;
    io->delay_us(io->context,100);io->barrier(io->context);
    io->write(io->context,0x14,0x1209U);io->barrier(io->context);
    return true;
}
#endif
