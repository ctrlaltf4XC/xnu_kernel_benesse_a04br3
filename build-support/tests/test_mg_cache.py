# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Execute actual identity parser and pre-launch cache transaction with VFS mocks."""
from pathlib import Path
import plistlib,struct,subprocess,tempfile,unittest

INC=Path(__file__).resolve().parents[2]/'bsd/dev'

def fixture(device='iPod',product='Benesse TAB-A04-BR3',number=2):
    packed=bytearray(0x14e2);struct.pack_into('<i',packed,93*8,number);packed[0x1290+93]=1
    return plistlib.dumps({'CacheVersion':'20H392','CacheUUID':'42F6FA57-C5A1-4134-9BF4-AB00F962EAE8','CacheData':bytes(packed),'CacheExtra':{'+3Uf0Pm5F8Xy7Onyvko0vA':device,'h9jDsbgj7xIVeIQ8S3/X3Q':product,'unrelated':{'preserved':[True,b'abc',42]}}},fmt=plistlib.FMT_BINARY)

HARNESS=r'''
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#define NULLVP NULL
#define VNODE_LOOKUP_NOFOLLOW 1
#define DELETE 2
#define ISLASTCN 1
#define FWRITE 2
#define O_CREAT 4
#define O_EXCL 8
#define O_NOFOLLOW 16
#define UIO_READ 0
#define UIO_WRITE 1
#define UIO_SYSSPACE 0
#define IO_SYNC 1
#define Z_WAITOK 0
#define bzero(p,n) memset(p,0,n)
#define kalloc_data(n,f) malloc(n)
#define kfree_data(p,n) free(p)
typedef int boolean_t;
struct vnode {int reg,dir,mount,uid,nlink;size_t size;unsigned char data[65536];};
typedef struct vnode *vnode_t;typedef void *vfs_context_t;
static struct vnode root={.dir=1,.mount=1},inodes[4];static vnode_t rootvnode=&root;
static int active,backup,stampidx,refs[5],moves,closes,fail_move,fail_restore,fail_open,fail_write,fail_close,short_read,bad_product;
struct vnode_attr {int va_uid,va_nlink;size_t va_data_size;};
#define VATTR_INIT(p) memset(p,0,sizeof(*(p)))
#define VATTR_WANTED(p,k) ((void)(p))
#define VATTR_IS_SUPPORTED(p,k) 1
struct componentname {int cn_nameiop,cn_flags;void *cn_context;char *cn_nameptr;int cn_namelen;};
static int ix(vnode_t v){return v==&root?4:(int)(v-inodes);}
static int pathidx(const char *p){return strstr(p,"pre-ipad-v1")?backup:strstr(p,"ipad-v1.stamp")?stampidx:active;}
static int vnode_lookup(const char *p,int flags,vnode_t *v,vfs_context_t c){(void)c;assert(flags==VNODE_LOOKUP_NOFOLLOW);if(p[strlen(p)-1]=='/'){*v=&root;refs[4]++;return 0;}int n=pathidx(p);if(n<0){*v=NULL;return ENOENT;}*v=&inodes[n];refs[n]++;return 0;}
static void vnode_put(vnode_t v){assert(v && refs[ix(v)]>0);refs[ix(v)]--;}
static int vnode_isdir(vnode_t v){return v->dir;}static int vnode_isreg(vnode_t v){return v->reg;}static int vnode_mount(vnode_t v){return v->mount;}
static vfs_context_t vfs_context_kernel(void){return NULL;}static void *vfs_context_ucred(vfs_context_t c){return c;}static void *vfs_context_proc(vfs_context_t c){return c;}
static int vnode_getattr(vnode_t v,struct vnode_attr *a,vfs_context_t c){(void)c;a->va_uid=v->uid;a->va_nlink=v->nlink;a->va_data_size=v->size;return 0;}
static int vnode_open(const char *p,int flags,int mode,int unused,vnode_t *v,vfs_context_t c){(void)c;assert(strstr(p,"ipad-v1.stamp") && flags==(FWRITE|O_CREAT|O_EXCL|O_NOFOLLOW) && mode==0600 && !unused);if(fail_open)return fail_open;if(stampidx>=0)return EEXIST;stampidx=2;inodes[2].reg=inodes[2].mount=inodes[2].nlink=1;*v=&inodes[2];refs[2]++;return 0;}
static int vn_close(vnode_t v,int flags,vfs_context_t c){(void)c;assert(refs[ix(v)]>0 && flags==FWRITE);closes++;return fail_close;}
static int VNOP_REMOVE(vnode_t p,vnode_t v,struct componentname *cn,int flags,vfs_context_t c){(void)c;assert(p==&root && v==&inodes[2] && cn->cn_nameiop==DELETE && !flags);assert(!strcmp(cn->cn_nameptr,"com.apple.MobileGestalt.a04br3-ipad-v1.stamp"));stampidx=-1;return 0;}
static int vn_rdwr(int rw,vnode_t v,caddr_t data,int n,off_t off,int seg,int flags,void *cred,int *resid,void *proc){(void)cred;(void)proc;assert(!off && seg==UIO_SYSSPACE && n>=0 && n<=65536);*resid=0;if(rw==UIO_READ){assert(!flags && (size_t)n==v->size);memcpy(data,v->data,n);if(short_read)*resid=1;return 0;}assert(v==&inodes[2] && flags==IO_SYNC);if(fail_write)return fail_write;memcpy(v->data,data,n);v->size=n;return 0;}
boolean_t PEGetProductName(char *p,int n){snprintf(p,n,"%s",bad_product?"Benesse TAB-A04-BR3":"iPad-A04BR3");return 1;}
boolean_t PEGetTargetName(char *p,int n){snprintf(p,n,"a04br3");return 1;}
int a04br3_mg_cache_move(unsigned restore){moves++;if(restore){if(fail_restore)return fail_restore;if(active>=0)return EEXIST;assert(backup>=0);active=backup;backup=-1;}else{if(fail_move)return fail_move;if(backup>=0)return EEXIST;assert(active>=0);backup=active;active=-1;}return 0;}
#include "a04br3_mg_cache.h"
static unsigned char old[65536],good[65536];static size_t oldsize,goodsize;
static void checkrefs(void){for(int i=0;i<5;i++)assert(!refs[i]);}
static void reset(void){checkrefs();memset(inodes,0,sizeof(inodes));active=0;backup=stampidx=-1;moves=closes=fail_move=fail_restore=fail_open=fail_write=fail_close=short_read=bad_product=0;inodes[0].reg=inodes[0].mount=inodes[0].nlink=1;inodes[0].uid=501;inodes[0].size=oldsize;memcpy(inodes[0].data,old,oldsize);}
static size_t load(const char *p,unsigned char *b){FILE *f=fopen(p,"rb");assert(f);size_t n=fread(b,1,65536,f);fclose(f);return n;}
int main(int argc,char **argv){assert(argc>=4);oldsize=load(argv[1],old);goodsize=load(argv[2],good);reset();int stale=0;
 assert(a04_mg_cache_identity(old,oldsize,&stale)==0 && stale==1);
 assert(a04_mg_cache_identity(good,goodsize,&stale)==0 && stale==0);
 for(size_t n=0;n<oldsize;n++)assert(a04_mg_cache_identity(old,n,&stale)!=0);
 assert(prepare_mobilegestalt()==0 && active==-1 && backup==0 && stampidx==2 && moves==1 && closes==1);checkrefs();assert(inodes[0].size==oldsize && !memcmp(inodes[0].data,old,oldsize));
 assert(prepare_mobilegestalt()==0 && moves==1);checkrefs();
 active=3;inodes[3]=inodes[0];inodes[3].size=goodsize;memcpy(inodes[3].data,good,goodsize);
 assert(prepare_mobilegestalt()==0 && moves==1 && backup==0);checkrefs();
 reset();bad_product=1;assert(!prepare_mobilegestalt() && !moves);checkrefs();
 reset();backup=1;assert(prepare_mobilegestalt()==EEXIST && !moves && active==0 && backup==1);checkrefs();
 reset();stampidx=2;assert(prepare_mobilegestalt()==EEXIST && !moves);checkrefs();
 reset();fail_move=EROFS;assert(prepare_mobilegestalt()==EROFS && active==0 && backup==-1);checkrefs();
 reset();fail_open=EACCES;assert(prepare_mobilegestalt()==EACCES && active==0 && backup==-1 && moves==2 && !closes);checkrefs();
 reset();fail_write=EIO;assert(prepare_mobilegestalt()==EIO && active==0 && backup==-1 && stampidx==-1 && closes==1);checkrefs();
 reset();fail_close=EIO;assert(prepare_mobilegestalt()==EIO && active==0 && backup==-1 && stampidx==-1 && closes==1);checkrefs();
 reset();fail_write=EIO;fail_restore=EROFS;assert(prepare_mobilegestalt()==EIO && active==-1 && backup==0);checkrefs();assert(!memcmp(inodes[backup].data,old,oldsize));
 reset();short_read=1;assert(prepare_mobilegestalt()==EIO && !moves);checkrefs();
 reset();inodes[0].uid=502;assert(prepare_mobilegestalt()==EINVAL && !moves);checkrefs();
 reset();inodes[0].nlink=2;assert(prepare_mobilegestalt()==EINVAL && !moves);checkrefs();
 reset();inodes[0].mount=2;assert(prepare_mobilegestalt()==EINVAL && !moves);checkrefs();
 reset();inodes[0].reg=0;assert(prepare_mobilegestalt()==EINVAL && !moves);checkrefs();
 reset();inodes[0].data[0]='x';assert(prepare_mobilegestalt()==ENOTSUP && !moves);checkrefs();
 reset();inodes[0].size=load(argv[3],inodes[0].data);assert(prepare_mobilegestalt()==0 && active==-1 && backup==0 && stampidx==2);checkrefs();
 for(int a=4;a<argc;a++){reset();inodes[0].size=load(argv[a],inodes[0].data);assert(prepare_mobilegestalt()!=0 && !moves);checkrefs();}
 puts("MG transaction and parser host cases passed; no Apple runtime executed");return 0;}
'''

class MGCache(unittest.TestCase):
    def test_parser_and_transaction(self):
        with tempfile.TemporaryDirectory() as tmp:
            d=Path(tmp);(d/'case.c').write_text(HARNESS);(d/'old').write_bytes(fixture());(d/'good').write_bytes(fixture('iPad','iPad-A04BR3',3));(d/'current-product-stale-class').write_bytes(fixture('iPod','iPad-A04BR3',2))
            subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I',str(INC),str(d/'case.c'),'-o',str(d/'case')],check=True)
            bad=[]
            for i,data in enumerate([fixture('iPod','unknown',2),fixture('iPad','iPad-A04BR3',2),plistlib.dumps({'CacheExtra':{}}),b'not a plist']):
                path=d/f'bad-{i}';path.write_bytes(data);bad.append(str(path))
            subprocess.run([str(d/'case'),str(d/'old'),str(d/'good'),str(d/'current-product-stale-class')]+bad,check=True)

    def test_fixed_path_exclusive_rename(self):
        source=(INC.parent/'vfs/vfs_syscalls.c').read_text();fn=source[source.index('int a04br3_mg_cache_move(unsigned restore)\n{'):source.index('int a04br3_fairplay_override_commit(void);')]
        self.assertIn('RENAME_EXCL | RENAME_NOFOLLOW_ANY',fn)
        self.assertNotIn('unlink',fn)

if __name__=='__main__':unittest.main()
