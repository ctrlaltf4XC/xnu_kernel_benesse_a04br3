# SPDX-License-Identifier: MIT
"""Compile the actual private cdev transaction code with a mock eMMC and real SHA256.
No device, UART, copied boot image or flash access is used by these tests.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[2]

class BootUpdate(unittest.TestCase):
    def test_actual_transaction_and_gpt(self):
        source = (ROOT/'bsd/dev/a04br3_msdc.c').read_text()
        body = source[source.index('static LCK_MTX_DECLARE(boot_lock'):source.index('static const struct cdevsw boot_cdevsw')]
        harness = r'''
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <openssl/sha.h>
#define __unused __attribute__((unused))
#define FWRITE 2
#define KERN_SUCCESS 0
#define KMA_ZERO 0
#define VM_KERN_MEMORY_BSD 0
#define CFG 0
#define INTEN 4
#define INTR 8
#define MIN(a,b) ((a)<(b)?(a):(b))
#define LCK_MTX_DECLARE(n,g) int n
#define lck_mtx_lock(x) ((void)(x))
#define lck_mtx_unlock(x) ((void)(x))
typedef uintptr_t vm_offset_t;
typedef size_t vm_size_t;
typedef int64_t user_ssize_t;
struct proc { int root; };
static struct proc rootproc={1},other={0};
static struct proc *active=&rootproc;
static struct proc *current_proc(void){return active;}
static int proc_suser(struct proc *p){return p->root?0:EPERM;}
static int kernel_map,msdc_lock,allocations,allocfail;
static int kmem_alloc(int map,vm_offset_t *out,size_t size,int flags,int tag){
 (void)map;(void)flags;(void)tag;
 if(allocfail && ++allocations==allocfail)return ENOMEM;
 *out=(uintptr_t)calloc(1,size);return *out?0:ENOMEM;
}
static void kmem_free(int map,uintptr_t p,size_t size){(void)map;(void)size;free((void *)p);}
struct uio { int64_t offset,resid;const uint8_t *data;int fault; };
static int64_t uio_offset(struct uio *u){return u->offset;}
static int64_t uio_resid(struct uio *u){return u->resid;}
static int uiomove(char *p,int bytes,struct uio *u){
 int n=u->fault?bytes/2:bytes;memcpy(p,u->data,n);u->data+=n;u->offset+=n;u->resid-=n;
 return u->fault?EFAULT:0;
}
'''+f'#include "{ROOT}/bsd/dev/a04br3_boot_update.h"\n'+r'''
static unsigned boot_update_enabled=1;
static uint32_t disk_sectors=8192,boot_first=100,boot_sectors=4096,boot_gpt_lba=1;
static uint8_t disk[8192*512],image[132*1024];
static unsigned writes,reads,flushes,writefault,readfault,flushfault;
static uint32_t regs[4];
static uint32_t rd(unsigned r){return regs[r/4];}
static void wr(unsigned r,uint32_t v){regs[r/4]=v;}
static uint32_t le32(const uint8_t *p){return a04_boot_u32(p);}
static uint64_t le64(const uint8_t *p){return a04_boot_u64(p);}
static int read_block(unsigned op,uint32_t lba,uint8_t *p){
 assert(op==17 && lba<disk_sectors);memcpy(p,disk+lba*512,512);return 0;
}
static int read_blocks(unsigned op,uint32_t lba,uint8_t *p,unsigned n){
 assert(op==17 && n && n<=128 && lba>=boot_first && lba+n<=boot_first+boot_sectors);
 ++reads;memcpy(p,disk+lba*512,n*512);if(readfault)p[0]^=1;return 0;
}
static int write_blocks_raw(uint32_t lba,const uint8_t *p,unsigned n){
 assert(n && n<=128 && lba>=boot_first && lba+n<=boot_first+boot_sectors);
 if(writefault && ++writes==writefault)return EIO;
 if(!writefault)++writes;
 memcpy(disk+lba*512,p,n*512);return 0;
}
static int flush_card_force(void){++flushes;return flushfault?EIO:0;}
'''+body+r'''
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static void put64(uint8_t *p,uint64_t v){put32(p,(uint32_t)v);put32(p+4,(uint32_t)(v>>32));}
static void crc_gpt(void){
 uint8_t *h=disk+512;put32(h+88,a04_boot_crc(disk+1024,4*128,0));
 put32(h+16,a04_boot_crc(h,92,1));
}
static void make_gpt(void){
 memset(disk,0,sizeof(disk));uint8_t *h=disk+512,*e=disk+1024;
 memcpy(h,"EFI PART",8);put32(h+12,92);put64(h+24,1);put64(h+32,8191);
 put64(h+40,34);put64(h+48,8158);put64(h+72,2);put32(h+80,4);put32(h+84,128);
 e[0]=1;put64(e+32,100);put64(e+40,4195);memcpy(e+56,"b\0o\0o\0t",8);
 e+=128;e[0]=1;put64(e+32,5000);put64(e+40,7999);memcpy(e+56,"r\0o\0o\0t",8);
 crc_gpt();
}
static void make_image(void){
 memset(image,0,sizeof(image));memcpy(image,"ANDROID!",8);
 unsigned k=530,r=sizeof(image)-4096;
 put32(image+8,k);put32(image+12,0x40080000);put32(image+16,r);put32(image+20,0x50000000);
 put32(image+28,0x40f00000);put32(image+32,0x4e000000);put32(image+36,2048);
 memcpy(image+64,"bootopt=64S3,32N2,64N2",22);
 put32(image+2048,0x58881688);put32(image+2052,k-512);memcpy(image+2056,"KERNEL",6);
 image[2560]=0x1f;image[2561]=0x8b;image[2562]=8;
 put32(image+4096,0x58881688);put32(image+4100,r-512);memcpy(image+4104,"ROOTFS",6);
 memcpy(image+4608,"A04XNU01",8);
}
static void digest(uint8_t out[32]){SHA256(image,sizeof(image),out);}
static void start(void){
 assert(a04br3_boot_rx_interval_ms()==16);
 assert(boot_open(0,FWRITE,0,&rootproc)==0);
 struct a04_boot_begin b={.image_bytes=sizeof(image)};digest(b.sha256);
 assert(boot_ioctl(0,A04_BOOT_BEGIN,(char *)&b,FWRITE,&rootproc)==0);
 assert(a04br3_boot_rx_interval_ms()==1);
 assert(!writes);
}
static void upload(void){
 struct uio u={0,sizeof(image),image,0};assert(boot_write(0,&u,0)==0);
 assert(!u.resid && !writes && boot_staged==sizeof(image));
}
static void reset(void){
 boot_close(0,0,0,&rootproc);writes=reads=flushes=writefault=readfault=flushfault=0;
 allocations=allocfail=0;active=&rootproc;make_gpt();make_image();
}
int main(void){
 uint32_t first,count;make_gpt();make_image();
 assert(sizeof(struct a04_boot_info)==32 && sizeof(struct a04_boot_begin)==48);
 assert(a04_boot_gpt(disk+512,disk+1024,8192,&first,&count)==0 && first==100 && count==4096);
 assert(a04_boot_image(image,sizeof(image))==0);
 assert(a04_boot_length(0,1)!=0 && a04_boot_length(A04_BOOT_MAX_BYTES+2048,A04_BOOT_MAX_BYTES+2048)!=0);
 assert(a04_boot_length(6144,6143)!=0 && a04_boot_length(6145,10000)!=0);
 /* Duplicated/overlapping boot, CRC corruption, missing/type-zero boot. */
 memcpy(disk+1280,disk+1024,128);crc_gpt();assert(a04_boot_gpt(disk+512,disk+1024,8192,&first,&count)!=0);
 make_gpt();put64(disk+1152+32,200);crc_gpt();assert(a04_boot_gpt(disk+512,disk+1024,8192,&first,&count)!=0);
 make_gpt();disk[1040]^=1;assert(a04_boot_gpt(disk+512,disk+1024,8192,&first,&count)!=0);
 make_gpt();disk[512+50]^=1;assert(a04_boot_gpt(disk+512,disk+1024,8192,&first,&count)!=0);
 make_gpt();disk[1024]=0;crc_gpt();assert(a04_boot_gpt(disk+512,disk+1024,8192,&first,&count)!=0);
 reset();assert(boot_open(0,FWRITE,0,&other)==EPERM);start();assert(boot_open(0,FWRITE,0,&rootproc)==EBUSY);
 assert(boot_commit()==EINVAL && !writes);upload();
 assert(boot_commit()==0 && writes==3 && reads==3 && flushes==2 && boot_flags==A04_BOOT_VERIFIED);
 assert(a04br3_boot_rx_interval_ms()==16);
 assert(!memcmp(disk+boot_first*512,image,sizeof(image)));
 assert(!disk[boot_first*512-1] && !disk[boot_first*512+sizeof(image)]);
 assert(boot_commit()==EINVAL);reset();start();upload();boot_digest[0]^=1;
 assert(boot_commit()==EINVAL && !writes && boot_flags==A04_BOOT_FAILED);
 reset();image[20]^=1;start();upload();assert(boot_commit()==EINVAL && !writes);
 reset();start();upload();disk[512+16]^=1;assert(boot_commit()!=0 && !writes);
 reset();start();upload();writefault=2;assert(boot_commit()==EIO && writes==2 && !reads && boot_flags==A04_BOOT_FAILED);
 reset();start();upload();readfault=1;assert(boot_commit()==EIO && boot_flags==A04_BOOT_FAILED);
 reset();start();upload();flushfault=1;assert(boot_commit()==EIO && !reads);
 reset();start();struct uio u={0,sizeof(image),image,1};
 assert(boot_write(0,&u,0)==EFAULT && boot_staged==32768 && boot_flags==A04_BOOT_FAILED);
 assert(boot_commit()==EINVAL && !writes);
 reset();start();u=(struct uio){1,2,image,0};assert(boot_write(0,&u,0)==EINVAL && !writes);
 reset();start();u=(struct uio){0,sizeof(image)+1,image,0};assert(boot_write(0,&u,0)==EINVAL && !writes);
 reset();start();active=&other;u=(struct uio){0,1,image,0};assert(boot_write(0,&u,0)==EPERM);
 assert(boot_ioctl(0,A04_BOOT_COMMIT,0,FWRITE,&other)==EPERM);active=&rootproc;
 assert(boot_ioctl(0,A04_BOOT_COMMIT,0,0,&rootproc)==EBADF);
 assert(boot_ioctl(0,A04_BOOT_ABORT,0,FWRITE,&rootproc)==0 && !boot_stage && !writes);
 assert(a04br3_boot_rx_interval_ms()==16);
 reset();allocfail=2;assert(boot_open(0,FWRITE,0,&rootproc)==0);
 struct a04_boot_begin b={.image_bytes=sizeof(image)};digest(b.sha256);
 assert(boot_ioctl(0,A04_BOOT_BEGIN,(char *)&b,FWRITE,&rootproc)==ENOMEM && !boot_stage);
 assert(boot_transfer(4096ULL*512,image,512,1)==EINVAL && !writes);
 assert(boot_transfer(0,image,65537,1)==EINVAL && !writes);
 assert(boot_transfer(1,image,512,1)==EINVAL && !writes);
 reset();start();
 for(unsigned at=0;at<sizeof(image);) {
  unsigned n=sizeof(image)-at;if(n>384)n=384;
  struct uio chunk={at,n,image+at,0};assert(boot_write(0,&chunk,0)==0 && !chunk.resid && !writes);at+=n;
 }
 assert(boot_commit()==0 && boot_flags==A04_BOOT_VERIFIED);
 reset();boot_update_enabled=0;assert(boot_open(0,FWRITE,0,&rootproc)==ENXIO);boot_update_enabled=1;
 reset();puts("boot updater actual cdev/GPT/SHA256/bounded-disk contracts PASS");return 0;
}
'''
        with tempfile.TemporaryDirectory() as d:
            src=Path(d)/'test.c';exe=Path(d)/'test';src.write_text(harness)
            subprocess.run(['cc','-Wall','-Wextra','-Werror','-Wno-deprecated-declarations','-Wno-unused-function',
                            '-fsanitize=address,undefined','-g',str(src),'-lcrypto','-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

    def test_existing_raw_policy_and_registration(self):
        s=(ROOT/'bsd/dev/a04br3_msdc.c').read_text()
        self.assertIn('UID_ROOT,GID_WHEEL,0600,"a04boot"',s)
        self.assertIn('if ((count&511u)',s)
        self.assertIn('if (!reading && (!write_enabled || minor(buf_device(bp))!=1))',s)
        self.assertIn('static int disk_write(__unused dev_t dev,__unused struct uio *uio,__unused int flags) { return EROFS; }',s)
        body=s[s.index('static int boot_commit('):s.index('static int boot_write(')]
        self.assertNotIn('reboot(',body)
        self.assertNotIn('panic(',body)

if __name__=='__main__':unittest.main()
