/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Append a new root dictionary to a native binary plist without rewriting
 * existing objects. All unrelated values (including nested objects) survive.
 * Object-reference width stays unchanged; reject files needing a wider width.
 */
#ifndef A04BR3_BPLIST_H
#define A04BR3_BPLIST_H
#define A04_FP_MAX_INPUT 16384u
#define A04_FP_MAX_OUTPUT 32768u
static uint64_t a04_bp_uint(const unsigned char *p,unsigned n)
{
    uint64_t v=0; for(unsigned i=0;i<n;i++)v=(v<<8)|p[i]; return v;
}
static void a04_bp_put(unsigned char *p,uint64_t v,unsigned n)
{
    for(unsigned i=n;i;i--){p[i-1]=(unsigned char)v;v>>=8;}
}
static int a04_bp_count(const unsigned char *s,size_t limit,size_t *pos,uint64_t *count)
{
    if(*pos>=limit)return EINVAL;
    unsigned tag=s[(*pos)++]; *count=tag&15;
    if(*count==15){
        if(*pos>=limit || (s[*pos]>>4)!=1 || (s[*pos]&15)>3)return EINVAL;
        unsigned n=1u<<(s[(*pos)++]&15);
        if(n>limit-*pos)return EINVAL;
        *count=a04_bp_uint(s+*pos,n);*pos+=n;
    }
    return 0;
}
static size_t a04_bp_tag(unsigned char *p,unsigned type,uint64_t count)
{
    if(count<15){p[0]=(unsigned char)(type|count);return 1;}
    unsigned n=count<=255?1:2;p[0]=(unsigned char)(type|15);p[1]=n==1?0x10:0x11;
    a04_bp_put(p+2,count,n);return 2+n;
}
static int a04_bp_disable_fp(const unsigned char *s,size_t len,unsigned char *out,size_t cap,size_t *written)
{
    static const char key[]="com.apple.fairplayd.H2";
    if(len<40 || len>A04_FP_MAX_INPUT || memcmp(s,"bplist00",8))return ENOTSUP;
    const unsigned char *tr=s+len-32;
    unsigned os=tr[6],rs=tr[7];
    if(!os || os>8 || !rs || rs>8)return EINVAL;
    uint64_t objects=a04_bp_uint(tr+8,8),root=a04_bp_uint(tr+16,8),table=a04_bp_uint(tr+24,8);
    if(!objects || objects>1024 || root>=objects || table<8 || table>len-32 ||
        objects>(len-32-table)/os)return EINVAL;
    uint64_t rootat=a04_bp_uint(s+table+root*os,os);
    if(rootat<8 || rootat>=table || (s[rootat]>>4)!=13)return EINVAL;
    size_t pos=(size_t)rootat;uint64_t count=0;
    int error=a04_bp_count(s,(size_t)table,&pos,&count);
    if(error || count>1024 || count>(table-pos)/(2*rs))return EINVAL;
    uint64_t found=UINT64_MAX;
    for(uint64_t i=0;i<count;i++){
        uint64_t kr=a04_bp_uint(s+pos+i*rs,rs),vr=a04_bp_uint(s+pos+(count+i)*rs,rs);
        if(kr>=objects || vr>=objects)return EINVAL;
        uint64_t at=a04_bp_uint(s+table+kr*os,os);
        if(at<8 || at>=table)return EINVAL;
        unsigned type=s[at]>>4;
        if(type!=5 && type!=6)return ENOTSUP;
        size_t kp=(size_t)at;uint64_t chars=0;
        if(a04_bp_count(s,(size_t)table,&kp,&chars))return EINVAL;
        unsigned unit=type==6?2:1;
        if(chars>(table-kp)/unit)return EINVAL;
        int equal=chars==sizeof(key)-1;
        if(equal)for(size_t j=0;j<sizeof(key)-1;j++){
            if((unit==2 && s[kp+2*j]!=0) || s[kp+unit*j+unit-1]!=(unsigned char)key[j])equal=0;
        }
        if(equal){if(found!=UINT64_MAX)return EINVAL;found=i;}
    }
    if(found!=UINT64_MAX){
        uint64_t vr=a04_bp_uint(s+pos+(count+found)*rs,rs);
        uint64_t at=a04_bp_uint(s+table+vr*os,os);
        if(at<8 || at>=table)return EINVAL;
        if(s[at]==9){if(len>cap)return ENOSPC;memcpy(out,s,len);*written=len;return 0;}
        if(s[at]!=8)return ENOTSUP;
    }
    uint64_t extra=found==UINT64_MAX?3:2,nobjects=objects+extra;
    if(rs<8 && nobjects-1>=(1ULL<<(rs*8)))return ENOSPC;
    uint64_t n=count+(found==UINT64_MAX),need=table+64+2*n*rs+nobjects*8+32;
    if(need>cap || n>1024)return ENOSPC;
    memcpy(out,s,(size_t)table);size_t end=(size_t)table;
    uint64_t valueat=end;out[end++]=9;
    uint64_t keyat=0;
    if(found==UINT64_MAX){keyat=end;end+=a04_bp_tag(out+end,0x50,sizeof(key)-1);memcpy(out+end,key,sizeof(key)-1);end+=sizeof(key)-1;}
    uint64_t newroot=end;end+=a04_bp_tag(out+end,0xd0,n);
    for(uint64_t i=0;i<count;i++){memcpy(out+end,s+pos+i*rs,rs);end+=rs;}
    if(found==UINT64_MAX){a04_bp_put(out+end,objects+1,rs);end+=rs;}
    for(uint64_t i=0;i<count;i++){
        uint64_t v=i==found?objects:a04_bp_uint(s+pos+(count+i)*rs,rs);a04_bp_put(out+end,v,rs);end+=rs;
    }
    if(found==UINT64_MAX){a04_bp_put(out+end,objects,rs);end+=rs;}
    uint64_t newtable=end;
    for(uint64_t i=0;i<objects;i++){a04_bp_put(out+end,a04_bp_uint(s+table+i*os,os),8);end+=8;}
    a04_bp_put(out+end,valueat,8);end+=8;
    if(found==UINT64_MAX){a04_bp_put(out+end,keyat,8);end+=8;}
    a04_bp_put(out+end,newroot,8);end+=8;
    memset(out+end,0,32);out[end+6]=8;out[end+7]=(unsigned char)rs;
    a04_bp_put(out+end+8,nobjects,8);a04_bp_put(out+end+16,nobjects-1,8);a04_bp_put(out+end+24,newtable,8);end+=32;
    *written=end;return 0;
}
#endif
