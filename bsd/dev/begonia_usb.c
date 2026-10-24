/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Opt-in board USB console cdev and finite-work polling thread.
 * Canonical console/verified bootwriter remain native userland owners.
 */
#if defined(ARM64_BOARD_CONFIG_BEGONIA)
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <sys/select.h>
#include <kern/locks.h>
#include <kern/thread.h>
extern kern_return_t thread_terminate(thread_t);
#include <kern/sched_prim.h>
#include <kern/clock.h>
#include <mach/mach_time.h>
#include <miscfs/devfs/devfs.h>
#include <pexpert/pexpert.h>
#include "begonia_usb_stream.h"

static LCK_GRP_DECLARE(begoniausb_group,"begonia_usb");
static LCK_MTX_DECLARE(begoniausb_lock,&begoniausb_group);
static struct begonia_usb_stream stream;
static struct selinfo read_select,write_select;
static unsigned registered;
/* Implemented by the real clock/PHY + gadget adapter, not success stubs.
 * start0=ready to service EP0 (not necessarily configured); poll0=idle,
 * poll1=progress, negative errno=fatal. Adapter updates real online state. */
extern int begonia_usb_controller_start(void);
extern int begonia_usb_controller_poll(void);
extern void begonia_usb_controller_stop(void);

static void notify(void) {
 wakeup(&stream.rx);wakeup(&stream.tx);
 selwakeup(&read_select);selwakeup(&write_select);
}
void begonia_usb_console_online(unsigned online) {
 lck_mtx_lock(&begoniausb_lock);
 if(!online && stream.online){stream.online=0;begonia_usb_stream_reset(&stream);notify();}
 else if(online && !stream.online && !stream.failed){stream.online=1;notify();}
 lck_mtx_unlock(&begoniausb_lock);
}
uint64_t begonia_usb_console_generation(void) {
 lck_mtx_lock(&begoniausb_lock);uint64_t g=stream.generation;lck_mtx_unlock(&begoniausb_lock);return g;
}
unsigned begonia_usb_console_rx_space(void) {
 lck_mtx_lock(&begoniausb_lock);
 unsigned n=stream.online&&!begonia_usb_stream_stale(&stream)?BEGONIA_USB_RING_SIZE-stream.rx.used:0;
 lck_mtx_unlock(&begoniausb_lock);return n;
}
unsigned begonia_usb_console_rx(const void *bytes,unsigned n) {
 if(!bytes || !n || n>512)return 0;
 lck_mtx_lock(&begoniausb_lock);
 unsigned got=stream.online&&!begonia_usb_stream_stale(&stream)?begonia_usb_ring_put(&stream.rx,bytes,n):0;
 if(got){wakeup(&stream.rx);selwakeup(&read_select);}
 lck_mtx_unlock(&begoniausb_lock);return got;
}
unsigned begonia_usb_console_tx(void *bytes,unsigned cap) {
 if(!bytes || !cap || cap>512)return 0;
 lck_mtx_lock(&begoniausb_lock);
 unsigned got=stream.online&&!begonia_usb_stream_stale(&stream)?begonia_usb_ring_get(&stream.tx,bytes,cap):0;
 if(got)stream.pending_in=1;
 if(got){wakeup(&stream.tx);selwakeup(&write_select);}
 lck_mtx_unlock(&begoniausb_lock);return got;
}
void begonia_usb_console_tx_complete(void) {
 lck_mtx_lock(&begoniausb_lock);stream.pending_in=0;wakeup(&stream.tx);selwakeup(&write_select);lck_mtx_unlock(&begoniausb_lock);
}
static int usb_open(dev_t dev,int flags,int type,struct proc *p) {
 (void)flags;(void)type;if(minor(dev))return ENXIO;
 if(proc_suser(p))return EPERM;
 lck_mtx_lock(&begoniausb_lock);int e=0;
 if(!registered || stream.failed)e=ENXIO;
 else if(stream.opened)e=EBUSY;
 else if(!stream.online)e=EAGAIN; /* supervisor retries; PID1 never waits here */
 else {begonia_usb_ring_clear(&stream.rx);begonia_usb_ring_clear(&stream.tx);stream.opened=1;stream.opened_generation=stream.generation;}
 lck_mtx_unlock(&begoniausb_lock);return e;
}
static int usb_close(dev_t dev,int flags,int type,struct proc *p) {
 (void)dev;(void)flags;(void)type;(void)p;
 lck_mtx_lock(&begoniausb_lock);stream.opened=0;begonia_usb_stream_reset(&stream);notify();lck_mtx_unlock(&begoniausb_lock);return 0;
}
static int usb_read(dev_t dev,struct uio *uio,int flags) {
 (void)dev;uint8_t buf[512];int e=0;
 if(!uio_resid(uio))return 0;
 lck_mtx_lock(&begoniausb_lock);
 for(;;){
  if(begonia_usb_stream_stale(&stream) || !stream.online){e=EIO;break;}
  if(stream.rx.used){
   unsigned n=stream.rx.used;if(n>sizeof(buf))n=sizeof(buf);if(n>(uint64_t)uio_resid(uio))n=(unsigned)uio_resid(uio);
   /* uiomove failure must not silently consume a request prefix. */
   for(unsigned i=0;i<n;i++)buf[i]=stream.rx.bytes[(stream.rx.head+i)%BEGONIA_USB_RING_SIZE];
   user_ssize_t before=uio_resid(uio);e=uiomove((caddr_t)buf,n,uio);
   unsigned copied=(unsigned)(before-uio_resid(uio));
   if(copied)begonia_usb_ring_get(&stream.rx,buf,copied);
   break;
  }
  if(flags&IO_NDELAY){e=EAGAIN;break;}
  e=msleep(&stream.rx,&begoniausb_lock,PCATCH,"begoniausbrx",NULL);if(e)break;
 }
 lck_mtx_unlock(&begoniausb_lock);return e;
}
static int usb_write(dev_t dev,struct uio *uio,int flags) {
 (void)dev;uint8_t buf[512];int e=0,waited=0;unsigned total=0;
 lck_mtx_lock(&begoniausb_lock);
 while(uio_resid(uio)>0 && total<4096){
  if(begonia_usb_stream_stale(&stream) || !stream.online){e=EIO;break;}
  unsigned space=BEGONIA_USB_RING_SIZE-stream.tx.used;
  if(!space){
   if(total)break;
   if((flags&IO_NDELAY)||waited){e=EAGAIN;break;}
   struct timespec ts={0,100000000};waited=1;
   e=msleep(&stream.tx,&begoniausb_lock,PCATCH,"begoniausbtx",&ts);
   if(e)break;continue;
  }
  unsigned n=space;if(n>sizeof(buf))n=sizeof(buf);if(n>(uint64_t)uio_resid(uio))n=(unsigned)uio_resid(uio);
  user_ssize_t before=uio_resid(uio);e=uiomove((caddr_t)buf,n,uio);
  unsigned copied=(unsigned)(before-uio_resid(uio));
  if(copied){begonia_usb_ring_put(&stream.tx,buf,copied);total+=copied;}
  if(e)break;
 }
 lck_mtx_unlock(&begoniausb_lock);return e;
}
static int usb_ioctl(dev_t d,u_long cmd,caddr_t data,int flags,struct proc *p) {
 (void)d;(void)data;(void)flags;if(cmd!=0x20005570UL)return ENOTTY;
 if(proc_suser(p))return EPERM;
 lck_mtx_lock(&begoniausb_lock);int e=0;uint64_t end;
 clock_interval_to_deadline(5000,NSEC_PER_MSEC,&end);
 for(;;){
  if(begonia_usb_stream_stale(&stream)||!stream.online){e=EIO;break;}
  if(!stream.tx.used&&!stream.pending_in)break;
  uint64_t now=mach_absolute_time(),ns;
  if(now>=end){e=ETIMEDOUT;break;}
  absolutetime_to_nanoseconds(end-now,&ns);
  if(ns>100000000)ns=100000000;
  struct timespec ts={0,(long)ns};
  e=msleep(&stream.tx,&begoniausb_lock,PCATCH,"begoniausbdrain",&ts);
  if(e&&e!=EWOULDBLOCK)break;e=0;
 }
 lck_mtx_unlock(&begoniausb_lock);return e;
}
static int usb_select(dev_t d,int which,void *wql,struct proc *p) {
 (void)d;lck_mtx_lock(&begoniausb_lock);int ready=0;
 if(begonia_usb_stream_stale(&stream)||!stream.online)ready=1;
 else if(which==FREAD){ready=stream.rx.used!=0;if(!ready)selrecord(p,&read_select,wql);}
 else if(which==FWRITE){ready=stream.tx.used<BEGONIA_USB_RING_SIZE;if(!ready)selrecord(p,&write_select,wql);}
 lck_mtx_unlock(&begoniausb_lock);return ready;
}
static struct cdevsw usb_cdevsw={
 .d_open=usb_open,.d_close=usb_close,.d_read=usb_read,.d_write=usb_write,
 .d_ioctl=usb_ioctl,.d_stop=eno_stop,.d_reset=eno_reset,.d_select=usb_select,
 .d_mmap=eno_mmap,.d_strategy=eno_strat,.d_reserved_1=eno_getc,.d_reserved_2=eno_putc,.d_type=0
};
static void usb_worker(void *arg,wait_result_t result) {
 (void)arg;(void)result;int error=begonia_usb_controller_start();
 printf("BEGONIA USB: controller start=%d; console not image verification\n",error);
 if(!error)for(;;){
  int progress=0;
  for(unsigned i=0;i<16;i++){int r=begonia_usb_controller_poll();if(r<0){error=r;break;}if(!r)break;progress=1;}
  if(error)break;
  /* No empty busy-loop. Finite packet budget, release CPU between polls. */
  uint64_t deadline;clock_interval_to_deadline(progress?1:5,NSEC_PER_MSEC,&deadline);
  assert_wait_deadline(&registered,THREAD_UNINT,deadline);thread_block(THREAD_CONTINUE_NULL);
 }
 begonia_usb_console_online(0);begonia_usb_controller_stop();
 printf("BEGONIA USB: worker stopped error=%d (no automatic restart)\n",error);
 thread_terminate(current_thread());
}
void begonia_usb_init(void) {
 unsigned enable=0;
 if(!PE_parse_boot_argn("a04br3.usb",&enable,sizeof(enable)) || enable!=1)return;
 if(registered)return;
 stream.generation=1;
 int major=cdevsw_add(-1,&usb_cdevsw);
 if(major<0){printf("BEGONIA USB: cdev registration failed\n");return;}
 if(!devfs_make_node(makedev(major,0),DEVFS_CHAR,UID_ROOT,GID_WHEEL,0600,"begoniausb")){
  cdevsw_remove(major,&usb_cdevsw);printf("BEGONIA USB: devfs registration failed\n");return;
 }
 registered=1;thread_t thread=THREAD_NULL;
 kern_return_t e=kernel_thread_start(usb_worker,NULL,&thread);
 if(e==KERN_SUCCESS)thread_deallocate(thread);
 else {stream.failed=1;printf("BEGONIA USB: worker creation failed=%d\n",e);}
 printf("BEGONIA USB: /dev/begoniausb root-exclusive stream; enabled by a04br3.usb=1 worker=%d\n",e);
}
#endif
