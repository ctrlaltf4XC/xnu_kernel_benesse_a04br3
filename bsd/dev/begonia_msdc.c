/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MT8167 inherited MSDC0 bounded read/write bring-up.
 * Adapted from mouseos EDK2 Mt8167Pkg/Drivers/EmmcBlockIoDxe,
 * and Platform/Benesse/BEGONIAPkg/Drivers/SdCardBlockIoDxe.
 * Filesystem writes are confined to GPT userdata; a separate root-only boot
 * staging cdev supports verified self-update. No clock/timing changes.
 */
#if defined(ARM64_BOARD_CONFIG_BEGONIA)
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/uio.h>
#include <sys/proc.h>
#include <sys/ioccom.h>
#include <libkern/crypto/sha2.h>
#include "begonia_boot_update.h"
#include <kern/locks.h>
#include <kern/kalloc.h>
#include <miscfs/devfs/devfs.h>
#include <kern/clock.h>
#include <mach/mach_time.h>
#include <pexpert/pexpert.h>
#include <libkern/libkern.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>
#include <arm/machine_routines.h>
#include "begonia_msdc_dma.h"
#include "begonia_wlan_nvram.h"
extern vm_offset_t ml_io_map(vm_offset_t phys_addr, vm_size_t size);
/* Same BSD-side declaration as memdev.c. */
extern ppnum_t pmap_find_phys(pmap_t map, addr64_t va);

#define CFG 0x00
#define INTR 0x0c
#define INTEN 0x10
#define FIFO 0x14
#define TX 0x18
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
    int e=clear_wait(STS, opcode==13 || opcode==12 ? 2 : 1);
    if (e) return e;
    wr(INTR,rd(INTR)); wr(ARG,argument);
    wr(CMD,opcode|((opcode==6 || opcode==12 ? 7u : 1u)<<7)|
        (opcode==12 ? (1u<<14) : 0)|
        (data ? ((data>=3 ? 2u : 1u)<<11)|(512u<<16)|(data==2 || data==4 ? (1u<<13) : 0) : 0));
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
/* Optional descriptor read DMA; PIO writes and inherited timing stay intact.
 * Controller ownership is serialized by msdc_lock after probe registration. */
static vm_offset_t dma_area;
static uint64_t dma_pa;
static int dma_enabled;
static void dma_cache_sync(vm_offset_t address,unsigned bytes)
{
    unsigned line=4u<<((__builtin_arm_rsr64("CTR_EL0")>>16)&15u);
    vm_offset_t end=address+bytes;
    for(vm_offset_t p=address&~((vm_offset_t)line-1);p<end;p+=line)
        __asm__ volatile("dc civac, %0"::"r"(p):"memory");
    __asm__ volatile("dsb sy":::"memory");
}
static int dma_read(uint32_t argument,uint8_t *buffer,unsigned blocks)
{
    if(!dma_area || !blocks || blocks>128)return EINVAL;
    uint32_t response=0;unsigned bytes=blocks*512;int e=0;
    struct begonia_dma_gpd *gpd=(void *)dma_area;
    struct begonia_dma_bd *bd=(void *)(dma_area+64);
    vm_offset_t bounce=dma_area+PAGE_SIZE;
    if(begonia_dma_descriptors(gpd,bd,dma_pa,dma_pa+64,dma_pa+PAGE_SIZE,bytes))return EINVAL;
    dma_cache_sync(dma_area,PAGE_SIZE);dma_cache_sync(bounce,bytes);
    if(blocks>1 && (e=command(23,blocks,0,&response)))return e;
    wr(BLOCKS,blocks);wr(FIFO,rd(FIFO)|(1u<<31));
    if((e=clear_wait(FIFO,1u<<31)))return e;
    wr(CFG,rd(CFG)&~(1u<<3));
    e=command(blocks>1?18:17,argument,blocks>1?3:1,&response);
    if(e)goto stop;
    wr(0x8c,0);wr(0x90,(uint32_t)dma_pa);wr(0xa8,0);
    wr(0x9c,rd(0x9c)|2u);
    wr(0x98,(rd(0x98)&~((1u<<0)|(1u<<1)|(1u<<10)|(7u<<12)))|(1u<<8)|(6u<<12));
    wr(0x98,rd(0x98)|1u);
    uint64_t end=deadline();
    for(;;){
        uint32_t ints=rd(INTR)&(DATA_DONE|DATA_ERR|(7u<<17));
        if(ints){wr(INTR,ints);if(ints&(DATA_ERR|(7u<<17))){e=EIO;break;}
            if(ints&DATA_DONE)break;}
        if(mach_absolute_time()>=end){e=ETIMEDOUT;break;}
    }
stop:
    /* Never recycle a buffer while hardware may still write to it. */
    wr(0x98,rd(0x98)|2u);
    if(clear_wait(0x9c,1u))panic("BEGONIA MSDC DMA: engine failed to stop");
    wr(0x9c,rd(0x9c)&~2u);wr(CFG,rd(CFG)|(1u<<3));
    if(e){if(blocks>1)(void)command(12,0,0,&response);return e;}
    e=clear_wait(STS,1u);if(e)return e;
    dma_cache_sync(bounce,bytes);memcpy(buffer,(void *)bounce,bytes);
    return 0;
}
/* CMD23 limits CMD18 to the requested block count: no AutoCMD timing
 * dependency and no open-ended stream. MT8167 SDC_CMD dtype=2 is multi.
 * Maximum 64KiB matches DKIOCGETMAXBLOCKCOUNTREAD. PIO remains inherited.
 */
static int read_blocks(unsigned opcode, uint32_t argument, uint8_t *buffer,
    unsigned blocks) {
    if (!blocks || blocks>128) return EINVAL;
    if (dma_enabled && (opcode==17 || opcode==18)) return dma_read(argument,buffer,blocks);
    uint32_t response;
    int e;
    if (blocks>1) {
        e=command(23,blocks,0,&response);
        if (e) return e;
        opcode=18;
    }
    wr(BLOCKS,blocks); wr(FIFO,rd(FIFO)|(1u<<31));
    e=clear_wait(FIFO,1u<<31);
    if (e) return e;
    e=command(opcode,argument,blocks>1 ? 3 : 1,&response);
    if (e) goto recover;
    uint64_t end=deadline(); unsigned offset=0, total=blocks*512; int done=0;
    do {
        uint32_t ints=rd(INTR)&(DATA_DONE|DATA_ERR);
        if (ints) {
            wr(INTR,ints);
            if (ints&DATA_ERR) { e=EIO; goto recover; }
            if (ints&DATA_DONE) done=1;
        }
        unsigned available=rd(FIFO)&255u;
        while (available && offset<total) {
            if (available>=4 && total-offset>=4) {
                uint32_t word=rd(RX);
                memcpy(buffer+offset,&word,4);offset+=4;available-=4;
            } else {
                buffer[offset++]=*(volatile uint8_t *)(msdc+RX);available--;
            }
        }
        if (done && offset==total) return 0;
    } while (mach_absolute_time()<end);
    e=ETIMEDOUT;
recover:
    /* Preserve original failure; stop an interrupted multi-block transfer. */
    if (blocks>1) (void)command(12,0,0,&response);
    return e;
}
static int read_block(unsigned opcode, uint32_t argument, uint8_t *buffer) {
    return read_blocks(opcode,argument,buffer,1);
}
/* CMD23/CMD25 bounded multi-block writes. Single-sector callers retain
 * CMD24. Fill each FIFO burst without a full-system barrier per word. */
static int write_blocks_raw(uint32_t block,const uint8_t *buffer,unsigned blocks) {
    if (!blocks || blocks>128) return EINVAL;
    uint32_t response;
    int error;
    if (blocks>1) {
        error=command(23,blocks,0,&response);
        if (error) return error;
    }
    wr(BLOCKS,blocks);wr(FIFO,rd(FIFO)|(1u<<31));
    error=clear_wait(FIFO,1u<<31);
    if (error) return error;
    error=command(blocks>1 ? 25 : 24,block,blocks>1 ? 4 : 2,&response);
    if (error) goto recover;
    uint64_t end=deadline();unsigned offset=0,total=blocks*512;int done=0;
    do {
        uint32_t ints=rd(INTR)&(DATA_DONE|DATA_ERR);
        if (ints) {
            wr(INTR,ints);
            if (ints&DATA_ERR) { error=EIO;goto recover; }
            if (ints&DATA_DONE) done=1;
        }
        unsigned used=(rd(FIFO)>>16)&255u;
        unsigned available=used<=128 ? 128-used : 0;
        while (available>=4 && offset<total) {
            uint32_t word;memcpy(&word,buffer+offset,4);
            *(volatile uint32_t *)(msdc+TX)=word;
            offset+=4;available-=4;
        }
        __asm__ volatile("dsb sy" ::: "memory");
        if (done && offset==total) {
            do {
                error=command(13,1u<<16,0,&response);
                if (error) return error;
                if ((response&(1u<<8)) && ((response>>9)&15u)==4) return 0;
            } while (mach_absolute_time()<end);
            error=ETIMEDOUT;goto recover;
        }
    } while (mach_absolute_time()<end);
    error=ETIMEDOUT;
recover:
    if (blocks>1) (void)command(12,0,0,&response);
    return error;
}
static int write_block_raw(uint32_t block,const uint8_t buffer[512]) {
    return write_blocks_raw(block,buffer,1);
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
static uint32_t disk_sectors;
static uint32_t root_first,root_sectors;
static uint32_t nvram_first,nvram_sectors;
static uint32_t boot_first,boot_sectors,boot_gpt_lba;
static unsigned boot_update_enabled;
static dev_t root_device;
static unsigned write_enabled,cache_enabled;
/* Never expose full-disk writes, including GPT, LK or boot partitions. */
static int flush_card_force(void);
static int write_root_block(uint64_t block,const uint8_t sector[512]) {
    if (!write_enabled) return EROFS;
    if (block>=root_sectors || root_first>=disk_sectors ||
        block>=(uint64_t)disk_sectors-root_first) return EINVAL;
    return write_block_raw(root_first+(uint32_t)block,sector);
}
static int write_root_blocks(uint64_t block,const uint8_t *data,unsigned blocks) {
    if (!write_enabled) return EROFS;
    if (!blocks || blocks>128 || block>=root_sectors || root_first>=disk_sectors ||
        block>=(uint64_t)disk_sectors-root_first ||
        blocks>(uint64_t)root_sectors-block ||
        blocks>(uint64_t)disk_sectors-root_first-block) return EINVAL;
    return write_blocks_raw(root_first+(uint32_t)block,data,blocks);
}
static int flush_card(void) {
    return write_enabled ? flush_card_force() : 0;
}
static int flush_card_force(void) {
    uint32_t response;
    if (!cache_enabled) return 0;
    int error=command(6,(3u<<24)|(32u<<16)|(1u<<8),0,&response);
    if (error) return error;
    uint64_t end=deadline();
    do {
        error=command(13,1u<<16,0,&response);
        if (error) return error;
        if ((response&(1u<<8)) && ((response>>9)&15u)==4) return 0;
    } while (mach_absolute_time()<end);
    return ETIMEDOUT;
}
static uint64_t read_bytes,read_ticks,read_requests;
static uint64_t write_bytes,write_ticks,write_requests;
static LCK_GRP_DECLARE(msdc_group,"begonia_msdc");
static LCK_MTX_DECLARE(msdc_lock,&msdc_group);
/* Separate, exclusive root-only staging endpoint. Existing full disk/raw
 * write policy is unchanged. boot_lock -> msdc_lock is the only lock order. */
static LCK_MTX_DECLARE(boot_lock,&msdc_group);
static vm_offset_t boot_stage,boot_scratch;
static uint64_t boot_image_bytes,boot_staged;
static uint8_t boot_digest[32];
static uint32_t boot_flags;
static int boot_opened;
/* The polled console may burst faster only while a root-owned image is staged.
 * No UART registers, divisor or hardware RX ownership changes here. */
static unsigned boot_rx_fast;
unsigned begonia_boot_rx_interval_ms(void);
unsigned begonia_boot_rx_interval_ms(void) {
    return __atomic_load_n(&boot_rx_fast,__ATOMIC_RELAXED) ? 1u : 16u;
}

static void boot_discard(void) {
    __atomic_store_n(&boot_rx_fast,0,__ATOMIC_RELAXED);
    if(boot_stage)kmem_free(kernel_map,boot_stage,(vm_size_t)boot_image_bytes);
    if(boot_scratch)kmem_free(kernel_map,boot_scratch,65536);
    boot_stage=boot_scratch=0;boot_image_bytes=boot_staged=0;boot_flags=0;
    bzero(boot_digest,sizeof(boot_digest));
}
static int boot_open(__unused dev_t dev,__unused int flags,__unused int type,struct proc *p) {
    if(proc_suser(p))return EPERM;
    lck_mtx_lock(&boot_lock);
    int error=(!boot_sectors || !boot_update_enabled) ? ENXIO : boot_opened ? EBUSY : 0;
    if(!error)boot_opened=1;
    lck_mtx_unlock(&boot_lock);return error;
}
static int boot_close(__unused dev_t dev,__unused int flags,__unused int type,__unused struct proc *p) {
    lck_mtx_lock(&boot_lock);boot_discard();boot_opened=0;
    lck_mtx_unlock(&boot_lock);return 0;
}
/* Freshly re-read both CRCs before any flash write. The discovered partition
 * must still be the exact same unique boot range used by INFO/BEGIN. */
static int boot_parse_gpt(const uint8_t header[512],uint32_t sectors,uint32_t *first,uint32_t *count) {
    static uint8_t entries[128*128];
    uint32_t n=le32(header+80),size=le32(header+84);
    uint64_t lba=le64(header+72);
    if(n==0 || n>128 || size!=128 || lba>=sectors ||
        (n*size+511)/512>sectors-lba)return EINVAL;
    for(unsigned at=0;at<n*size;at+=512) {
        int error=read_block(17,(uint32_t)lba+at/512,entries+at);if(error)return error;
    }
    return begonia_boot_gpt(header,entries,sectors,first,count);
}
static int boot_revalidate_gpt(void) {
    uint8_t header[512];uint32_t first=0,count=0;
    int error=read_block(17,boot_gpt_lba,header);if(error)return error;
    if(le64(header+24)!=boot_gpt_lba)return EINVAL;
    error=boot_parse_gpt(header,disk_sectors,&first,&count);
    return error ? error : first!=boot_first || count!=boot_sectors ? EINVAL : 0;
}
/* The caller has boot_lock. Controller ownership is bounded to one <=64KiB
 * transfer so root filesystem progress is not blocked by the entire update. */
static int boot_transfer(uint64_t offset,void *buffer,unsigned bytes,int writing) {
    if(!bytes || bytes>65536 || (bytes&511u) || (offset&511u) ||
        offset>(uint64_t)boot_sectors*512 || bytes>(uint64_t)boot_sectors*512-offset ||
        boot_first>=disk_sectors || boot_sectors>disk_sectors-boot_first)return EINVAL;
    lck_mtx_lock(&msdc_lock);
    uint32_t cfg=rd(CFG),inten=rd(INTEN);
    wr(INTEN,0);wr(CFG,cfg|(1u<<3));
    int error=writing ? write_blocks_raw(boot_first+(uint32_t)(offset/512),buffer,bytes/512) :
        read_blocks(17,boot_first+(uint32_t)(offset/512),buffer,bytes/512);
    wr(INTR,rd(INTR));wr(CFG,cfg);wr(INTEN,inten);
    lck_mtx_unlock(&msdc_lock);return error;
}
static int boot_flush(int validate) {
    lck_mtx_lock(&msdc_lock);
    uint32_t cfg=rd(CFG),inten=rd(INTEN);
    wr(INTEN,0);wr(CFG,cfg|(1u<<3));
    int error=validate ? boot_revalidate_gpt() : flush_card_force();
    wr(INTR,rd(INTR));wr(CFG,cfg);wr(INTEN,inten);
    lck_mtx_unlock(&msdc_lock);return error;
}
static int boot_commit(void) {
    if(!boot_stage || boot_flags!=BEGONIA_BOOT_STAGING || boot_staged!=boot_image_bytes)return EINVAL;
    __atomic_store_n(&boot_rx_fast,0,__ATOMIC_RELAXED);
    uint8_t digest[32];SHA256_CTX hash;
    SHA256_Init(&hash);SHA256_Update(&hash,(void *)boot_stage,(size_t)boot_image_bytes);
    SHA256_Final(digest,&hash);
    int error=memcmp(digest,boot_digest,32) ? EINVAL :
        begonia_boot_image((void *)boot_stage,boot_image_bytes);
    if(!error)error=boot_flush(1);
    /* No write occurs before ALL of the above checks succeed. */
    for(uint64_t at=0;!error && at<boot_image_bytes;) {
        unsigned bytes=(unsigned)MIN(65536ULL,boot_image_bytes-at);
        error=boot_transfer(at,(void *)(boot_stage+at),bytes,1);at+=bytes;
    }
    if(!error)error=boot_flush(0);
    SHA256_Init(&hash);
    for(uint64_t at=0;!error && at<boot_image_bytes;) {
        unsigned bytes=(unsigned)MIN(65536ULL,boot_image_bytes-at);
        error=boot_transfer(at,(void *)boot_scratch,bytes,0);
        if(!error)SHA256_Update(&hash,(void *)boot_scratch,bytes);
        at+=bytes;
    }
    SHA256_Final(digest,&hash);
    if(!error && memcmp(digest,boot_digest,32))error=EIO;
    if(!error)error=boot_flush(0);
    boot_flags=error ? BEGONIA_BOOT_FAILED : BEGONIA_BOOT_VERIFIED;
    printf("BEGONIA BOOT UPDATE: commit bytes=%llu result=%d verified=%u; no automatic reboot\n",
        (unsigned long long)boot_image_bytes,error,error==0);
    return error;
}
/* Read the CURRENT boot partition for verified patch staging (any offset).
 * Root-only, only while a stage exists; uses the stage scratch buffer and the
 * same bounded <=64KiB controller transfers. Never touches staged bytes. */
static int boot_read(__unused dev_t dev,struct uio *uio,__unused int flags) {
    if(proc_suser(current_proc()))return EPERM;
    lck_mtx_lock(&boot_lock);
    uint64_t part=(uint64_t)boot_sectors*512;int error=0;
    if(!boot_opened || !boot_stage || !boot_scratch || boot_flags!=BEGONIA_BOOT_STAGING || uio_offset(uio)<0)error=EINVAL;
    while(!error && uio_resid(uio)>0 && (uint64_t)uio_offset(uio)<part) {
        uint64_t off=(uint64_t)uio_offset(uio),base=off&~511ULL,within=off-base;
        uint64_t want=within+(uint64_t)uio_resid(uio);
        uint64_t bytes=MIN(65536ULL,(want+511)&~511ULL);
        bytes=MIN(bytes,part-base);
        error=boot_transfer(base,(void *)boot_scratch,(unsigned)bytes,0);
        if(error)break;
        uint64_t n=MIN(bytes-within,(uint64_t)uio_resid(uio));
        error=uiomove((caddr_t)(boot_scratch+within),(int)n,uio);
    }
    lck_mtx_unlock(&boot_lock);return error;
}
static int boot_write(__unused dev_t dev,struct uio *uio,__unused int flags) {
    if(proc_suser(current_proc()))return EPERM;
    lck_mtx_lock(&boot_lock);
    int error=0;
    if(!boot_opened || !boot_stage || boot_flags!=BEGONIA_BOOT_STAGING ||
        uio_offset(uio)<0 || (uint64_t)uio_offset(uio)!=boot_staged ||
        uio_resid(uio)<0 || (uint64_t)uio_resid(uio)>boot_image_bytes-boot_staged)error=EINVAL;
    while(!error && uio_resid(uio)>0) {
        int bytes=(int)MIN((uint64_t)uio_resid(uio),65536ULL);
        user_ssize_t before=uio_resid(uio);
        error=uiomove((caddr_t)(boot_stage+boot_staged),bytes,uio);
        boot_staged+=(uint64_t)(before-uio_resid(uio));
    }
    /* Even a partially faulted copy is never eligible for COMMIT. */
    if(error && boot_stage){boot_flags=BEGONIA_BOOT_FAILED;__atomic_store_n(&boot_rx_fast,0,__ATOMIC_RELAXED);}
    lck_mtx_unlock(&boot_lock);return error;
}
static int boot_ioctl(__unused dev_t dev,u_long cmd,caddr_t data,int flag,struct proc *p) {
    if(proc_suser(p))return EPERM;
    lck_mtx_lock(&boot_lock);int error=0;
    if(!boot_opened){error=ENXIO;goto out;}
    switch(cmd) {
    case BEGONIA_BOOT_INFO: {
        struct begonia_boot_info *info=(void *)data;
        *info=(struct begonia_boot_info){BEGONIA_BOOT_VERSION,boot_flags,
            (uint64_t)boot_sectors*512,boot_staged,boot_image_bytes};break;
    }
    case BEGONIA_BOOT_BEGIN: {
        const struct begonia_boot_begin *request=(void *)data;
        if(!(flag&FWRITE)){error=EBADF;break;}
        if(boot_stage){error=EBUSY;break;}
        if(request->reserved || begonia_boot_length(request->image_bytes,(uint64_t)boot_sectors*512)){
            error=EINVAL;break;
        }
        vm_offset_t stage=0,scratch=0;
        if(kmem_alloc(kernel_map,&stage,(vm_size_t)request->image_bytes,KMA_ZERO,VM_KERN_MEMORY_BSD)!=KERN_SUCCESS){
            error=ENOMEM;break;
        }
        if(kmem_alloc(kernel_map,&scratch,65536,KMA_ZERO,VM_KERN_MEMORY_BSD)!=KERN_SUCCESS){
            kmem_free(kernel_map,stage,(vm_size_t)request->image_bytes);error=ENOMEM;break;
        }
        boot_stage=stage;boot_scratch=scratch;boot_image_bytes=request->image_bytes;
        boot_staged=0;memcpy(boot_digest,request->sha256,32);boot_flags=BEGONIA_BOOT_STAGING;
        __atomic_store_n(&boot_rx_fast,1,__ATOMIC_RELAXED);break;
    }
    case BEGONIA_BOOT_COMMIT:
        error=(flag&FWRITE) ? boot_commit() : EBADF;break;
    case BEGONIA_BOOT_COPY: {
        const struct begonia_boot_copy *copy=(void *)data;
        uint64_t part=(uint64_t)boot_sectors*512;
        if(!(flag&FWRITE)){error=EBADF;break;}
        if(!boot_stage || boot_flags!=BEGONIA_BOOT_STAGING){error=EINVAL;break;}
        if(!copy->bytes || copy->bytes>65536 || (copy->bytes&511u) || (copy->source&511u) ||
            copy->source>part || copy->bytes>part-copy->source ||
            copy->bytes>boot_image_bytes-boot_staged){error=EINVAL;break;}
        error=boot_transfer(copy->source,(void *)boot_scratch,(unsigned)copy->bytes,0);
        if(error){boot_flags=BEGONIA_BOOT_FAILED;__atomic_store_n(&boot_rx_fast,0,__ATOMIC_RELAXED);break;}
        memcpy((void *)(boot_stage+boot_staged),(void *)boot_scratch,(size_t)copy->bytes);
        boot_staged+=copy->bytes;break;
    }
    case BEGONIA_BOOT_ABORT:
        if(!(flag&FWRITE))error=EBADF;else boot_discard();break;
    default:error=ENOTTY;break;
    }
out:
    lck_mtx_unlock(&boot_lock);return error;
}
static const struct cdevsw boot_cdevsw = {
    .d_open=boot_open,.d_close=boot_close,.d_read=boot_read,.d_write=boot_write,
    .d_ioctl=boot_ioctl,.d_stop=eno_stop,.d_reset=eno_reset,.d_select=eno_select,
    .d_mmap=eno_mmap,.d_strategy=eno_strat,.d_reserved_1=eno_getc,.d_reserved_2=eno_putc
};
static int disk_open(__unused dev_t dev,int flags,__unused int type,__unused struct proc *p) {
    if (!disk_sectors || minor(dev)>1 || (minor(dev)==1 && !root_sectors)) return ENXIO;
    return (flags&FWRITE) && (!write_enabled || minor(dev)!=1) ? EROFS : 0;
}
static int disk_close(__unused dev_t dev,__unused int flags,__unused int type,__unused struct proc *p) { return 0; }
static void disk_strategy(buf_t bp) {
    uint32_t count=buf_count(bp), completed=0;
    int error=0;caddr_t data=NULL;
    uint64_t block=buf_blkno(bp);
    uint32_t sectors=minor(buf_device(bp)) ? root_sectors : disk_sectors;
    uint32_t first=minor(buf_device(bp)) ? root_first : 0;
    buf_setresid(bp,count);
    /* Raw user I/O uses disk_read/uiomove, never a direct user pointer. */
    if (buf_flags(bp)&B_PHYS) { error=EFAULT;goto done; }
    int reading=(buf_flags(bp)&B_READ)!=0;
    if (!reading && (!write_enabled || minor(buf_device(bp))!=1)) { error=EROFS;goto done; }
    if ((count&511u) || block>sectors || (uint64_t)(count/512)>sectors-block) { error=EINVAL;goto done; }
    if (!count) goto done;
    if (buf_map(bp,&data)) { error=EIO;goto done; }
    lck_mtx_lock(&msdc_lock);
    uint32_t cfg=rd(CFG),inten=rd(INTEN);uint8_t sector[512];
    wr(INTEN,0);wr(CFG,cfg|(1u<<3));
    while (completed<count) {
        if (reading) {
            unsigned blocks=MIN((count-completed)/512,128u);
            uint64_t started=mach_absolute_time();
            error=read_blocks(17,first+(uint32_t)block+completed/512,
                (uint8_t *)data+completed,blocks);
            read_ticks+=mach_absolute_time()-started;
            read_requests++;
            if (!error) {
                uint64_t previous=read_bytes;
                read_bytes+=blocks*512;
                if (!previous || (previous>>24)!=(read_bytes>>24)) {
                    uint64_t ns;
                    absolutetime_to_nanoseconds(read_ticks,&ns);
                    printf("BEGONIA MSDC: read=%llu KiB service=%llu ms requests=%llu batch=%u blocks\n",
                        read_bytes/1024,ns/1000000,read_requests,blocks);
                }
                completed+=blocks*512; continue;
            }
        } else {
            unsigned blocks=MIN((count-completed)/512,128u);
            uint64_t started=mach_absolute_time();
            if (blocks==1) {
                memcpy(sector,data+completed,512);
                error=write_root_block(block+completed/512,sector);
            } else {
                error=write_root_blocks(block+completed/512,(uint8_t *)data+completed,blocks);
            }
            write_ticks+=mach_absolute_time()-started;write_requests++;
            if (!error) {
                uint64_t previous=write_bytes;
                write_bytes+=blocks*512;
                if (!previous || (previous>>24)!=(write_bytes>>24)) {
                    uint64_t ns;absolutetime_to_nanoseconds(write_ticks,&ns);
                    printf("BEGONIA MSDC: write=%llu KiB service=%llu ms requests=%llu batch=%u blocks\n",
                        write_bytes/1024,ns/1000000,write_requests,blocks);
                }
                completed+=blocks*512;continue;
            }
        }
        if (error) break;
        completed+=512;
    }
    wr(INTR,rd(INTR));wr(CFG,cfg);wr(INTEN,inten);
    lck_mtx_unlock(&msdc_lock);
    buf_unmap(bp);buf_setresid(bp,count-completed);
done:
    if (error) buf_seterror(bp,error);
    buf_biodone(bp);
}
static int disk_ioctl(dev_t dev,u_long cmd,caddr_t data,__unused int flag,__unused struct proc *p) {
    switch (cmd) {
    case DKIOCGETBLOCKSIZE:
    case DKIOCGETPHYSICALBLOCKSIZE: *(uint32_t *)data=512;return 0;
    case DKIOCSETBLOCKSIZE: return *(uint32_t *)data==512 ? 0 : EINVAL;
    case DKIOCGETBLOCKCOUNT: *(uint64_t *)data=minor(dev) ? root_sectors : disk_sectors;return 0;
    case DKIOCISWRITABLE: *(uint32_t *)data=write_enabled && minor(dev)==1;return 0;
    case DKIOCGETMAXBLOCKCOUNTREAD: *(uint64_t *)data=128;return 0;
    case DKIOCGETMAXBLOCKCOUNTWRITE: *(uint64_t *)data=write_enabled && minor(dev)==1 ? 128 : 0;return 0;
    case DKIOCSYNCHRONIZE:
    case DKIOCSYNCHRONIZECACHE: {
        lck_mtx_lock(&msdc_lock);
        uint32_t cfg=rd(CFG),inten=rd(INTEN);
        wr(INTEN,0);wr(CFG,cfg|(1u<<3));
        int error=flush_card();
        wr(INTR,rd(INTR));wr(CFG,cfg);wr(INTEN,inten);
        lck_mtx_unlock(&msdc_lock);return error;
    }
    case DKIOCGETMAXSEGMENTCOUNTREAD:
    case DKIOCGETMAXSEGMENTCOUNTWRITE: *(uint64_t *)data=1;return 0;
    default:return ENOTTY;
    }
}
static int disk_psize(dev_t dev) { return (int)(minor(dev) ? root_sectors : disk_sectors); }
static int read_disk_sector(uint32_t block,uint8_t sector[512]) {
    lck_mtx_lock(&msdc_lock);
    uint32_t cfg=rd(CFG),inten=rd(INTEN);
    wr(INTEN,0);wr(CFG,cfg|(1u<<3));
    int error=read_block(17,block,sector);
    wr(INTR,rd(INTR));wr(CFG,cfg);wr(INTEN,inten);
    lck_mtx_unlock(&msdc_lock);
    return error;
}
static int disk_read(dev_t dev,struct uio *uio,__unused int flags) {
    uint8_t sector[512];
    uint32_t first=minor(dev) ? root_first : 0;
    uint64_t capacity=(uint64_t)(minor(dev) ? root_sectors : disk_sectors)*512;
    if (uio_offset(uio)<0) return EINVAL;
    while (uio_resid(uio)>0 && (uint64_t)uio_offset(uio)<capacity) {
        uint64_t offset=(uint64_t)uio_offset(uio);
        unsigned skip=(unsigned)(offset&511u);
        int count=(int)MIN((uint64_t)uio_resid(uio),512u-skip);
        int error=read_disk_sector(first+(uint32_t)(offset/512),sector);
        if (error) return error;
        /* uio selects copyout for user buffers and preserves fault recovery. */
        error=uiomove((caddr_t)sector+skip,count,uio);
        if (error) return error;
    }
    return 0;
}
static int disk_write(__unused dev_t dev,__unused struct uio *uio,__unused int flags) { return EROFS; }
static const struct bdevsw msdc_bdevsw = {
    .d_open=disk_open,.d_close=disk_close,.d_strategy=disk_strategy,
    .d_ioctl=disk_ioctl,.d_dump=eno_dump,.d_psize=disk_psize,.d_type=D_DISK
};
static const struct cdevsw msdc_cdevsw = {
    .d_open=disk_open,.d_close=disk_close,.d_read=disk_read,.d_write=disk_write,
    .d_ioctl=disk_ioctl,.d_stop=eno_stop,.d_reset=eno_reset,.d_select=eno_select,
    .d_mmap=eno_mmap,.d_strategy=eno_strat,.d_reserved_1=eno_getc,.d_reserved_2=eno_putc,.d_type=D_DISK
};
static int register_disk(uint32_t sectors) {
    int bmajor=bdevsw_add(-1,&msdc_bdevsw);
    if (bmajor<0) return ENXIO;
    int cmajor=cdevsw_add_with_bdev(-1,&msdc_cdevsw,bmajor);
    if (cmajor<0) return ENXIO;
    disk_sectors=sectors;
    if (!devfs_make_node(makedev(bmajor,0),DEVFS_BLOCK,UID_ROOT,GID_OPERATOR,0440,"emmc0") ||
        !devfs_make_node(makedev(cmajor,0),DEVFS_CHAR,UID_ROOT,GID_OPERATOR,0440,"remmc0")) return ENXIO;
    if (root_sectors) {
        root_device=makedev(bmajor,1);
        if (!devfs_make_node(root_device,DEVFS_BLOCK,UID_ROOT,GID_OPERATOR,0440,"emmcroot0") ||
            !devfs_make_node(makedev(cmajor,1),DEVFS_CHAR,UID_ROOT,GID_OPERATOR,0440,"remmcroot0")) return ENXIO;
    }
    if(boot_update_enabled && boot_sectors) {
        int major=cdevsw_add(-1,&boot_cdevsw);
        if(major<0 || !devfs_make_node(makedev(major,0),DEVFS_CHAR,UID_ROOT,GID_WHEEL,0600,"a04boot")) {
            printf("BEGONIA BOOT UPDATE: device registration failed\n");
        } else printf("BEGONIA BOOT UPDATE: /dev/a04boot root-only stage+verified commit; first=%u sectors=%u\n",boot_first,boot_sectors);
    }
    printf("BEGONIA MSDC0: full disk read-only; userdata writes=%u cache=%u\n",write_enabled,cache_enabled);
    return 0;
}
static int find_root_partition(const uint8_t header[512],uint32_t sectors) {
    static uint8_t entries[128*128];
    uint64_t lba=le64(header+72);
    uint32_t count=le32(header+80),size=le32(header+84),expected=le32(header+88);
    uint64_t first_usable=le64(header+40),last_usable=le64(header+48);
    if (!count || count>128 || size!=128 || lba>=sectors || (count*size+511)/512>sectors-lba) return EINVAL;
    unsigned bytes=count*size;
    for (unsigned at=0;at<bytes;at+=512) {
        uint8_t block[512];int error=read_block(17,(uint32_t)lba+at/512,block);
        if (error) return error;
        memcpy(entries+at,block,MIN(512u,bytes-at));
    }
    uint32_t crc=~0u;
    for (unsigned i=0;i<bytes;i++) {
        crc^=entries[i];
        for (unsigned j=0;j<8;j++) crc=(crc>>1)^(0xedb88320u & (0u-(crc&1u)));
    }
    if (~crc!=expected) return EINVAL;
    /* Discover original per-device NVRAM by the CRC-verified GPT name. No
     * fixed LBA, image input, extraction step, or write access is introduced. */
    static const char nvname[]="nvram";
    uint32_t nvfirst=0,nvcount=0;unsigned nvinvalid=0;
    for(unsigned i=0;i<count;i++) {
        const uint8_t *e=entries+i*size;unsigned n=0;
        while(n<sizeof(nvname)-1 && e[56+n*2]==nvname[n] && !e[57+n*2])n++;
        if(n!=sizeof(nvname)-1 || e[56+n*2] || e[57+n*2])continue;
        uint64_t first=le64(e+32),last=le64(e+40);
        if(nvcount || first<first_usable || last>last_usable || first>last || last>=sectors ||
           (last-first+1)>0x4000000u/512){nvinvalid=1;continue;}
        nvfirst=(uint32_t)first;nvcount=(uint32_t)(last-first+1);
    }
    nvram_first=nvinvalid?0:nvfirst;nvram_sectors=nvinvalid?0:nvcount;
    static const char wanted[]="userdata";
    for (unsigned i=0;i<count;i++) {
        const uint8_t *e=entries+i*size;unsigned n=0;
        while (n<sizeof(wanted)-1 && e[56+n*2]==wanted[n] && !e[57+n*2]) n++;
        if (n!=sizeof(wanted)-1 || e[56+n*2] || e[57+n*2]) continue;
        uint64_t first=le64(e+32),last=le64(e+40);
        if (root_sectors || first<first_usable || last>last_usable || first>last || last>=sectors) return EINVAL;
        root_first=(uint32_t)first;root_sectors=(uint32_t)(last-first+1);
    }
    if(nvram_sectors && root_sectors &&
       (uint64_t)nvram_first<(uint64_t)root_first+root_sectors &&
       (uint64_t)root_first<(uint64_t)nvram_first+nvram_sectors){nvram_first=nvram_sectors=0;}
    printf("BEGONIA MSDC0: GPT entries CRC verified; userdata first=%u sectors=%u\n",root_first,root_sectors);
    return root_sectors ? 0 : ENOENT;
}
int begonia_emmc_rootdev(dev_t *dev);
int begonia_emmc_rootdev(dev_t *dev) {
    if (!root_sectors || !root_device) return ENXIO;
    *dev=root_device;return 0;
}
/* Runtime calibration consumer; all reads remain inside the discovered
 * NVRAM partition and share the existing MSDC lock with root filesystem IO. */
static int nvram_read_bytes(void *context,uint32_t offset,void *out,unsigned count)
{
    (void)context;
    uint8_t sector[512];uint8_t *destination=out;
    uint64_t size=(uint64_t)nvram_sectors*512;
    if(offset>size || count>size-offset)return EINVAL;
    while(count){
        unsigned within=offset&511u,n=512-within;if(n>count)n=count;
        uint32_t block=offset/512;
        if(block>=nvram_sectors || nvram_first>=disk_sectors || block>=disk_sectors-nvram_first)return EINVAL;
        int error=read_block(17,nvram_first+block,sector);if(error)return error;
        memcpy(destination,sector+within,n);destination+=n;offset+=n;count-=n;
    }
    return 0;
}
int begonia_wlan_read_calibration(void *bytes,unsigned capacity);
int begonia_wlan_read_calibration(void *bytes,unsigned capacity)
{
    if(!bytes || capacity!=512)return EINVAL;
    if(!root_device || !nvram_sectors || nvram_sectors>0x4000000u/512)return ENXIO;
    struct begonia_nvram_work *work=kalloc_type(struct begonia_nvram_work,Z_WAITOK|Z_ZERO);
    if(!work)return ENOMEM;
    const struct begonia_nvram_reader reader={NULL,nvram_sectors*512,nvram_read_bytes};
    lck_mtx_lock(&msdc_lock);
    int error=begonia_nvram_wifi(&reader,work,bytes);
    lck_mtx_unlock(&msdc_lock);
    bzero(work,sizeof(*work));kfree_type(struct begonia_nvram_work,work);
    printf("BEGONIA WLAN: runtime NVRAM calibration read=%d bytes=%u; no disk writes\n",error,error?0u:512u);
    return error;
}
extern void begonia_sample_start(void);
void begonia_msdc_probe(void);
void begonia_msdc_probe(void) {
    unsigned enabled=0;
    if (!PE_parse_boot_argn("a04br3.msdc_probe",&enabled,sizeof(enabled)) || !enabled) return;
    msdc=ml_io_map(0x11230000,0x1000);
    uint32_t cfg=rd(CFG),inten=rd(INTEN),response=0;
    printf("BEGONIA MSDC0: inherited CFG=0x%08x STS=0x%08x\n",cfg,rd(STS));
    /* No reset: retain LK's card clock, voltage, bus width and sampling. */
    wr(INTEN,0);wr(CFG,cfg|(1u<<3));
    int error=command(13,1u<<16,0,&response);
    if (!error && ((response>>9)&15u)!=4u) error=ENXIO;
    uint8_t sector[512];uint32_t sectors=0;
    if (!error) error=read_block(8,0,sector);
    if (!error) {
        sectors=le32(sector+212);
        cache_enabled=sector[33]&1u;
        printf("BEGONIA MSDC0: EXT_CSD rev=%u sectors=%u partition_access=%u\n",
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
        printf("BEGONIA MSDC0: GPT header CRC verified at LBA=%u entries_LBA=%llu count=%u size=%u\n",
            locations[i],le64(sector+72),le32(sector+80),le32(sector+84));
        error=find_root_partition(sector,sectors);
        if(!error) {
            boot_update_enabled=(PE_parse_boot_argn("a04br3.console",&enabled,sizeof(enabled)) ||
                PE_parse_boot_argn("a04br3.boot_update",&enabled,sizeof(enabled))) && enabled==1;
            if(boot_update_enabled) {
                int be=boot_parse_gpt(sector,sectors,&boot_first,&boot_sectors);
                if(!be)boot_gpt_lba=locations[i];
                else {boot_first=boot_sectors=0;printf("BEGONIA BOOT UPDATE: unavailable GPT error=%d\n",be);}
            }
        }
        found=!error;break;
    }
    if (!error && !found) error=ENOENT;
    unsigned use_dma=0;
    if(!error && PE_parse_boot_argn("a04br3.msdc_dma",&use_dma,sizeof(use_dma)) && use_dma==1){
        kern_return_t kr=kmem_alloc_contig(kernel_map,&dma_area,PAGE_SIZE+65536,
            PAGE_MASK,(ppnum_t)(0xffffffffULL>>PAGE_SHIFT),0,KMA_ZERO,VM_KERN_MEMORY_BSD);
        int de=kr==KERN_SUCCESS?0:ENOMEM;
        if(!de){
            dma_pa=(uint64_t)pmap_find_phys(kernel_pmap,(addr64_t)dma_area)<<PAGE_SHIFT;
            if(!dma_pa)panic("BEGONIA MSDC DMA: unmapped allocation");
            uint8_t reference[2048],result[2048];
            de=read_blocks(17,0x40,reference,4);
            if(!de)de=dma_read(0x40,result,4);
            if(!de && memcmp(reference,result,sizeof(reference)))de=EIO;
            if(!de)dma_enabled=1;
        }
        printf("BEGONIA MSDC DMA: PIO comparison %s error=%d PA=0x%llx\n",
            dma_enabled?"PASS":"FAILED, retaining PIO",de,dma_pa);
    }
    wr(INTR,rd(INTR));wr(CFG,cfg);wr(INTEN,inten);
    if (!error) {
        unsigned enabled=0;
        write_enabled=PE_parse_boot_argn("a04br3.emmc_rw",&enabled,sizeof(enabled)) && enabled==1;
        error=register_disk(sectors);
        if (!error) begonia_sample_start();
    }
    printf("BEGONIA MSDC0: read-only probe %s error=%d R1=0x%08x\n",
        error ? "FAILED" : "PASS",error,response);
}
#endif
