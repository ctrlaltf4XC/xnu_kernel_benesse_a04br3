/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MTK GPT NVRAM read-only runtime WIFI lookup. Geometry and checksums follow
 * libnvram NVM_GetDeviceInfo/RestoreFromBinRegion_OneFile/CheckData_OneFile/
 * ComputeCheckNo. Records are original on-device bytes, never defaults.
 * Caller serializes controller access. read() is bounded to this partition.
 */
#ifndef BEGONIA_WLAN_NVRAM_H
#define BEGONIA_WLAN_NVRAM_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <sys/errno.h>
#define BEGONIA_NVRAM_BLOCK 0x20000u
#define BEGONIA_NVRAM_MAX_BLOCKS 512u
struct begonia_nvram_reader {void *context;uint32_t bytes;int (*read)(void *,uint32_t,void *,unsigned);};
struct begonia_nvram_work {uint8_t bitmap[BEGONIA_NVRAM_MAX_BLOCKS];uint8_t scratch[BEGONIA_NVRAM_MAX_BLOCKS*3+4];uint8_t record[514];};
static inline uint32_t begonia_nvram_le32(const uint8_t *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static inline int begonia_nvram_read(const struct begonia_nvram_reader *r,uint32_t off,void *out,unsigned n)
{
 if(!r || !r->read || !out || off>r->bytes || n>r->bytes-off)return EINVAL;
 return r->read(r->context,off,out,n);
}
/* Logical AllFile blocks skip the same bitmap-marked bad blocks as libnvram. */
static inline int begonia_nvram_file_read(const struct begonia_nvram_reader *r,const struct begonia_nvram_work *w,
 unsigned blocks,unsigned start,uint32_t off,void *out,unsigned n)
{
 uint8_t *p=(uint8_t *)out;unsigned index=off/BEGONIA_NVRAM_BLOCK,inside=off%BEGONIA_NVRAM_BLOCK,b=start;
 for(;b<blocks;b++)if(!w->bitmap[b]){if(!index)break;index--;}
 while(n){
  if(b>=blocks)return EINVAL;
  unsigned part=BEGONIA_NVRAM_BLOCK-inside;if(part>n)part=n;
  int e=begonia_nvram_read(r,b*BEGONIA_NVRAM_BLOCK+inside,p,part);if(e)return e;
  n-=part;p+=part;inside=0;
  do{b++;}while(b<blocks && w->bitmap[b]);
 }
 return 0;
}
static inline int begonia_nvram_wifi(const struct begonia_nvram_reader *r,struct begonia_nvram_work *w,uint8_t out[512])
{
 if(!r || !w || !out || r->bytes%BEGONIA_NVRAM_BLOCK)return EINVAL;
 unsigned total=r->bytes/BEGONIA_NVRAM_BLOCK;
 if(total<3 || total>BEGONIA_NVRAM_MAX_BLOCKS)return EINVAL;
 /* Match libnvram's descending search and triple bitmap + 0xaabbccdd flag.
  * The bitmap length is recovered from its flag position, not a baked-in GPT
  * partition length or a backup's offsets. No speculative file bytes accepted. */
 unsigned blocks=0,bitmap_block=0;
 for(unsigned at=total;at>0 && !blocks;){
  at--;int e=begonia_nvram_read(r,at*BEGONIA_NVRAM_BLOCK,w->scratch,total*3+4);if(e)return e;
  for(unsigned n=at+1;n<=total;n++){
   if(begonia_nvram_le32(w->scratch+3*n)!=0xaabbccddu)continue;
   unsigned valid=1;
   for(unsigned i=0;i<n;i++)if(w->scratch[i]!=w->scratch[i+n] || w->scratch[i]!=w->scratch[i+2*n]){valid=0;break;}
   if(valid){blocks=n;bitmap_block=at;memcpy(w->bitmap,w->scratch,n);break;}
  }
 }
 if(!blocks)return ENOENT;
 unsigned map=0;while(map<bitmap_block && w->bitmap[map])map++;
 if(map>=bitmap_block)return EINVAL;
 int e=begonia_nvram_read(r,map*BEGONIA_NVRAM_BLOCK,w->scratch,64);if(e)return e;
 uint32_t mapsize=begonia_nvram_le32(w->scratch),datasize=begonia_nvram_le32(w->scratch+4);
 uint32_t expected=begonia_nvram_le32(w->scratch+12);
 if(begonia_nvram_le32(w->scratch+8)!=0xffffffffu || mapsize<52 || (mapsize-52)%140 ||
    mapsize+12>BEGONIA_NVRAM_BLOCK || !datasize || datasize>r->bytes)return EINVAL;
 unsigned data=map+1;while(data<bitmap_block && w->bitmap[data])data++;
 if(data>=bitmap_block)return EINVAL;
 /* AllMap uses 12-byte container header then 52-byte map header. */
 uint32_t wifi_offset=0;unsigned matches=0;
 static const char name[]="/data/nvram/APCFG/APRDEB/WIFI";
 for(uint32_t at=52;at<mapsize;at+=140){
  e=begonia_nvram_read(r,map*BEGONIA_NVRAM_BLOCK+12+at,w->scratch,140);if(e)return e;
  uint32_t offset=begonia_nvram_le32(w->scratch+4),size=begonia_nvram_le32(w->scratch+8);
  if(!size || offset>datasize || size>datasize-offset)return EINVAL;
  if(!memcmp(w->scratch+12,name,sizeof(name))){
   if(begonia_nvram_le32(w->scratch)!=7 || size!=514 || matches++)return EINVAL;
   wifi_offset=offset;
  }
 }
 if(matches!=1)return ENOENT;
 uint32_t checksum=0;
 for(uint32_t off=0;off<datasize;){
  unsigned n=datasize-off;if(n>512)n=512;
  e=begonia_nvram_file_read(r,w,bitmap_block,data,off,w->scratch,n);if(e)return e;
  unsigned whole=n&~3u;
  for(unsigned i=0;i<whole;i+=4){uint32_t v=begonia_nvram_le32(w->scratch+i);checksum=((off+i)&4)?checksum+v:checksum^v;}
  uint32_t tail=0;for(unsigned i=whole;i<n;i++)tail|=(uint32_t)w->scratch[i]<<(8*(i-whole));
  checksum+=tail;off+=n;
 }
 if((checksum^datasize)!=expected)return EINVAL;
 e=begonia_nvram_file_read(r,w,bitmap_block,data,wifi_offset,w->record,514);if(e)return e;
 unsigned check=0;for(unsigned i=0;i<512;i++)check=((i&1)?check^w->record[i]:check+w->record[i])&255;
 if(w->record[512]!=0xaa || w->record[513]!=check)return EINVAL;
 unsigned any=0;for(unsigned i=4;i<10;i++)any|=w->record[i];
 if(!any || (w->record[4]&1) || w->record[196]!=1)return EINVAL;
 memcpy(out,w->record,512);return 0;
}
#endif
