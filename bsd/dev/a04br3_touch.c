/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * TAB-A04-BR3 BU21025 single-contact input, polled PIO (no DMA/IRQ).
 * Register/protocol facts: board Linux BU21025 driver (SANYO TST, 2017),
 * MT8167 i2c-mt65xx, MT6392 regulator and mtk-pmic-wrap tables.
 * New bounded implementation. No Linux driver code is linked into XNU.
 */
#if defined(ARM64_BOARD_CONFIG_A04BR3)
#include <sys/systm.h>
#include <sys/errno.h>
#include <kern/thread.h>
#include <kern/sched_prim.h>
#include <kern/clock.h>
#include <mach/mach_types.h>
#include <pexpert/pexpert.h>
#include "a04br3_touch_protocol.h"
extern vm_offset_t ml_io_map(vm_offset_t,vm_size_t);
extern kern_return_t thread_terminate(thread_t);
extern void a04br3_hid_touch(int,int,int,int);
void a04br3_touch_init(void);
static vm_offset_t bus,gpio,top,infra,pwrap;
static int sleep_chan;
static unsigned started;
static uint32_t rd(vm_offset_t b,unsigned o){uint32_t v=*(volatile uint32_t *)(b+o);__asm__ volatile("dmb oshld":::"memory");return v;}
static void wr(vm_offset_t b,unsigned o,uint32_t v){*(volatile uint32_t *)(b+o)=v;__asm__ volatile("dsb oshst":::"memory");}
static uint16_t iread(unsigned o){uint16_t v=*(volatile uint16_t *)(bus+o);__asm__ volatile("dmb oshld":::"memory");return v;}
static void iw(unsigned o,uint16_t v){*(volatile uint16_t *)(bus+o)=v;__asm__ volatile("dsb oshst":::"memory");}
static void pause_ms(unsigned ms){uint64_t deadline;clock_interval_to_deadline(ms,NSEC_PER_MSEC,&deadline);assert_wait_deadline(&sleep_chan,THREAD_UNINT,deadline);thread_block(THREAD_CONTINUE_NULL);}
static int pw_wait(unsigned fsm){for(unsigned n=0;n<1000;n++){if(((rd(pwrap,0xa4)>>16)&7)==fsm)return 0;delay(10);}return ETIMEDOUT;}
static int pw_read(unsigned addr,uint16_t *value){int e=pw_wait(0);if(e)return e;wr(pwrap,0xa0,(addr>>1)<<16);e=pw_wait(6);if(e)return e;*value=(uint16_t)rd(pwrap,0xa4);wr(pwrap,0xa8,1);return pw_wait(0);}
static int pw_write(unsigned addr,uint16_t value){int e=pw_wait(0);if(e)return e;wr(pwrap,0xa0,0x80000000|((addr>>1)<<16)|value);return pw_wait(0);}
static int power_on(void){uint16_t v;int e;uint32_t init=rd(pwrap,0x9c);
    printf("A04BR3 TOUCH: PWRAP init=0x%x status=0x%x\n",init,rd(pwrap,0xa4));
    if(!(init&1))return ENXIO;
    e=pw_read(0x530,&v);if(e)return e;e=pw_write(0x530,(uint16_t)((v&~0xe0)|0x60));if(e)return e;
    e=pw_read(0x50a,&v);if(e)return e;e=pw_write(0x50a,v|0x8000);if(e)return e;
    pause_ms(20);e=pw_read(0x530,&v);if(e)return e;return (v&0xe0)==0x60?0:EIO;
}
static int transfer(int read,unsigned char *data,unsigned len){
    if(!len || len>2)return EINVAL;
    iw(0x0c,7);iw(0x38,1);iw(0x08,0);iw(0x10,0x28); /* ACK error + clock extension, PIO */
    iw(0x04,(0x48<<1)|(read?1:0));iw(0x14,(uint16_t)len);iw(0x18,1);iw(0x1c,2);
    if(!read)for(unsigned i=0;i<len;i++)iw(0,data[i]);iw(0x24,1);
    for(unsigned n=0;n<2000;n++){
        unsigned status=iread(0x0c);
        if(status&6){iw(0x0c,7);return EIO;}
        if(status&1){if(read)for(unsigned i=0;i<len;i++)data[i]=(unsigned char)iread(0);iw(0x0c,7);return 0;}
        delay(5);
    }
    iw(0x50,1);return ETIMEDOUT;
}
static int command(unsigned char c){return transfer(0,&c,1);}
static int measure(unsigned set,unsigned op,unsigned wait,unsigned *value){unsigned char b[2];int e=command((unsigned char)set);if(e)return e;delay(wait);e=command((unsigned char)op);if(e)return e;e=transfer(1,b,2);if(!e)*value=a04br3_touch_adc(b[0],b[1]);return e;}
static void touch_thread(void *arg,wait_result_t result){
    (void)arg;(void)result;int e=power_on();if(e){printf("A04BR3 TOUCH: VGP1 1.8V error=%d\n",e);(void)thread_terminate(current_thread());return;}
    /* GPIO58/59 ALT1; GPIO8 input. Only the specified fields are changed. */
    for(unsigned pin=58;pin<=59;pin++){unsigned off=0x300+(pin/5)*0x10,shift=(pin%5)*3;wr(gpio,off,(rd(gpio,off)&~(7u<<shift))|(1u<<shift));}
    wr(gpio,0x300+0x10,(rd(gpio,0x310)&~(7u<<9)));wr(gpio,0x08,1u<<8);
    wr(gpio,0x904,(1u<<3)|(1u<<15)); /* GPIO8 and I2C input enables */
    wr(top,0x84,1u<<3);wr(infra,0x80,rd(infra,0x80)&~(1u<<1));
    iw(0x50,1);iw(0x40,3);iw(0x54,0);iw(0x70,4);iw(0x20,0x73f);iw(0x48,0);iw(0x28,0x8001);
    e=command(0x50);if(!e)e=command(0xb0);if(!e)e=command(0);
    printf("A04BR3 TOUCH: BU21025 i2c0@48 init=%d timing=0x%x irq=%u\n",e,iread(0x20),(rd(gpio,0x200)>>8)&1);
    if(e){(void)thread_terminate(current_thread());return;}
    unsigned lastx=0,lasty=0,failures=0;int down=0;
    for(;;){
        if(!down && (rd(gpio,0x200)&(1u<<8))){pause_ms(20);continue;}
        unsigned x=0,y=0,z=0;e=measure(0x80,0xc4,1500,&x);if(!e)e=measure(0x90,0xd4,800,&y);if(!e)e=measure(0xa0,0xe4,600,&z);
        if(e){if(down){a04br3_hid_touch(0,0,lastx,lasty);down=0;}if(++failures<=3)printf("A04BR3 TOUCH: transfer error=%d\n",e);pause_ms(100);continue;}
        failures=0;
        /* Board raw landscape X/Y to portrait panel: X=1-Y, Y=X. */
        int active=a04br3_touch_point(x,y,z,&lastx,&lasty);
        if(active<0){(void)command(0);pause_ms(20);continue;}
        if(active || down)a04br3_hid_touch(0,active,lastx,lasty);
        if(active!=down)printf("A04BR3 TOUCH: %s raw=%u,%u z=%u panel=%u,%u\n",active?"down":"up",x,y,z,lastx,lasty);
        down=active;(void)command(0);pause_ms(20);
    }
}
void a04br3_touch_init(void){
    if(__atomic_exchange_n(&started,1,__ATOMIC_ACQ_REL))return;
    bus=ml_io_map(0x11009000,0x1000);gpio=ml_io_map(0x10005000,0x1000);top=ml_io_map(0x10000000,0x1000);infra=ml_io_map(0x10001000,0x1000);pwrap=ml_io_map(0x1000f000,0x1000);
    if(!bus || !gpio || !top || !infra || !pwrap){printf("A04BR3 TOUCH: MMIO map failed\n");return;}
    thread_t t=THREAD_NULL;kern_return_t e=kernel_thread_start(touch_thread,NULL,&t);if(e==KERN_SUCCESS)thread_deallocate(t);else printf("A04BR3 TOUCH: thread error=%d\n",e);
}
#endif
