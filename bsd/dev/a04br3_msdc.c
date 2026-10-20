/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MT8167 inherited MSDC0 read-only bring-up.
 * Adapted from mouseos EDK2 Mt8167Pkg/Drivers/EmmcBlockIoDxe,
 * and Platform/Benesse/A04BR3Pkg/Drivers/SdCardBlockIoDxe.
 * No flash programming, clock/timing changes, or persistent card switches.
 */
#if defined(ARM64_BOARD_CONFIG_A04BR3)
#include <sys/systm.h>
#include <sys/errno.h>
#include <kern/clock.h>
#include <mach/mach_time.h>
#include <pexpert/pexpert.h>
#include <libkern/libkern.h>
extern vm_offset_t ml_io_map(vm_offset_t phys_addr, vm_size_t size);

#define CFG 0x00
#define INTR 0x0c
#define INTEN 0x10
#define FIFO 0x14
#define RX 0x1c
#define CMD 0x34
#define ARG 0x38
#define STS 0x3c
#define RESP 0x40
#define BLOCKS 0x50
#define CMD_DONE (1u<<8)
#define CMD_ERR ((1u<<9)|(1u<<10))
#define DATA_DONE (1u<<12)
#define DATA_ERR ((1u<<14)|(1u<<15))
static vm_offset_t msdc;
static uint32_t rd(unsigned r) { return *(volatile uint32_t *)(msdc+r); }
static void wr(unsigned r, uint32_t v) {
    *(volatile uint32_t *)(msdc+r)=v;
    __asm__ volatile("dsb sy" ::: "memory");
}
static uint64_t deadline(void) {
    uint64_t ticks; nanoseconds_to_absolutetime(1000000000ULL, &ticks);
    return mach_absolute_time()+ticks;
}
static int clear_wait(unsigned reg, uint32_t mask) {
    uint64_t end=deadline();
    do { if (!(rd(reg)&mask)) return 0; } while (mach_absolute_time()<end);
    return ETIMEDOUT;
}
static int command(unsigned opcode, uint32_t argument, int data, uint32_t *response) {
    int e=clear_wait(STS, opcode==13 ? 2 : 1);
    if (e) return e;
    wr(INTR,rd(INTR)); wr(ARG,argument);
    wr(CMD,opcode|(1u<<7)|(data ? ((1u<<11)|(512u<<16)) : 0));
    uint64_t end=deadline();
    do {
        uint32_t status=rd(INTR)&(CMD_DONE|CMD_ERR);
        if (status) {
            wr(INTR,status);
            if (status&CMD_ERR) return EIO;
            *response=rd(RESP);
            /* R1 error bits, not the current-state/ready indicators. */
            if (*response&0xfdffa080u) return EIO;
            return 0;
        }
    } while (mach_absolute_time()<end);
    return ETIMEDOUT;
}
static int read_block(unsigned opcode, uint32_t argument, uint8_t *buffer) {
    wr(BLOCKS,1); wr(FIFO,rd(FIFO)|(1u<<31));
    int e=clear_wait(FIFO,1u<<31); uint32_t response;
    if (e) return e;
    e=command(opcode,argument,1,&response);
    if (e) return e;
    uint64_t end=deadline(); unsigned offset=0; int done=0;
    do {
        uint32_t ints=rd(INTR)&(DATA_DONE|DATA_ERR);
        if (ints) {
            wr(INTR,ints);
            if (ints&DATA_ERR) return EIO;
            if (ints&DATA_DONE) done=1;
        }
        unsigned available=rd(FIFO)&255u;
        while (available && offset<512) {
            if (available>=4 && 512-offset>=4) {
                uint32_t word=rd(RX);
                memcpy(buffer+offset,&word,4);offset+=4;available-=4;
            } else {
                buffer[offset++]=*(volatile uint8_t *)(msdc+RX);available--;
            }
        }
        if (done && offset==512) return 0;
    } while (mach_absolute_time()<end);
    return ETIMEDOUT;
}
static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static uint64_t le64(const uint8_t *p) { return le32(p)|((uint64_t)le32(p+4)<<32); }
static uint32_t header_crc(const uint8_t *p, unsigned size) {
    uint32_t crc=~0u;
    for (unsigned i=0;i<size;i++) {
        crc^=(i>=16 && i<20) ? 0 : p[i];
        for (unsigned j=0;j<8;j++) crc=(crc>>1)^(0xedb88320u & (0u-(crc&1u)));
    }
    return ~crc;
}
void a04br3_msdc_probe(void);
void a04br3_msdc_probe(void) {
    unsigned enabled=0;
    if (!PE_parse_boot_argn("a04br3.msdc_probe",&enabled,sizeof(enabled)) || !enabled) return;
    msdc=ml_io_map(0x11120000,0x1000);
    uint32_t cfg=rd(CFG),inten=rd(INTEN),response=0;
    printf("A04BR3 MSDC0: inherited CFG=0x%08x STS=0x%08x\n",cfg,rd(STS));
    /* No reset: retain LK's card clock, voltage, bus width and sampling. */
    wr(INTEN,0);wr(CFG,cfg|(1u<<3));
    int error=command(13,1u<<16,0,&response);
    if (!error && ((response>>9)&15u)!=4u) error=ENXIO;
    uint8_t sector[512];uint32_t sectors=0;
    if (!error) error=read_block(8,0,sector);
    if (!error) {
        sectors=le32(sector+212);
        printf("A04BR3 MSDC0: EXT_CSD rev=%u sectors=%u partition_access=%u\n",
            sector[192],sectors,sector[179]&7u);
        if (!sectors || (sector[179]&7u)) error=ENXIO;
    }
    uint32_t locations[2]={1,0x1c01};int found=0;
    for (unsigned i=0;!error && i<2;i++) {
        error=read_block(17,locations[i],sector);
        if (error) break;
        if (memcmp(sector,"EFI PART",8)) continue;
        unsigned size=le32(sector+12);
        if (size<92 || size>512 || le64(sector+24)!=locations[i] ||
            le32(sector+16)!=header_crc(sector,size)) { error=EINVAL;break; }
        printf("A04BR3 MSDC0: GPT header CRC verified at LBA=%u entries_LBA=%llu count=%u size=%u\n",
            locations[i],le64(sector+72),le32(sector+80),le32(sector+84));
        found=1;break;
    }
    if (!error && !found) error=ENOENT;
    wr(INTR,rd(INTR));wr(CFG,cfg);wr(INTEN,inten);
    printf("A04BR3 MSDC0: read-only probe %s error=%d R1=0x%08x\n",
        error ? "FAILED" : "PASS",error,response);
}
#endif
