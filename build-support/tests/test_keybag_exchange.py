# SPDX-License-Identifier: MIT
"""Execute the actual development keybag fallback gate, including refusals."""
from pathlib import Path
import subprocess,tempfile,unittest
class Keybag(unittest.TestCase):
 def test_gate(self):
  s=(Path(__file__).resolve().parents[2]/'bsd/a04br3_hfs/core/hfs_vnops.c').read_text()
  gate=s[s.index('static int\na04br3_keybag_swap'):s.index('\n#endif',s.index('static int\na04br3_keybag_swap'))]
  harness=r"""
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
struct desc {int cd_namelen;const char *cd_nameptr;};
struct cnode {struct desc c_desc;int c_uid,c_gid,c_linkcount,c_mode;};
struct vnode {int reg;struct cnode c;};
struct componentname {int cn_namelen;const char *cn_nameptr;};
struct vnop_renamex_args {struct vnode *a_fvp,*a_tvp,*a_fdvp,*a_tdvp;struct componentname *a_tcnp;void *a_context;};
struct vnop_exchange_args {void *a_desc;struct vnode *a_fvp,*a_tvp;int a_options;void *a_context;};
static int vnop_exchange_desc,enabled=1,exchanges,failure;
static const char *caller="usermanagerd";
#define VTOC(v) (&(v)->c)
static void PE_parse_boot_argn(const char *n,unsigned *v,unsigned z) {(void)n;(void)z;*v=enabled;}
static int vnode_isreg(struct vnode *v) {return v->reg;}
static void *vfs_context_proc(void *ctx) {return ctx;}
static int proc_pid(void *p) {(void)p;return 1;}
static void proc_name(int pid,char *b,unsigned n) {(void)pid;snprintf(b,n,"%s",caller);}
static int hfs_vnop_exchange(struct vnop_exchange_args *a) {assert(a->a_options==0 && a->a_desc==&vnop_exchange_desc);exchanges++;return failure;}
"""+gate+r"""
int main(void) {
 struct vnode parent={0,{{7,"keybags"},0,0,1,0700}},from={1,{{0,NULL},0,0,1,0600}},to=from,other=parent;
 struct componentname target={10,"persona.kb"};
 struct vnop_renamex_args a={&from,&to,&parent,&parent,&target,NULL};
 enabled=0;assert(a04br3_keybag_swap(&a)==ENOTSUP && !exchanges);enabled=1;
 caller="other";assert(a04br3_keybag_swap(&a)==ENOTSUP && !exchanges);caller="usermanagerd";
 a.a_tdvp=&other;assert(a04br3_keybag_swap(&a)==ENOTSUP && !exchanges);a.a_tdvp=&parent;
 parent.c.c_desc.cd_nameptr="notbags";assert(a04br3_keybag_swap(&a)==ENOTSUP && !exchanges);parent.c.c_desc.cd_nameptr="keybags";
 target.cn_nameptr="unknown.kb";assert(a04br3_keybag_swap(&a)==ENOTSUP && !exchanges);target.cn_nameptr="persona.kb";
 to.c.c_uid=501;assert(a04br3_keybag_swap(&a)==ENOTSUP && !exchanges);to.c.c_uid=0;
 to.c.c_linkcount=2;assert(a04br3_keybag_swap(&a)==ENOTSUP && !exchanges);to.c.c_linkcount=1;
 to.c.c_mode=0644;assert(a04br3_keybag_swap(&a)==ENOTSUP && !exchanges);to.c.c_mode=0600;
 assert(a04br3_keybag_swap(&a)==0 && exchanges==1);
 target.cn_nameptr="usersession.kb";target.cn_namelen=14;failure=EIO;
 assert(a04br3_keybag_swap(&a)==EIO && exchanges==2);
 a.a_tvp=NULL;assert(a04br3_keybag_swap(&a)==ENOTSUP && exchanges==2);
 return 0;
}
"""
  with tempfile.TemporaryDirectory() as d:
   src=Path(d)/'gate.c';exe=Path(d)/'gate';src.write_text(harness)
   subprocess.run(['cc','-Wall','-Wextra','-Werror',str(src),'-o',str(exe)],check=True)
   subprocess.run([str(exe)],check=True)
