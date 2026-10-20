# SPDX-License-Identifier: MIT
"""Exercise actual restore directory creation with hostile existing entries."""
from pathlib import Path
import subprocess,tempfile,unittest

class RootPrepare(unittest.TestCase):
    def test_existing_and_creation(self):
        s=(Path(__file__).resolve().parents[2]/'bsd/dev/a04br3_root_prepare.c').read_text()
        helper=s[s.index('static int\nprepare_directory'):s.index('\nvoid\na04br3_root_prepare')]
        harness=r"""
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#define NULLVP NULL
#define VNODE_LOOKUP_NOFOLLOW 1
#define CREATE 1
#define ISLASTCN 1
#define VDIR 1
#define VA_EXCLUSIVE 1
struct vnode {int dir,mount;};
typedef struct vnode *vnode_t;
typedef void *vfs_context_t;
static struct vnode root={1,1},child={1,1},parent_node={1,1};
static vnode_t rootvnode=&root;
static int exists=1,parent_error,mkdir_error,writes;
struct componentname {int cn_nameiop,cn_flags;void *cn_context;char *cn_pnbuf;int cn_pnlen;char *cn_nameptr;int cn_namelen;};
struct vnode_attr {int va_uid,va_gid,va_mode,va_type,va_vaflags;};
#define VATTR_INIT(p) memset(p,0,sizeof(*(p)))
#define VATTR_SET(p,k,v) ((p)->k=(v))
#define strlcpy test_strlcpy
static size_t test_strlcpy(char *d,const char *s,size_t n) {size_t l=strlen(s);if(n)snprintf(d,n,"%s",s);return l;}
static vfs_context_t vfs_context_kernel(void) {return NULL;}
static int vnode_lookup(const char *p,int flags,vnode_t *vp,vfs_context_t ctx) {
 (void)ctx;assert(flags==VNODE_LOOKUP_NOFOLLOW);
 if(!strcmp(p,"/private/var/containers")) {if(!exists)return ENOENT;*vp=&child;return 0;}
 if(parent_error)return parent_error;
 *vp=&parent_node;return 0;
}
static int vnode_isdir(vnode_t v) {return v->dir;}
static int vnode_mount(vnode_t v) {return v->mount;}
static void vnode_put(vnode_t v) {assert(v);}
static int VNOP_MKDIR(vnode_t p,vnode_t *vp,struct componentname *cn,struct vnode_attr *va,vfs_context_t ctx) {
 (void)ctx;assert(p==&parent_node);assert(!strcmp(cn->cn_nameptr,"containers"));
 assert(va->va_uid==0 && va->va_gid==0 && va->va_mode==0755 && va->va_type==VDIR && va->va_vaflags==VA_EXCLUSIVE);
 writes++;if(mkdir_error)return mkdir_error;*vp=&child;exists=1;return 0;
}
"""+helper+r"""
int main(void) {
 int made=0;const char *p="/private/var/containers";
 assert(prepare_directory(p,&made)==0 && made==0 && writes==0);
 child.dir=0;assert(prepare_directory(p,&made)==ENOTDIR && writes==0);child.dir=1;
 child.mount=2;assert(prepare_directory(p,&made)==ENOTDIR && writes==0);child.mount=1;
 exists=0;parent_error=EACCES;assert(prepare_directory(p,&made)==EACCES && writes==0);parent_error=0;
 parent_node.mount=2;assert(prepare_directory(p,&made)==ENOTDIR && writes==0);parent_node.mount=1;
 mkdir_error=EROFS;assert(prepare_directory(p,&made)==EROFS && made==0 && writes==1);mkdir_error=0;
 assert(prepare_directory(p,&made)==0 && made==1 && writes==2);
 made=0;assert(prepare_directory(p,&made)==0 && made==0 && writes==2);
 return 0;
}
"""
        with tempfile.TemporaryDirectory() as d:
            src=Path(d)/'root.c';exe=Path(d)/'root';src.write_text(harness)
            subprocess.run(['cc','-Wall','-Wextra','-Werror',str(src),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
