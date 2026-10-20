# SPDX-License-Identifier: MIT
"""Run the actual pre-launch helper against hostile VFS entries and I/O errors."""
from pathlib import Path
import subprocess,tempfile,unittest,plistlib

class FairplayOverride(unittest.TestCase):
 def test_vfs_contract_and_native_plist(self):
  source=(Path(__file__).resolve().parents[2]/'bsd/dev/a04br3_root_prepare.c').read_text()
  helper=source[source.index('static const char fp_override'):source.index('static int prepare_audio')]
  harness=r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
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
#define bzero(p,n) memset(p,0,n)
struct vnode {int reg,dir,mount;};
typedef struct vnode *vnode_t;
typedef void *vfs_context_t;
static struct vnode root={0,1,1},parent={0,1,1},file={1,0,1};
static vnode_t rootvnode=&root;
static int exists,opens,removes,uid,nlink=1,write_error,short_write,close_error,read_error;
static size_t stored;
static char bytes[4096];
struct vnode_attr {int va_uid,va_nlink;size_t va_data_size;};
#define VATTR_INIT(p) memset(p,0,sizeof(*(p)))
#define VATTR_WANTED(p,k) ((void)(p))
#define VATTR_IS_SUPPORTED(p,k) 1
struct componentname {int cn_nameiop,cn_flags;void *cn_context;char *cn_nameptr;int cn_namelen;};
static vfs_context_t vfs_context_kernel(void) {return NULL;}
static void *vfs_context_ucred(vfs_context_t c) {return c;}
static void *vfs_context_proc(vfs_context_t c) {return c;}
static int prepare_directory(const char *p,int *made) {(void)p;*made=0;return 0;}
static int vnode_lookup(const char *p,int flags,vnode_t *v,vfs_context_t c) {
 (void)c;assert(flags==VNODE_LOOKUP_NOFOLLOW);
 if(strstr(p,"disabled.plist")) {if(!exists)return ENOENT;*v=&file;} else *v=&parent;
 return 0;
}
static int vnode_isreg(vnode_t v) {return v->reg;}
static int vnode_isdir(vnode_t v) {return v->dir;}
static int vnode_mount(vnode_t v) {return v->mount;}
static void vnode_put(vnode_t v) {assert(v);}
static int vnode_getattr(vnode_t v,struct vnode_attr *a,vfs_context_t c) {
 (void)c;assert(v==&file);a->va_uid=uid;a->va_nlink=nlink;a->va_data_size=stored;return 0;
}
static int vnode_open(const char *p,int f,int mode,int flags,vnode_t *v,vfs_context_t c) {
 (void)c;assert(strstr(p,"disabled.plist"));assert(f==(FWRITE|O_CREAT|O_EXCL|O_NOFOLLOW));assert(mode==0644 && !flags);assert(!exists);exists=1;opens++;*v=&file;return 0;
}
static int vnode_close(vnode_t v,int f,vfs_context_t c) {(void)c;assert(v==&file && f==FWRITE);return close_error;}
static int vn_rdwr(int rw,vnode_t v,caddr_t data,int len,off_t offset,int seg,int flags,void *cred,int *resid,void *proc) {
 (void)cred;(void)proc;assert(v==&file && !offset && seg==UIO_SYSSPACE);
 *resid=0;
 if(rw==UIO_READ) {if(read_error)return read_error;assert((size_t)len==stored);memcpy(data,bytes,len);}
 else {assert(flags==IO_SYNC);if(write_error)return write_error;stored=len-short_write;memcpy(bytes,data,stored);*resid=short_write;}
 return 0;
}
static int VNOP_REMOVE(vnode_t p,vnode_t v,struct componentname *n,int flags,vfs_context_t c) {
 (void)c;assert(p==&parent && v==&file && !flags && n->cn_nameiop==DELETE && n->cn_flags==ISLASTCN && n->cn_namelen==14 && !strcmp(n->cn_nameptr,"disabled.plist"));exists=0;removes++;return 0;
}
'''+helper+r'''
int main(int argc,char **argv) {
 assert(argc==2);
 assert(prepare_fairplay(1)==0 && !exists && !opens);
 assert(prepare_fairplay(0)==0 && exists && opens==1);
 FILE *f=fopen(argv[1],"wb");assert(f);assert(fwrite(bytes,1,stored,f)==stored);fclose(f);
 assert(prepare_fairplay(0)==0 && opens==1);
 uid=501;assert(prepare_fairplay(1)==EEXIST && exists);uid=0;
 nlink=2;assert(prepare_fairplay(1)==EEXIST && exists);nlink=1;
 file.reg=0;assert(prepare_fairplay(0)==EEXIST && opens==1);file.reg=1;
 file.mount=2;assert(prepare_fairplay(1)==EEXIST && exists);file.mount=1;
 bytes[0]='X';assert(prepare_fairplay(0)==EEXIST && opens==1);bytes[0]='<';
 read_error=EIO;assert(prepare_fairplay(1)==EIO && exists);read_error=0;
 assert(prepare_fairplay(1)==0 && !exists && removes==1);
 write_error=EROFS;assert(prepare_fairplay(0)==EROFS && !exists && removes==2);write_error=0;
 short_write=1;assert(prepare_fairplay(0)==EIO && !exists && removes==3);short_write=0;
 close_error=EIO;assert(prepare_fairplay(0)==EIO && !exists && removes==4);close_error=0;
 assert(prepare_fairplay(0)==0 && exists);
 parent.mount=2;assert(prepare_fairplay(1)==ENOTDIR && exists);parent.mount=1;
 assert(prepare_fairplay(1)==0 && !exists);
 return 0;
}
'''
  with tempfile.TemporaryDirectory() as d:
   d=Path(d);c=d/'case.c';exe=d/'case';out=d/'disabled.plist';c.write_text(harness)
   subprocess.run(['cc','-Wall','-Wextra','-Werror',str(c),'-o',str(exe)],check=True)
   subprocess.run([str(exe),str(out)],check=True)
   self.assertEqual(plistlib.loads(out.read_bytes()),{'com.apple.fairplayd.H2':True})
