# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Execute actual BSD glue bodies with OS primitives modeled, no hardware I/O."""
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[2]
DEV=ROOT/'bsd/dev'
PRE=r'''
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "a04br3_usb_stream.h"
typedef unsigned dev_t;typedef unsigned long u_long;typedef char *caddr_t;
typedef long user_ssize_t;typedef int lck_mtx_t;typedef int wait_result_t;
typedef int kern_return_t;typedef void *thread_t;
struct proc {int root;};struct uio {unsigned char *data;long left;unsigned moved;int fault;};
struct timespec {long tv_sec,tv_nsec;};struct selinfo {unsigned waiting;};
#define LCK_GRP_DECLARE(a,b) int a __attribute__((unused))
#define LCK_MTX_DECLARE(a,b) lck_mtx_t a
#define minor(d) ((d)&255)
#define makedev(a,b) (((a)<<8)|(b))
#define EPERM 1
#define EIO 5
#define ENXIO 6
#define EAGAIN 35
#define EWOULDBLOCK 35
#define ETIMEDOUT 60
#define EBUSY 16
#define ENOTTY 25
#define EINTR 4
#define IO_NDELAY 16
#define PCATCH 256
#define FREAD 1
#define FWRITE 2
#define DEVFS_CHAR 1
#define UID_ROOT 0
#define GID_WHEEL 0
#define THREAD_UNINT 0
#define THREAD_CONTINUE_NULL NULL
#define NSEC_PER_MSEC 1000000
#define KERN_SUCCESS 0
#define THREAD_NULL NULL
static unsigned bootarg,devices,threads,lockdepth,wakes,waits,mode,init_start;
static int waitmode,copyfault;static uint64_t fake_now;
static void lck_mtx_lock(lck_mtx_t *l){(void)l;assert(!lockdepth++);}
static void lck_mtx_unlock(lck_mtx_t *l){(void)l;assert(lockdepth--==1);}
static int proc_suser(struct proc *p){return !p->root;}
static long uio_resid(struct uio *u){return u->left;}
static int uiomove(caddr_t data,unsigned n,struct uio *u){assert(lockdepth==1);if(copyfault)n=n?1:0;
 if(u->fault)memcpy(data,u->data+u->moved,n);else memcpy(u->data+u->moved,data,n);
 u->left-=n;u->moved+=n;return copyfault?EIO:0;}
static int msleep(void *,lck_mtx_t *,int,const char *,struct timespec *);
static void wakeup(void *p){(void)p;wakes++;}
static void selrecord(struct proc *p,struct selinfo *s,void *w){(void)p;(void)w;s->waiting++;}
static void selwakeup(struct selinfo *s){s->waiting=0;}
struct cdevsw {int (*d_open)(dev_t,int,int,struct proc *);int (*d_close)(dev_t,int,int,struct proc *);
 int (*d_read)(dev_t,struct uio *,int);int (*d_write)(dev_t,struct uio *,int);
 int (*d_ioctl)(dev_t,u_long,caddr_t,int,struct proc *);int(*d_stop)(void);int(*d_reset)(void);
 int(*d_select)(dev_t,int,void *,struct proc *);int(*d_mmap)(void);int(*d_strategy)(void);int(*d_reserved_1)(void);int(*d_reserved_2)(void);int d_type;};
static int eno_stop(void){return 1;}static int eno_reset(void){return 1;}static int eno_mmap(void){return 1;}
static int eno_strat(void){return 1;}static int eno_getc(void){return 1;}static int eno_putc(void){return 1;}
static int cdevsw_add(int n,const struct cdevsw *s){(void)n;assert(s->d_open&&s->d_read&&s->d_write);devices++;return 10;}
static int cdevsw_remove(int n,const struct cdevsw *s){(void)n;(void)s;return 0;}
static void *devfs_make_node(dev_t d,int type,int uid,int gid,int m,const char *name){(void)d;(void)gid;assert(type==1&&uid==0&&!strcmp(name,"a04usb"));mode=m;return (void *)1;}
static int PE_parse_boot_argn(const char *s,void *p,unsigned n){assert(!strcmp(s,"a04br3.usb")&&n==4);*(unsigned *)p=bootarg;return bootarg!=0;}
static int kernel_thread_start(void (*f)(void *,int),void *p,thread_t *t){(void)f;(void)p;threads++;*t=(void *)1;return 0;}
static void thread_deallocate(thread_t t){(void)t;}
static void thread_terminate(thread_t t){(void)t;}
static thread_t current_thread(void){return NULL;}
static void clock_interval_to_deadline(unsigned v,unsigned n,uint64_t *d){*d=fake_now+(uint64_t)v*n;}
static uint64_t mach_absolute_time(void){return fake_now;}
static void absolutetime_to_nanoseconds(uint64_t t,uint64_t *n){*n=t;}
static void assert_wait_deadline(void *a,int b,uint64_t c){(void)a;(void)b;(void)c;}
static void thread_block(void *p){(void)p;}
int a04br3_usb_controller_start(void){init_start++;return 0;}
int a04br3_usb_controller_poll(void){return -EIO;}
void a04br3_usb_controller_stop(void){}
'''
POST=r'''
static int msleep(void *c,lck_mtx_t *m,int pri,const char *msg,struct timespec *ts){
 (void)c;(void)m;(void)msg;assert(lockdepth==1&&pri==PCATCH);waits++;
 if(ts){assert(ts->tv_sec==0&&ts->tv_nsec>0&&ts->tv_nsec<=100000000);fake_now+=(uint64_t)ts->tv_nsec;if(waitmode==3){stream.tx.used=0;stream.pending_in=1;waitmode=4;return EAGAIN;}if(waitmode==4){stream.pending_in=0;stream.tx.used=0;return 0;}if(waitmode==5){stream.online=0;a04_usb_stream_reset(&stream);return 0;}return EAGAIN;}
 if(waitmode==1){unsigned char a='X';a04_usb_ring_put(&stream.rx,&a,1);return 0;}
 if(waitmode==2){stream.online=0;a04_usb_stream_reset(&stream);return 0;}
 return EINTR;
}
int main(void){
 struct proc root={1},user={0};unsigned char in[512],out[512];for(unsigned i=0;i<512;i++)in[i]=(unsigned char)i;
 a04br3_usb_init();assert(!devices&&!threads&&!registered&&!init_start);
 bootarg=2;a04br3_usb_init();assert(!devices&&!threads);
 bootarg=1;a04br3_usb_init();assert(devices==1&&threads==1&&mode==0600&&stream.generation==1);
 a04br3_usb_init();assert(devices==1&&threads==1);
 assert(usb_open(0,0,0,&user)==EPERM);assert(usb_open(1,0,0,&root)==ENXIO);
 assert(usb_open(0,0,0,&root)==EAGAIN);a04br3_usb_console_online(1);
 assert(!usb_open(0,0,0,&root));assert(usb_open(0,0,0,&root)==EBUSY);
 unsigned initial_wakes=wakes;a04br3_usb_console_online(1);assert(wakes==initial_wakes);
 assert(a04br3_usb_console_rx_space()==A04_USB_RING_SIZE);
 struct uio u={out,5,0,0};assert(usb_read(0,&u,IO_NDELAY)==EAGAIN&&!u.moved);
 waitmode=1;u=(struct uio){out,1,0,0};assert(!usb_read(0,&u,0)&&u.moved==1&&out[0]=='X');
 waitmode=0;u=(struct uio){out,1,0,0};assert(usb_read(0,&u,0)==EINTR);
 assert(a04br3_usb_console_rx(in,512)==512);u=(struct uio){out,512,0,0};assert(!usb_read(0,&u,0)&&!memcmp(in,out,512));
 for(unsigned i=0;i<A04_USB_RING_SIZE/512;i++)assert(a04br3_usb_console_rx(in,512)==512);
 assert(!a04br3_usb_console_rx(in,1)&&!a04br3_usb_console_rx_space());
 for(unsigned i=0;i<A04_USB_RING_SIZE/512;i++){u=(struct uio){out,512,0,0};assert(!usb_read(0,&u,0)&&!memcmp(in,out,512));}
 for(unsigned i=0;i<A04_USB_RING_SIZE/512;i++){u=(struct uio){in,512,0,1};assert(!usb_write(0,&u,0)&&u.moved==512);}
 unsigned before=waits;u=(struct uio){in,5,0,1};assert(usb_write(0,&u,IO_NDELAY)==EAGAIN&&waits==before);
 assert(usb_write(0,&u,0)==EAGAIN&&waits==before+1&&!u.moved);
 assert(a04br3_usb_console_tx(out,512)==512&&!memcmp(out,in,512));
 assert(a04br3_usb_console_rx(in,512)==512);uint64_t g=a04br3_usb_console_generation();
 a04br3_usb_console_online(0);assert(stream.generation==g+1&&!stream.rx.used&&!stream.tx.used);
 assert(!a04br3_usb_console_tx(out,512)&&!a04br3_usb_console_rx(in,512));
 a04br3_usb_console_online(1);u=(struct uio){out,1,0,0};assert(usb_read(0,&u,0)==EIO);
 u=(struct uio){in,1,0,1};assert(usb_write(0,&u,0)==EIO&&usb_open(0,0,0,&root)==EBUSY);
 assert(!usb_close(0,0,0,&root));assert(!usb_open(0,0,0,&root));assert(!stream.rx.used&&!stream.tx.used);
 assert(a04br3_usb_console_rx(in,5)==5);copyfault=1;u=(struct uio){out,5,0,0};assert(usb_read(0,&u,0)==EIO&&u.moved==1&&stream.rx.used==4);
 copyfault=0;u=(struct uio){out,4,0,0};assert(!usb_read(0,&u,0)&&!memcmp(out,in+1,4));
 assert(usb_ioctl(0,0,NULL,0,&root)==ENOTTY);
 assert(usb_ioctl(0,0x20005570UL,NULL,0,&user)==EPERM);
 assert(!usb_ioctl(0,0x20005570UL,NULL,0,&root));
 u=(struct uio){in,4,0,1};assert(!usb_write(0,&u,0));
 waitmode=3;assert(usb_ioctl(0,0x20005570UL,NULL,0,&root)==0); // FIFO pull then later hardware completion; ring-empty alone is insufficient
 u=(struct uio){in,4,0,1};assert(!usb_write(0,&u,0));
 assert(a04br3_usb_console_tx(out,4)==4&&stream.pending_in);
 before=waits;waitmode=0;assert(usb_ioctl(0,0x20005570UL,NULL,0,&root)==ETIMEDOUT&&waits==before+50);
 a04br3_usb_console_tx_complete();assert(!usb_ioctl(0,0x20005570UL,NULL,0,&root));
 u=(struct uio){in,4,0,1};assert(!usb_write(0,&u,0));waitmode=5;
 assert(usb_ioctl(0,0x20005570UL,NULL,0,&root)==EIO);usb_close(0,0,0,&root);a04br3_usb_console_online(1);assert(!usb_open(0,0,0,&root));waitmode=0;

 assert(!usb_select(0,FREAD,NULL,&root));assert(read_select.waiting);
 assert(a04br3_usb_console_rx(in,1)==1&&!read_select.waiting);
 a04br3_usb_console_online(0);assert(usb_select(0,FREAD,NULL,&root));
 usb_close(0,0,0,&root);stream.generation=UINT64_MAX;a04br3_usb_console_online(1);usb_close(0,0,0,&root);assert(stream.failed&&!stream.online);
 assert(!lockdepth&&wakes);puts("PASS actual cdev: root/exclusive/nonblock/blocking/backpressure/partialcopy/reset/stalefd/generation/selection");return 0;
}
'''
def body():
    source=(DEV/'a04br3_usb.c').read_text()
    return '\n'.join(s for s in source.splitlines() if not s.startswith('#include') and not s.startswith('#if') and not s.startswith('#endif'))
class USBConsole(unittest.TestCase):
    def test_actual_cdev_contract_and_generation(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);source=p/'test.c';source.write_text(PRE+body()+POST)
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(DEV),str(source),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)
    def test_actual_glue_arm64(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);source=p/'compile.c'
            # Same real driver bodies, declaration-only OS ABI model, no hardware
            # callbacks executed. Syntax/code generation does not claim SDK link.
            prefix=PRE.replace('#include <stdio.h>','extern int printf(const char *,...);extern int puts(const char *);').replace('#include <string.h>','extern void *memcpy(void *,const void *,size_t);extern int memcmp(const void *,const void *,size_t);extern int strcmp(const char *,const char *);').replace('#include <assert.h>','#define assert(x) ((void)(x))')
            source.write_text(prefix+body()+POST)
            subprocess.run(['clang','-target','arm64-apple-ios16.0','-ffreestanding','-std=c11','-I',str(DEV),'-c',str(source),'-o',str(p/'glue.o')],check=True)
if __name__=='__main__':unittest.main()
