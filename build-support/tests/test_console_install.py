# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Execute actual two-program atomic installer with bounded vnode mocks."""
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[2]
class Installer(unittest.TestCase):
 def test_two_independent_payloads_and_failure_preserves_old_capture(self):
  source=r'''
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#define NULLVP NULL
#define VNODE_LOOKUP_NOFOLLOW 1
#define FWRITE 2
#define O_CREAT 0x200
#define O_NOFOLLOW 0x100
#define UIO_WRITE 1
#define UIO_READ 0
#define UIO_SYSSPACE 0
#define IO_SYNC 1
#define MNT_WAIT 1
#define ISLASTCN 1
#define DELETE 2
#define RENAME 3
#define va_uid 0
#define va_gid 1
#define va_mode 2
#define va_nlink 3
#define va_data_size 4
struct vnode_attr {unsigned mask,supported;uint64_t values[5];};
/* Field syntax va.va_uid is rewritten to an array member by token macros. */
#undef va_uid
#undef va_gid
#undef va_mode
#undef va_nlink
#undef va_data_size
#define va_uid values[0]
#define va_gid values[1]
#define va_mode values[2]
#define va_nlink values[3]
#define va_data_size values[4]
#define BIT_va_uid 1u
#define BIT_va_gid 2u
#define BIT_va_mode 4u
#define BIT_va_nlink 8u
#define BIT_va_data_size 16u
#define VATTR_INIT(v) memset(v,0,sizeof(*(v)))
#define VATTR_WANTED(v,f) ((v)->mask|=BIT_##f)
#define VATTR_SET(v,f,x) ((v)->mask|=BIT_##f,(v)->f=(x))
#define VATTR_IS_SUPPORTED(v,f) (((v)->supported&BIT_##f)!=0)
typedef struct node {int dir,regular,uid,mode,links,mount,exists;unsigned bytes;unsigned char data[2048];} *vnode_t;
typedef int vfs_context_t;
struct componentname {int cn_nameiop,cn_flags,cn_context;char *cn_nameptr;int cn_namelen;};
static struct node root,dirs[3],targets[2],temps[2];static vnode_t rootvnode=&root;
static unsigned writes,renames,removes,enabled=1,corrupt_capture,partial_capture,unsafe_temp,unsafe_target;
static const char *leaves[]={"launchd.a04br3","a04-fb-capture"};
static int which(const char *path){return strstr(path,"a04-fb-capture")?1:0;}
static vfs_context_t vfs_context_kernel(void){return 1;}
static int vfs_context_ucred(int x){return x;}
static int vfs_context_proc(int x){return x;}
static int vnode_isreg(vnode_t n){return n->regular;}
static int vnode_isdir(vnode_t n){return n->dir;}
static int vnode_mount(vnode_t n){return n->mount;}
static int vnode_getattr(vnode_t n,struct vnode_attr *v,int ctx){
 (void)ctx;v->supported=31;v->va_uid=n->uid;v->va_nlink=n->links;v->va_mode=n->mode;return 0;
}
static int vnode_setattr(vnode_t n,struct vnode_attr *v,int ctx){
 (void)ctx;if(v->mask&BIT_va_mode)n->mode=(int)v->va_mode;
 if(v->mask&BIT_va_data_size)n->bytes=(unsigned)v->va_data_size;return 0;
}
static int prepare_directory(const char *path,int *created){assert(!strncmp(path,"/usr",4));*created=0;return 0;}
static int vnode_lookup(const char *path,int flags,vnode_t *out,int ctx){
 (void)ctx;assert(flags==VNODE_LOOKUP_NOFOLLOW);
 const char *names[]={"/usr","/usr/appleinternal","/usr/appleinternal/sbin"};
 for(unsigned i=0;i<3;i++)if(!strcmp(path,names[i])){*out=dirs+i;return 0;}
 assert(!strncmp(path,"/usr/appleinternal/sbin/",24));*out=targets+which(path);return 0;
}
static int vnode_open(const char *path,int flags,int mode,int unused,vnode_t *out,int ctx){
 (void)unused;(void)ctx;assert(flags==(FWRITE|O_CREAT|O_NOFOLLOW)&&mode==0600);
 assert(!strncmp(path,"/usr/appleinternal/sbin/.",25));unsigned i=which(path);
 *out=temps+i;(*out)->exists=1;if(i&&unsafe_temp)(*out)->links=2;return 0;
}
static int vn_rdwr(int operation,vnode_t n,char *p,int count,off_t offset,int space,int flags,int cred,int *resid,int proc){
 (void)space;(void)flags;(void)cred;(void)proc;assert(count>=0&&offset>=0&&offset+count<=2048);
 *resid=0;if(operation==UIO_WRITE){writes++;memcpy(n->data+offset,p,count);n->bytes=(unsigned)(offset+count);if(n==temps+1&&partial_capture)*resid=1;}
 else{memcpy(p,n->data+offset,count);if(n==temps+1&&corrupt_capture)p[0]^=1;}return 0;
}
static int vn_close(vnode_t n,int flags,int ctx){(void)n;(void)flags;(void)ctx;return 0;}
static void vnode_put(vnode_t n){(void)n;}
static int sync_node(vnode_t n,int flags,int ctx){(void)n;(void)flags;(void)ctx;return 0;}
#define VNOP_FSYNC sync_node
static int rename_node(vnode_t p,vnode_t temp,struct componentname *from,vnode_t p2,vnode_t target,struct componentname *to,int ctx){
 (void)ctx;assert(p==dirs+2&&p2==p);unsigned i=target==targets+1;
 assert(temp==temps+i&&!strcmp(to->cn_nameptr,leaves[i]));
 assert(from->cn_nameptr[0]=='.'&&from->cn_nameiop==DELETE&&to->cn_nameiop==RENAME);
 *target=*temp;temp->exists=0;renames++;return 0;
}
#define VNOP_RENAME rename_node
static int remove_node(vnode_t p,vnode_t temp,struct componentname *cn,int flags,int ctx){
 (void)flags;(void)ctx;assert(p==dirs+2&&temp->links==1&&cn->cn_nameptr[0]=='.');temp->exists=0;removes++;return 0;
}
#define VNOP_REMOVE remove_node
static int PE_parse_boot_argn(const char *s,void *p,int bytes){(void)s;assert(bytes==sizeof(unsigned));*(unsigned *)p=enabled;return 1;}
#include "HEADER"
static void reset(void){
 writes=renames=removes=corrupt_capture=partial_capture=unsafe_temp=unsafe_target=0;enabled=1;
 root.mount=1;
 for(unsigned i=0;i<3;i++)dirs[i]=(struct node){.dir=1,.uid=0,.mode=0755,.links=1,.mount=1};
 for(unsigned i=0;i<2;i++){
  targets[i]=(struct node){.regular=1,.uid=0,.mode=0755,.links=1,.mount=1,.exists=1,.bytes=3,.data={'o','l','d'}};
  temps[i]=(struct node){.regular=1,.uid=0,.mode=0600,.links=1,.mount=1};
 }
}
int main(void){
 reset();assert(prepare_uart_console()==0&&writes==2&&renames==2);
 assert(targets[0].bytes==sizeof(a04br3_console_payload)&&targets[1].bytes==sizeof(a04br3_fb_capture_payload));
 assert(!memcmp(targets[0].data,a04br3_console_payload,targets[0].bytes));
 assert(!memcmp(targets[1].data,a04br3_fb_capture_payload,targets[1].bytes));
 assert(targets[0].mode==0755&&targets[1].mode==0755);
 reset();corrupt_capture=1;assert(prepare_uart_console()==0&&renames==1&&removes==1);
 assert(targets[1].bytes==3&&!memcmp(targets[1].data,"old",3));
 reset();partial_capture=1;assert(prepare_uart_console()==0&&renames==1&&removes==1);
 assert(!memcmp(targets[1].data,"old",3));
 reset();unsafe_temp=1;assert(prepare_uart_console()==0&&renames==1&&writes==1&&!removes);
 assert(!memcmp(targets[1].data,"old",3));
 reset();targets[1].links=2;assert(prepare_uart_console()==0&&renames==1&&writes==1);
 reset();enabled=0;assert(prepare_uart_console()==0&&!writes&&!renames);
 reset();targets[0].links=2;assert(prepare_uart_console()==EEXIST&&!writes&&!renames);
 assert(a04_console_install_program(a04br3_console_payload,0,A04_CONSOLE_LEAF,A04_CONSOLE_TEMP)==EFBIG);
 puts("actual two-program atomic installer PASS");return 0;
}
'''.replace('HEADER',str(ROOT/'bsd/dev/a04br3_console_install.h'))
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'a04br3_console_payload.inc').write_text('static const unsigned char a04br3_console_payload[700]={1,2,3};\n#define A04BR3_CAPTURE_PAYLOAD_PRESENT 1\nstatic const unsigned char a04br3_fb_capture_payload[1100]={4,5,6};\n')
   (p/'install.c').write_text(source)
   subprocess.run(['clang','-std=c11','-D_DEFAULT_SOURCE','-Wall','-Wextra','-Werror',
                   '-fsanitize=address,undefined','-I',str(p),str(p/'install.c'),'-o',str(p/'install')],check=True)
   subprocess.run([str(p/'install')],check=True)
   # The exact same installer must compile and run for a legacy generated
   # include with no capture macro or symbol at all.
   (p/'a04br3_console_payload.inc').write_text('static const unsigned char a04br3_console_payload[700]={1,2,3};\n')
   old_source=source[:source.index('int main(void){')]+'''int main(void){reset();assert(prepare_uart_console()==0&&writes==1&&renames==1);assert(!memcmp(targets[1].data,"old",3));return 0;}'''
   (p/'legacy.c').write_text(old_source)
   subprocess.run(['clang','-std=c11','-D_DEFAULT_SOURCE','-Wall','-Wextra','-Werror',
                   '-fsanitize=address,undefined','-I',str(p),str(p/'legacy.c'),'-o',str(p/'legacy')],check=True)
   subprocess.run([str(p/'legacy')],check=True)
if __name__=='__main__':unittest.main()
