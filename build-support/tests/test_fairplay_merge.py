# SPDX-License-Identifier: MIT
"""Execute the actual backup/atomic replacement/restore VFS transaction."""
from pathlib import Path
import plistlib,subprocess,tempfile,unittest

class MergeTransaction(unittest.TestCase):
 def test_transaction(self):
  inc=Path(__file__).resolve().parents[2]/'bsd/dev'
  harness=r'''
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
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
struct vnode {int exists,reg,dir,mount,uid,gid,mode,nlink;size_t size;unsigned char bytes[32768];};
typedef struct vnode *vnode_t;
typedef void *vfs_context_t;
static struct vnode root={.exists=1,.dir=1,.mount=1},nodes[3];
static vnode_t rootvnode=&root;
static int writes,fail_write,rename_error;
struct vnode_attr {int va_uid,va_gid,va_mode,va_nlink;size_t va_data_size;};
#define VATTR_INIT(p) memset(p,0,sizeof(*(p)))
#define VATTR_WANTED(p,k) ((void)(p))
#define VATTR_SET(p,k,v) ((p)->k=(v))
#define VATTR_IS_SUPPORTED(p,k) 1
struct componentname {int cn_nameiop,cn_flags;void *cn_context;char *cn_nameptr;int cn_namelen;};
static int node_index(const char *p) {return strstr(p,".original")?1:strstr(p,".pending")?2:0;}
static int vnode_lookup(const char *p,int f,vnode_t *v,vfs_context_t c) {
 (void)c;assert(f==VNODE_LOOKUP_NOFOLLOW);if(p[strlen(p)-1]=='/') {*v=&root;return 0;}*v=&nodes[node_index(p)];return (*v)->exists?0:ENOENT;
}
static int vnode_isreg(vnode_t v) {return v->reg;}
static int vnode_isdir(vnode_t v) {return v->dir;}
static int vnode_mount(vnode_t v) {return v->mount;}
static void vnode_put(vnode_t v) {assert(v);}
static void *vfs_context_ucred(vfs_context_t c) {return c;}
static void *vfs_context_proc(vfs_context_t c) {return c;}
static int vnode_getattr(vnode_t v,struct vnode_attr *a,vfs_context_t c) {
 (void)c;a->va_uid=v->uid;a->va_gid=v->gid;a->va_mode=v->mode;a->va_nlink=v->nlink;a->va_data_size=v->size;return 0;
}
static int vnode_setattr(vnode_t v,struct vnode_attr *a,vfs_context_t c) {(void)c;v->uid=a->va_uid;v->gid=a->va_gid;v->mode=a->va_mode;return 0;}
static int vnode_open(const char *p,int f,int mode,int flags,vnode_t *v,vfs_context_t c) {
 (void)c;assert(f==(FWRITE|O_CREAT|O_EXCL|O_NOFOLLOW) && !flags);*v=&nodes[node_index(p)];if((*v)->exists)return EEXIST;memset(*v,0,sizeof(**v));(*v)->exists=(*v)->reg=(*v)->mount=(*v)->nlink=1;(*v)->mode=mode;return 0;
}
static int vnode_close(vnode_t v,int f,vfs_context_t c) {(void)c;assert(v && f==FWRITE);return 0;}
static int VNOP_REMOVE(vnode_t p,vnode_t v,struct componentname *n,int f,vfs_context_t c) {
 (void)c;assert(p==&root && !f && n->cn_nameiop==DELETE && n->cn_flags==ISLASTCN);assert(n->cn_namelen==(int)strlen(n->cn_nameptr));assert(v==&nodes[node_index(n->cn_nameptr)]);v->exists=0;return 0;
}
static int vn_rdwr(int rw,vnode_t v,caddr_t data,int len,off_t offset,int seg,int flags,void *cred,int *resid,void *proc) {
 (void)cred;(void)proc;assert(!offset && seg==UIO_SYSSPACE && len>=0 && len<=32768);*resid=0;
 if(rw==UIO_READ) {assert((size_t)len==v->size);memcpy(data,v->bytes,len);}
 else {assert(flags==IO_SYNC);writes++;if(writes==fail_write)return EIO;memcpy(v->bytes,data,len);v->size=len;}
 return 0;
}
int a04br3_fairplay_override_commit(void) {if(rename_error)return rename_error;assert(nodes[2].exists);nodes[0]=nodes[2];nodes[2].exists=0;return 0;}
#include "a04br3_fairplay_merge.h"
static unsigned char baseline[16384];static size_t baseline_size;
static void reset(void) {memset(nodes,0,sizeof(nodes));nodes[0].exists=nodes[0].reg=nodes[0].mount=nodes[0].nlink=1;nodes[0].gid=20;nodes[0].mode=0640;nodes[0].size=baseline_size;memcpy(nodes[0].bytes,baseline,baseline_size);writes=fail_write=rename_error=0;}
int main(int argc,char **argv) {
 assert(argc==3);FILE *f=fopen(argv[1],"rb");assert(f);baseline_size=fread(baseline,1,sizeof(baseline),f);fclose(f);reset();
 assert(merge_existing_fairplay(&nodes[0],1,NULL)==0 && !writes);
 nodes[0].uid=501;assert(merge_existing_fairplay(&nodes[0],0,NULL)==EEXIST && !writes);reset();
 nodes[0].nlink=2;assert(merge_existing_fairplay(&nodes[0],0,NULL)==EEXIST && !writes);reset();
 fail_write=1;assert(merge_existing_fairplay(&nodes[0],0,NULL)==EIO && !nodes[1].exists && !nodes[2].exists);assert(!memcmp(nodes[0].bytes,baseline,baseline_size));reset();
 fail_write=2;assert(merge_existing_fairplay(&nodes[0],0,NULL)==EIO && nodes[1].exists && !nodes[2].exists);assert(!memcmp(nodes[0].bytes,baseline,baseline_size));fail_write=0;
 assert(merge_existing_fairplay(&nodes[0],0,NULL)==0);assert(nodes[0].mode==0640 && nodes[0].gid==20);
 f=fopen(argv[2],"wb");assert(f);assert(fwrite(nodes[0].bytes,1,nodes[0].size,f)==nodes[0].size);fclose(f);
 int before=writes;assert(merge_existing_fairplay(&nodes[0],0,NULL)==0 && writes==before);
 nodes[0].bytes[0]='X';assert(merge_existing_fairplay(&nodes[0],1,NULL)==EBUSY && writes==before);nodes[0].bytes[0]='b';
 assert(merge_existing_fairplay(&nodes[0],1,NULL)==0 && !nodes[1].exists && nodes[0].size==baseline_size && !memcmp(nodes[0].bytes,baseline,baseline_size));
 reset();rename_error=EIO;assert(merge_existing_fairplay(&nodes[0],0,NULL)==EIO && !nodes[2].exists && nodes[1].exists && !memcmp(nodes[0].bytes,baseline,baseline_size));rename_error=0;assert(merge_existing_fairplay(&nodes[0],0,NULL)==0);
 return 0;
}
'''
  with tempfile.TemporaryDirectory() as tmp:
   d=Path(tmp);src=d/'case.c';exe=d/'case';original=d/'original';result=d/'result';src.write_text(harness)
   data={'com.apple.some-disabled-job':True,'com.apple.some-enabled-job':False,'opaque-data':{'array':[b'abc',5,'\u65e5']}}
   original.write_bytes(plistlib.dumps(data,fmt=plistlib.FMT_BINARY))
   subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(inc),str(src),'-o',str(exe)],check=True)
   subprocess.run([str(exe),str(original),str(result)],check=True)
   data['com.apple.fairplayd.H2']=True;self.assertEqual(plistlib.loads(result.read_bytes()),data)
