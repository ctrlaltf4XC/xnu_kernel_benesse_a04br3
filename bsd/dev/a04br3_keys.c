/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Board keypad: hardware bit0=volume up, bit1=Home per a04br3.dts.
 * Existing LK pinmux/scan timing is retained. No GPIO41 USB-ID changes.
 */
#if defined(ARM64_BOARD_CONFIG_A04BR3)
#include <sys/systm.h>
#include <kern/thread.h>
#include <kern/sched_prim.h>
#include <kern/clock.h>
extern vm_offset_t ml_io_map(vm_offset_t,vm_size_t);
extern void a04br3_hid_key(int,int);
extern kern_return_t thread_terminate(thread_t);
void a04br3_keys_init(void);
static vm_offset_t keypad;
static unsigned started;
static int wait_chan;
static uint16_t kr(unsigned off){uint16_t v=*(volatile uint16_t *)(keypad+off);__asm__ volatile("dmb oshld":::"memory");return v;}
static void kw(unsigned off,uint16_t v){*(volatile uint16_t *)(keypad+off)=v;__asm__ volatile("dsb oshst":::"memory");}
static void key_thread(void *arg,wait_result_t result){
    (void)arg;(void)result;unsigned previous=0,candidate=0,stable=0,logged=0;
    kw(0x18,1024);kw(0x24,1);
    printf("A04BR3 KEYS: keypad enabled mem1=0x%x scan=0x%x sel=0x%x\n",kr(4),kr(0x1c),kr(0x20));
    for(;;){
        unsigned value=(~kr(4))&3;
        if(value==candidate)stable++;else{candidate=value;stable=0;}
        if(stable>=2 && candidate!=previous){
            unsigned changed=candidate^previous;
            if(changed&1)a04br3_hid_key(1,!!(candidate&1));
            if(changed&2)a04br3_hid_key(3,!!(candidate&2));
            if(logged++<64)printf("A04BR3 KEYS: volup=%u home=%u mem1=0x%x\n",!!(candidate&1),!!(candidate&2),kr(4));
            previous=candidate;
        }
        uint64_t deadline;clock_interval_to_deadline(10,NSEC_PER_MSEC,&deadline);
        assert_wait_deadline(&wait_chan,THREAD_UNINT,deadline);thread_block(THREAD_CONTINUE_NULL);
    }
}
void a04br3_keys_init(void){
    if(__atomic_exchange_n(&started,1,__ATOMIC_ACQ_REL))return;
    keypad=ml_io_map(0x10002000,0x1000);if(!keypad){printf("A04BR3 KEYS: map failed\n");return;}
    thread_t t=THREAD_NULL;kern_return_t e=kernel_thread_start(key_thread,NULL,&t);
    if(e==KERN_SUCCESS)thread_deallocate(t);else printf("A04BR3 KEYS: thread error=%d\n",e);
}
#endif
