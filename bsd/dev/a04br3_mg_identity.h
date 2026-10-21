/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Read only the three MG identity answers needed for a bounded cache rotation.
 * Unknown formats/identities fail closed; this never edits a plist value.
 */
#ifndef A04BR3_MG_IDENTITY_H
#define A04BR3_MG_IDENTITY_H
#define A04_MG_MAX_CACHE (256u * 1024u)
struct a04_mg_plist { const unsigned char *s; size_t table; uint64_t objects,root; unsigned os,rs; };
static uint64_t mg_uint(const unsigned char *p,unsigned n)
{
    uint64_t v=0;for(unsigned i=0;i<n;i++)v=(v<<8)|p[i];return v;
}
static int mg_object(const struct a04_mg_plist *p,uint64_t ref,size_t *at)
{
    if(ref>=p->objects)return EINVAL;
    uint64_t off=mg_uint(p->s+p->table+ref*p->os,p->os);
    if(off<8 || off>=p->table)return EINVAL;
    *at=(size_t)off;return 0;
}
static int mg_count(const struct a04_mg_plist *p,size_t *at,unsigned type,uint64_t *n)
{
    if(*at>=p->table || p->s[*at]>>4!=type)return EINVAL;
    *n=p->s[(*at)++]&15;
    if(*n==15){
        if(*at>=p->table || p->s[*at]>>4!=1 || (p->s[*at]&15)>3)return EINVAL;
        unsigned bytes=1u<<(p->s[(*at)++]&15);
        if(bytes>p->table-*at)return EINVAL;
        *n=mg_uint(p->s+*at,bytes);*at+=bytes;
    }
    return 0;
}
static int mg_string(const struct a04_mg_plist *p,uint64_t ref,const char *wanted,int *equal)
{
    size_t at=0;uint64_t n=0;*equal=0;int error=mg_object(p,ref,&at);if(error)return error;
    unsigned type=p->s[at]>>4;if(type!=5 && type!=6)return ENOTSUP;
    error=mg_count(p,&at,type,&n);if(error)return error;
    unsigned unit=type==6?2:1;if(n>(p->table-at)/unit)return EINVAL;
    if(n!=strlen(wanted))return 0;
    for(size_t i=0;i<(size_t)n;i++)if((unit==2 && p->s[at+2*i]) ||
        p->s[at+unit*i+unit-1]!=(unsigned char)wanted[i])return 0;
    *equal=1;return 0;
}
static int mg_find(const struct a04_mg_plist *p,uint64_t dict,const char *key,uint64_t *value)
{
    size_t at=0;uint64_t n=0;int error=mg_object(p,dict,&at);if(error)return error;
    error=mg_count(p,&at,13,&n);if(error)return error;
    if(n>4096 || n>(p->table-at)/(2*p->rs))return EINVAL;
    int found=0;
    for(uint64_t i=0;i<n;i++){
        int equal=0;uint64_t ref=mg_uint(p->s+at+i*p->rs,p->rs);
        error=mg_string(p,ref,key,&equal);if(error)return error;
        if(equal){if(found)return EINVAL;*value=mg_uint(p->s+at+(n+i)*p->rs,p->rs);if(*value>=p->objects)return EINVAL;found=1;}
    }
    return found?0:ENOENT;
}
static int a04_mg_cache_identity(const unsigned char *s,size_t len,int *stale)
{
    *stale=0;
    if(len<40 || len>A04_MG_MAX_CACHE || memcmp(s,"bplist00",8))return ENOTSUP;
    const unsigned char *tr=s+len-32;struct a04_mg_plist p={.s=s,.os=tr[6],.rs=tr[7]};
    if(!p.os || p.os>8 || !p.rs || p.rs>8)return EINVAL;
    p.objects=mg_uint(tr+8,8);p.root=mg_uint(tr+16,8);uint64_t table=mg_uint(tr+24,8);
    if(table<8 || table>len-32 || !p.objects || p.objects>8192 || p.root>=p.objects ||
        p.objects>(len-32-table)/p.os)return EINVAL;
    p.table=(size_t)table;
    uint64_t extra=0,dc=0,product=0,data=0;int error=mg_find(&p,p.root,"CacheExtra",&extra);
    if(!error)error=mg_find(&p,extra,"+3Uf0Pm5F8Xy7Onyvko0vA",&dc);
    if(!error)error=mg_find(&p,extra,"h9jDsbgj7xIVeIQ8S3/X3Q",&product);
    if(!error)error=mg_find(&p,p.root,"CacheData",&data);
    if(error)return error;
    size_t at=0;uint64_t count=0;error=mg_object(&p,data,&at);
    if(!error)error=mg_count(&p,&at,4,&count);
    if(error)return error;
    if(count!=0x14e2 || count>p.table-at || !s[at+0x1290+93])return EINVAL;
    /* This exact donor layout uses an eight-byte slot for CFNumber SInt32. */
    uint32_t number=(uint32_t)s[at+93*8] | (uint32_t)s[at+93*8+1]<<8 |
        (uint32_t)s[at+93*8+2]<<16 | (uint32_t)s[at+93*8+3]<<24;
    int ipad=0,ipod=0,current=0,old=0;
    error=mg_string(&p,dc,"iPad",&ipad);
    if(!error)error=mg_string(&p,dc,"iPod",&ipod);
    if(!error)error=mg_string(&p,product,"iPad-A04BR3",&current);
    if(!error)error=mg_string(&p,product,"Benesse TAB-A04-BR3",&old);
    if(error)return error;
    if(ipad && current && number==3)return 0;
    if(ipod && (old || current) && number==2){*stale=1;return 0;}
    return EBUSY;
}
#endif
