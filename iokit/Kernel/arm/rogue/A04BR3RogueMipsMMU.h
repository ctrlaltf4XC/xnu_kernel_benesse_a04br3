/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MT8167 Rogue MIPS EntryLo builder. Format credit: Imagination/MediaTek
 * m1.11ED5425693 rgx_mips.h and rgxmipsmmuinit.c. No hardware activation.
 * Use heap offsets, NOT MIPS virtual code addresses, for table indexing.
 */
#ifndef A04BR3_ROGUE_MIPS_MMU_H
#define A04BR3_ROGUE_MIPS_MMU_H
/* Shared by the device reservation and offline table construction. */
static inline int a04_rgx_mips_physical_range(uint64_t pa,uint32_t bytes,uint32_t alignment)
{
 return alignment && !(alignment&(alignment-1)) && bytes && pa &&
        !(pa&(alignment-1)) && !(bytes&4095) &&
        pa<0x100000000ULL && bytes<=0x100000000ULL-pa;
}
static inline int a04_rgx_mips_map(uint32_t *table,unsigned entries,uint32_t offset,
    uint64_t physical,uint32_t bytes,int writable,int executable)
{
 if(!table || !entries || entries>8192 || !bytes ||
    ((offset|physical|bytes)&4095) || physical>=0x100000000ULL ||
    bytes>0x100000000ULL-physical)return 1;
 unsigned first=offset>>12,count=bytes>>12;
 if(first>=entries || count>entries-first)return 2;
 /* Validate the entire range before changing any entry. */
 for(unsigned n=0;n<count;n++)if(table[first+n])return 3;
 for(unsigned n=0;n<count;n++){
  uint32_t pfn=(uint32_t)((physical+((uint64_t)n<<12))>>6)&0x03ffffc0u;
  /* Uncached until both host and firmware cache maintenance are present. */
  table[first+n]=pfn|0x13u|(writable?4u:0u)|(executable?0u:0x40000000u);
 }
 return 0;
}
#endif
