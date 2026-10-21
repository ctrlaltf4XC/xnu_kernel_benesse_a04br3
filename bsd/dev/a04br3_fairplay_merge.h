/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Pre-launch-only transaction for an existing native disabled.plist.
 * Save original bytes, atomically replace, and restore only if nobody else
 * has changed the generated configuration. Never drop unrelated overrides.
 */
#ifndef A04BR3_FAIRPLAY_MERGE_H
#define A04BR3_FAIRPLAY_MERGE_H
#include "a04br3_bplist.h"
#define A04_FP_DIR "/private/var/db/com.apple.xpc.launchd/"
static int fp_remove(vnode_t vp,const char *leaf,vfs_context_t ctx)
{
    vnode_t parent=NULLVP;
    int error=vnode_lookup(A04_FP_DIR,VNODE_LOOKUP_NOFOLLOW,&parent,ctx);
    if(error)return error;
    if(!vnode_isdir(parent) || vnode_mount(parent)!=vnode_mount(rootvnode)){
        vnode_put(parent);return ENOTDIR;
    }
    struct componentname cn;bzero(&cn,sizeof(cn));
    cn.cn_nameiop=DELETE;cn.cn_flags=ISLASTCN;cn.cn_context=ctx;
    cn.cn_nameptr=(char *)leaf;cn.cn_namelen=(int)strlen(leaf);
    error=VNOP_REMOVE(parent,vp,&cn,0,ctx);vnode_put(parent);return error;
}
static int fp_read(vnode_t vp,unsigned char *data,size_t cap,size_t *len,
    struct vnode_attr *va,vfs_context_t ctx)
{
    VATTR_INIT(va);VATTR_WANTED(va,va_uid);VATTR_WANTED(va,va_gid);
    VATTR_WANTED(va,va_nlink);VATTR_WANTED(va,va_data_size);VATTR_WANTED(va,va_mode);
    int error=vnode_getattr(vp,va,ctx);
    if(error)return error;
    if(!vnode_isreg(vp) || vnode_mount(vp)!=vnode_mount(rootvnode) ||
        !VATTR_IS_SUPPORTED(va,va_uid) || va->va_uid ||
        !VATTR_IS_SUPPORTED(va,va_gid) || !VATTR_IS_SUPPORTED(va,va_mode) ||
        !VATTR_IS_SUPPORTED(va,va_nlink) || va->va_nlink!=1 ||
        !VATTR_IS_SUPPORTED(va,va_data_size) || va->va_data_size>cap)return EEXIST;
    *len=(size_t)va->va_data_size;int resid=0;
    error=vn_rdwr(UIO_READ,vp,(caddr_t)data,(int)*len,0,UIO_SYSSPACE,0,
        vfs_context_ucred(ctx),&resid,vfs_context_proc(ctx));
    return error?error:resid?EIO:0;
}
static int fp_create(const char *path,const char *leaf,const unsigned char *data,size_t len,
    int mode,int gid,vnode_t *result,vfs_context_t ctx)
{
    vnode_t vp=NULLVP;
    int error=vnode_open(path,FWRITE|O_CREAT|O_EXCL|O_NOFOLLOW,mode,0,&vp,ctx);
    if(error)return error;
    struct vnode_attr va;VATTR_INIT(&va);VATTR_SET(&va,va_uid,0);
    VATTR_SET(&va,va_gid,gid);VATTR_SET(&va,va_mode,mode);
    error=vnode_setattr(vp,&va,ctx);
    int resid=0;
    if(!error)error=vn_rdwr(UIO_WRITE,vp,(caddr_t)data,(int)len,0,UIO_SYSSPACE,IO_SYNC,
        vfs_context_ucred(ctx),&resid,vfs_context_proc(ctx));
    if(!error && resid)error=EIO;
    int closed=vnode_close(vp,FWRITE,ctx);if(!error)error=closed;
    if(error){int cleanup=fp_remove(vp,leaf,ctx);if(cleanup)printf("A04BR3 ROOT: FairPlay cleanup error=%d\n",cleanup);vnode_put(vp);}
    else *result=vp;
    return error;
}
static int merge_existing_fairplay(vnode_t current,unsigned restore,vfs_context_t ctx)
{
    unsigned char *live=kalloc_data(A04_FP_MAX_OUTPUT,Z_WAITOK);
    unsigned char *original=kalloc_data(A04_FP_MAX_INPUT,Z_WAITOK);
    unsigned char *merged=kalloc_data(A04_FP_MAX_OUTPUT,Z_WAITOK);
    vnode_t backup=NULLVP,temp=NULLVP;
    size_t live_len=0,original_len=0,merged_len=0;
    struct vnode_attr live_va,backup_va;
    const char *stage="allocate";
    int error=ENOMEM;
    if(!live || !original || !merged)goto out;
    stage="read active policy";
    error=fp_read(current,live,A04_FP_MAX_OUTPUT,&live_len,&live_va,ctx);
    if(error)goto out;
    stage="lookup original policy";
    error=vnode_lookup(A04_FP_DIR "disabled.a04br3.original",VNODE_LOOKUP_NOFOLLOW,&backup,ctx);
    if(error==ENOENT){
        error=0;
        if(restore)goto out; /* No transaction owned by us to undo. */
        if(live_len>A04_FP_MAX_INPUT){error=EFBIG;goto out;}
        stage="parse active policy";
        error=a04_bp_disable_fp(live,live_len,merged,A04_FP_MAX_OUTPUT,&merged_len);
        if(error){printf("A04BR3 ROOT: FairPlay policy parse error=%d bytes=%lu header=%02x%02x%02x%02x%02x%02x%02x%02x\n",error,(unsigned long)live_len,live_len>0?live[0]:0,live_len>1?live[1]:0,live_len>2?live[2]:0,live_len>3?live[3]:0,live_len>4?live[4]:0,live_len>5?live[5]:0,live_len>6?live[6]:0,live_len>7?live[7]:0);goto out;}
        if(live_len==merged_len && !memcmp(live,merged,live_len))goto out;
        memcpy(original,live,live_len);original_len=live_len;
        stage="save original policy";
        error=fp_create(A04_FP_DIR "disabled.a04br3.original","disabled.a04br3.original",
            original,original_len,0600,0,&backup,ctx);
        if(error)goto out;
    }else{
        if(error)goto out;
        stage="read original policy";
        error=fp_read(backup,original,A04_FP_MAX_INPUT,&original_len,&backup_va,ctx);
        if(error)goto out;
        stage="parse original policy";
        error=a04_bp_disable_fp(original,original_len,merged,A04_FP_MAX_OUTPUT,&merged_len);
        if(error)goto out;
        int is_original=live_len==original_len && !memcmp(live,original,live_len);
        int is_merged=live_len==merged_len && !memcmp(live,merged,live_len);
        if(!is_original && !is_merged){error=EBUSY;goto out;}
        if(restore && is_original){error=fp_remove(backup,"disabled.a04br3.original",ctx);goto out;}
        if(!restore && is_merged)goto out;
    }
    stage="write pending policy";
    error=fp_create(A04_FP_DIR "disabled.a04br3.pending","disabled.a04br3.pending",
        restore?original:merged,restore?original_len:merged_len,
        live_va.va_mode&0777,live_va.va_gid,&temp,ctx);
    if(error)goto out;
    extern int a04br3_fairplay_override_commit(void);
    stage="commit pending policy";
    error=a04br3_fairplay_override_commit();
    if(error){int cleanup=fp_remove(temp,"disabled.a04br3.pending",ctx);if(cleanup)printf("A04BR3 ROOT: FairPlay pending cleanup error=%d\n",cleanup);}
    else if(restore)error=fp_remove(backup,"disabled.a04br3.original",ctx);
    if(!error)printf("A04BR3 ROOT: FairPlay native overrides %s (%lu -> %lu bytes)\n",
        restore?"restored":"merged",(unsigned long)live_len,
        (unsigned long)(restore?original_len:merged_len));
out:
    if(error)printf("A04BR3 ROOT: FairPlay transaction stage=%s error=%d\n",stage,error);
    if(temp!=NULLVP)vnode_put(temp);
    if(backup!=NULLVP)vnode_put(backup);
    if(live)kfree_data(live,A04_FP_MAX_OUTPUT);
    if(original)kfree_data(original,A04_FP_MAX_INPUT);
    if(merged)kfree_data(merged,A04_FP_MAX_OUTPUT);
    return error;
}
#endif
