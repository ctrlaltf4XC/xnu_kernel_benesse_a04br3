/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Private BEGONIA boot updater ABI v1. Framing follows the tracked device
 * tools/bootimg.py (Android v0 + MTK KERNEL/ROOTFS, never a raw disk writer).
 * Include integer, string, errno and ioctl declarations before this file.
 */
#ifndef BEGONIA_BOOT_UPDATE_H
#define BEGONIA_BOOT_UPDATE_H
#define BEGONIA_BOOT_VERSION 1u
#define BEGONIA_BOOT_MAX_BYTES (128ULL*1024*1024)
#define BEGONIA_BOOT_STAGING 1u
#define BEGONIA_BOOT_VERIFIED 2u
#define BEGONIA_BOOT_FAILED 4u
struct begonia_boot_info {
    uint32_t version,flags;
    uint64_t partition_bytes,staged_bytes,image_bytes;
};
struct begonia_boot_begin { uint64_t image_bytes;uint8_t sha256[32];uint64_t reserved; };
#define BEGONIA_BOOT_INFO _IOR('B',0x60,struct begonia_boot_info)
#define BEGONIA_BOOT_BEGIN _IOW('B',0x61,struct begonia_boot_begin)
#define BEGONIA_BOOT_COMMIT _IO('B',0x62)
#define BEGONIA_BOOT_ABORT _IO('B',0x63)
/* Delta transfer: append bytes copied from the CURRENT boot partition at a
 * 512-aligned source offset to the staging image. Only staged RAM changes;
 * the final full-image SHA/header/GPT/readback COMMIT gate is unchanged. */
struct begonia_boot_copy { uint64_t source,bytes; };
#define BEGONIA_BOOT_COPY _IOW('B',0x64,struct begonia_boot_copy)
_Static_assert(sizeof(struct begonia_boot_copy)==16,"boot copy ABI");
_Static_assert(sizeof(struct begonia_boot_info)==32,"boot info ABI");
_Static_assert(sizeof(struct begonia_boot_begin)==48,"boot begin ABI");
static inline uint32_t begonia_boot_u32(const uint8_t *p) {
    return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static inline uint64_t begonia_boot_u64(const uint8_t *p) {
    return begonia_boot_u32(p)|((uint64_t)begonia_boot_u32(p+4)<<32);
}
static inline uint32_t begonia_boot_crc(const uint8_t *p,unsigned bytes,int header) {
    uint32_t crc=~0u;
    for(unsigned i=0;i<bytes;i++) {
        crc^=header && i>=16 && i<20 ? 0 : p[i];
        for(unsigned j=0;j<8;j++)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1u)));
    }
    return ~crc;
}
/* Reject duplicate names, unallocated GUIDs, overlaps with ANY live partition,
 * usable-LBA violations and entry/header CRC errors. No guessed/fixed LBAs. */
static inline int begonia_boot_gpt(const uint8_t *h,const uint8_t *entries,
    uint32_t sectors,uint32_t *first,uint32_t *count) {
    uint32_t n=begonia_boot_u32(h+80),size=begonia_boot_u32(h+84),hs=begonia_boot_u32(h+12);
    uint64_t table=begonia_boot_u64(h+72),lo=begonia_boot_u64(h+40),hi=begonia_boot_u64(h+48);
    *first=*count=0;
    if(memcmp(h,"EFI PART",8) || hs<92 || hs>512 || !n || n>128 || size!=128 ||
        begonia_boot_u32(h+16)!=begonia_boot_crc(h,hs,1) || lo>hi || hi>=sectors ||
        table>=sectors || (n*size+511)/512>sectors-table ||
        begonia_boot_u32(h+88)!=begonia_boot_crc(entries,n*size,0))return EINVAL;
    uint64_t b=0,e=0;unsigned matches=0;
    for(unsigned i=0;i<n;i++) {
        const uint8_t *p=entries+i*128;
        if(memcmp(p+56,"b\0o\0o\0t\0\0\0",10))continue;
        if(++matches>1)return EINVAL;
        unsigned type=0;for(unsigned j=0;j<16;j++)type|=p[j];
        b=begonia_boot_u64(p+32);e=begonia_boot_u64(p+40);
        if(!type || b<lo || e>hi || b>e || b==0 ||
            (table<=e && table+(n*size+511)/512>b) ||
            (begonia_boot_u64(h+24)>=b && begonia_boot_u64(h+24)<=e) ||
            (begonia_boot_u64(h+32)>=b && begonia_boot_u64(h+32)<=e))return EINVAL;
    }
    if(matches!=1)return ENOENT;
    for(unsigned i=0;i<n;i++) {
        const uint8_t *p=entries+i*128;unsigned type=0;
        for(unsigned j=0;j<16;j++)type|=p[j];
        if(!type || !memcmp(p+56,"b\0o\0o\0t\0\0\0",10))continue;
        uint64_t f=begonia_boot_u64(p+32),l=begonia_boot_u64(p+40);
        if(f>l || f<lo || l>hi || (f<=e && l>=b))return EINVAL;
    }
    *first=(uint32_t)b;*count=(uint32_t)(e-b+1);return 0;
}
static inline int begonia_boot_length(uint64_t bytes,uint64_t partition) {
    return bytes<6144 || bytes>BEGONIA_BOOT_MAX_BYTES || bytes>partition || (bytes&2047u) ? EINVAL : 0;
}
/* Require the exact framing emitted by current fixed source, including load
 * addresses and preloader bootopt. SHA256 authenticates transport integrity;
 * this is deliberately not an image signature/trust verification mechanism. */
static inline int begonia_boot_image(const uint8_t *p,uint64_t bytes) {
    static const char opt[]="bootopt=64S3,32N2,64N2";
    if(begonia_boot_length(bytes,BEGONIA_BOOT_MAX_BYTES) || memcmp(p,"ANDROID!",8) ||
        begonia_boot_u32(p+12)!=0x40080000 || begonia_boot_u32(p+20)!=0x50000000 ||
        begonia_boot_u32(p+24) || begonia_boot_u32(p+28)!=0x40f00000 ||
        begonia_boot_u32(p+32)!=0x4e000000 || begonia_boot_u32(p+36)!=2048 ||
        begonia_boot_u32(p+40) || begonia_boot_u32(p+44) ||
        memcmp(p+64,opt,sizeof(opt)-1) ||
        (p[64+sizeof(opt)-1] && p[64+sizeof(opt)-1]!=' '))return EINVAL;
    uint64_t k=begonia_boot_u32(p+8),r=begonia_boot_u32(p+16);
    uint64_t roff=2048+((k+2047)&~2047ULL);
    if(k<512+18 || r<512+688 || roff>bytes || r>bytes-roff ||
        roff+((r+2047)&~2047ULL)!=bytes)return EINVAL;
    const uint8_t *sections[2]={p+2048,p+roff};
    const char *names[2]={"KERNEL","ROOTFS"};uint64_t lengths[2]={k,r};
    for(unsigned i=0;i<2;i++) {
        const uint8_t *s=sections[i];
        if(begonia_boot_u32(s)!=0x58881688 || begonia_boot_u32(s+4)!=lengths[i]-512 ||
            memcmp(s+8,names[i],6))return EINVAL;
        for(unsigned j=14;j<40;j++)if(s[j])return EINVAL;
    }
    if(p[2560]!=0x1f || p[2561]!=0x8b || p[2562]!=8 ||
        memcmp(sections[1]+512,"A04XNU01",8))return EINVAL;
    return 0;
}
#endif
