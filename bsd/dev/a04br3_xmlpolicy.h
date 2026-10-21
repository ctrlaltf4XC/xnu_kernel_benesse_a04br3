/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Conservative XML launchd policy editor. Preserve every unrelated byte.
 * Only flat boolean dictionaries are accepted; unknown structures fail closed.
 */
#ifndef A04BR3_XMLPOLICY_H
#define A04BR3_XMLPOLICY_H
static size_t fp_xml_find(const unsigned char *s,size_t n,size_t p,const char *v)
{
    size_t k=strlen(v);
    for(;p<=n && k<=n-p;p++)if(!memcmp(s+p,v,k))return p;
    return n;
}
static size_t fp_xml_space(const unsigned char *s,size_t n,size_t p)
{
    while(p<n && (s[p]==' ' || s[p]=='\t' || s[p]=='\n' || s[p]=='\r'))p++;
    return p;
}
static int fp_xml_disable(const unsigned char *s,size_t n,unsigned char *out,size_t cap,size_t *written)
{
    static const char key[]="com.apple.fairplayd.H2";
    static const char add[]="\n<key>com.apple.fairplayd.H2</key><true/>\n";
    if(!n || n>16384 || memchr(s,0,n))return ENOTSUP;
    size_t p=fp_xml_space(s,n,0);
    if(p+5<=n && !memcmp(s+p,"<?xml",5)){
        p=fp_xml_find(s,n,p,"?>");if(p==n)return EINVAL;p=fp_xml_space(s,n,p+2);
    }
    if(p+9<=n && !memcmp(s+p,"<!DOCTYPE",9)){
        size_t e=fp_xml_find(s,n,p,">");if(e==n)return EINVAL;
        /* Reject internal entities/subsets, whose expansion is not modelled. */
        if(memchr(s+p,'[',e-p))return ENOTSUP;
        p=fp_xml_space(s,n,e+1);
    }
    if(p+6>n || memcmp(s+p,"<plist",6))return ENOTSUP;
    p=fp_xml_find(s,n,p,">");if(p==n)return EINVAL;p=fp_xml_space(s,n,p+1);
    size_t insert=0,old_len=0;int found=0,already=0;
    if(p+7<=n && !memcmp(s+p,"<dict/>",7)){
        insert=p;old_len=7;p+=7;
    }else{
        if(p+6>n || memcmp(s+p,"<dict>",6))return ENOTSUP;
        p+=6;
        for(;;){
            p=fp_xml_space(s,n,p);
            if(p+7<=n && !memcmp(s+p,"</dict>",7))break;
            if(p+5>n || memcmp(s+p,"<key>",5))return ENOTSUP;
            size_t k=p+5,e=fp_xml_find(s,n,k,"</key>");if(e==n)return EINVAL;
            if(memchr(s+k,'&',e-k) || memchr(s+k,'<',e-k))return ENOTSUP;
            int target=e-k==sizeof(key)-1 && !memcmp(s+k,key,sizeof(key)-1);
            p=fp_xml_space(s,n,e+6);size_t value=p,len=0;
            if(p+8<=n && !memcmp(s+p,"<false/>",8)){len=8;}
            else if(p+7<=n && !memcmp(s+p,"<false/",7))return EINVAL;
            else if(p+7<=n && !memcmp(s+p,"<true/>",7)){len=7;}
            else return ENOTSUP;
            if(target){if(found)return EINVAL;found=1;insert=value;old_len=len;already=len==7;}
            p+=len;
        }
        if(!found)insert=p;
        p+=7;
    }
    p=fp_xml_space(s,n,p);
    if(p+8>n || memcmp(s+p,"</plist>",8) || fp_xml_space(s,n,p+8)!=n)return EINVAL;
    if(already){if(n>cap)return ENOSPC;memcpy(out,s,n);*written=n;return 0;}
    const char *replacement=found?"<true/>":old_len?"<dict>\n<key>com.apple.fairplayd.H2</key><true/>\n</dict>":add;
    size_t len=strlen(replacement);if(n-old_len>cap || len>cap-(n-old_len))return ENOSPC;
    memcpy(out,s,insert);memcpy(out+insert,replacement,len);
    memcpy(out+insert+len,s+insert+old_len,n-insert-old_len);*written=n-old_len+len;return 0;
}
#endif
