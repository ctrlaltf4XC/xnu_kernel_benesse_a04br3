/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Pre-launch-only MG cache rotation for the known A04BR3 product transition.
 * Backup is also a durable replay barrier if power fails before the stamp.
 */
#ifndef A04BR3_MG_CACHE_H
#define A04BR3_MG_CACHE_H
#include "a04br3_mg_identity.h"
#define A04_MG_DIR "/private/var/containers/Shared/SystemGroup/systemgroup.com.apple.mobilegestaltcache/Library/Caches/"
#define A04_MG_ACTIVE A04_MG_DIR "com.apple.MobileGestalt.plist"
#define A04_MG_BACKUP A04_MG_DIR "com.apple.MobileGestalt.a04br3-pre-ipad-v1.plist"
#define A04_MG_STAMP_LEAF "com.apple.MobileGestalt.a04br3-ipad-v1.stamp"
#define A04_MG_STAMP A04_MG_DIR A04_MG_STAMP_LEAF
static const char mg_stamp[]="a04br3 MG identity transition v1\nBenesse TAB-A04-BR3 -> iPad-A04BR3\n";
/* Fixed-path RENAME_EXCL | RENAME_NOFOLLOW_ANY in vfs_syscalls.c. */
extern int a04br3_mg_cache_move(unsigned restore);
extern boolean_t PEGetProductName(char *,int);
extern boolean_t PEGetTargetName(char *,int);
static int mg_absent(const char *path,vfs_context_t ctx)
{
    vnode_t vp=NULLVP;int error=vnode_lookup(path,VNODE_LOOKUP_NOFOLLOW,&vp,ctx);
    if(!error){vnode_put(vp);return EEXIST;}
    return error==ENOENT?0:error;
}
static int mg_remove_stamp(vnode_t vp,vfs_context_t ctx)
{
    vnode_t parent=NULLVP;int error=vnode_lookup(A04_MG_DIR,VNODE_LOOKUP_NOFOLLOW,&parent,ctx);
    if(error)return error;
    if(!vnode_isdir(parent) || vnode_mount(parent)!=vnode_mount(rootvnode)){vnode_put(parent);return ENOTDIR;}
    struct componentname cn;bzero(&cn,sizeof(cn));cn.cn_nameiop=DELETE;cn.cn_flags=ISLASTCN;
    cn.cn_context=ctx;cn.cn_nameptr=(char *)A04_MG_STAMP_LEAF;cn.cn_namelen=sizeof(A04_MG_STAMP_LEAF)-1;
    error=VNOP_REMOVE(parent,vp,&cn,0,ctx);vnode_put(parent);return error;
}
static int prepare_mobilegestalt(void)
{
    char product[64]={0},target[64]={0};
    if(!PEGetProductName(product,sizeof(product)) || !PEGetTargetName(target,sizeof(target)))return ENXIO;
    if(strcmp(product,"iPad-A04BR3") || strcmp(target,"a04br3"))return 0;
    printf("A04BR3 MG: identity product=%s target=%s\n",product,target);
    const char *stage="lookup canonical cache";
    vfs_context_t ctx=vfs_context_kernel();vnode_t vp=NULLVP,stamp=NULLVP;
    unsigned char *data=NULL;size_t size=0;int stale=0,resid=0;
    int error=vnode_lookup(A04_MG_ACTIVE,VNODE_LOOKUP_NOFOLLOW,&vp,ctx);
    if(error==ENOENT){printf("A04BR3 MG: canonical cache missing; native computation pending\n");return 0;}
    if(error)return error;
    struct vnode_attr va;VATTR_INIT(&va);VATTR_WANTED(&va,va_uid);VATTR_WANTED(&va,va_nlink);VATTR_WANTED(&va,va_data_size);
    error=vnode_getattr(vp,&va,ctx);
    if(!error && (!vnode_isreg(vp) || vnode_mount(vp)!=vnode_mount(rootvnode) ||
        !VATTR_IS_SUPPORTED(&va,va_uid) || (va.va_uid!=501 && va.va_uid!=0) ||
        !VATTR_IS_SUPPORTED(&va,va_nlink) || va.va_nlink!=1 ||
        !VATTR_IS_SUPPORTED(&va,va_data_size) || va.va_data_size>A04_MG_MAX_CACHE))error=EINVAL;
    if(error)goto out;
    size=(size_t)va.va_data_size;data=kalloc_data(A04_MG_MAX_CACHE,Z_WAITOK);
    if(!data){error=ENOMEM;goto out;}
    error=vn_rdwr(UIO_READ,vp,(caddr_t)data,(int)size,0,UIO_SYSSPACE,0,vfs_context_ucred(ctx),&resid,vfs_context_proc(ctx));
    if(!error && resid)error=EIO;
    stage="parse exact previous identity";
    if(!error)error=a04_mg_cache_identity(data,size,&stale);
    if(error || !stale)goto out; /* Correct iPad cache is never rotated again. */
    stage="refuse existing backup";
    error=mg_absent(A04_MG_BACKUP,ctx);if(error)goto out;
    stage="refuse existing stamp";
    error=mg_absent(A04_MG_STAMP,ctx);if(error)goto out;
    stage="rename canonical cache exclusively";
    error=a04br3_mg_cache_move(0);if(error)goto out;
    stage="commit identity stamp";
    error=vnode_open(A04_MG_STAMP,FWRITE|O_CREAT|O_EXCL|O_NOFOLLOW,0600,0,&stamp,ctx);
    if(!error){
        resid=0;
        error=vn_rdwr(UIO_WRITE,stamp,(caddr_t)mg_stamp,sizeof(mg_stamp)-1,0,UIO_SYSSPACE,IO_SYNC,vfs_context_ucred(ctx),&resid,vfs_context_proc(ctx));
        if(!error && resid)error=EIO;
        /* vn_close leaves the caller's iocount; vnode_put below releases it. */
        int closed=vn_close(stamp,FWRITE,ctx);if(!error)error=closed;
        if(error){int cleanup=mg_remove_stamp(stamp,ctx);if(cleanup)printf("A04BR3 MG: stamp cleanup error=%d\n",cleanup);}
    }
    if(error){
        int rollback=a04br3_mg_cache_move(1);
        printf("A04BR3 MG: transaction failed=%d rollback=%d (backup retained if rollback fails)\n",error,rollback);
    }else printf("A04BR3 MG: stale iPod/2 cache moved to preserved backup; native recomputation pending, NOT verified iPad\n");
out:
    if(error)printf("A04BR3 MG: stage=%s error=%d bytes=%lu stale=%d\n",stage,error,(unsigned long)size,stale);
    else if(!stale)printf("A04BR3 MG: cache already matches iPad identity; unchanged\n");
    if(stamp!=NULLVP)vnode_put(stamp);
    if(vp!=NULLVP)vnode_put(vp);
    if(data)kfree_data(data,A04_MG_MAX_CACHE);
    return error;
}
#endif
