# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Actual display/touch transaction functions and USB adapter share one lock."""
import pathlib,re,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
def function(text,name):
 m=re.search(r'\b'+name+r'\s*\([^;{}]*\)\s*\{',text)
 assert m,name
 start=text.rfind('\n',0,m.start())+1
 # Include return declaration from prior lines for multiline display functions.
 while start and text[start:m.start()].strip()=='': start=text.rfind('\n',0,start-1)+1
 b=text.index('{',m.start());depth=1;end=b+1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 # Return type intentionally supplied by the caller from actual signature body.
 return text[m.start():end]
class Shared(unittest.TestCase):
 def test_actual_transactions_contention_timeouts_and_unlock(self):
  display=(ROOT/'bsd/dev/a04br3_display.c').read_text();touch=(ROOT/'bsd/dev/a04br3_touch.c').read_text()
  lock=display[display.index('static LCK_GRP_DECLARE(a04pwrap_group'):display.index('static vm_offset_t display_system')]
  source=r'''
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <sched.h>
#include "A04BR3USBPwrap.h"
#define LCK_GRP_DECLARE(n,s) int n __attribute__((unused))
#define LCK_MTX_DECLARE(n,g) pthread_mutex_t n=PTHREAD_MUTEX_INITIALIZER
static _Thread_local int held;
static void lck_mtx_lock(pthread_mutex_t *m){assert(!held);assert(!pthread_mutex_lock(m));held=1;}
static int lck_mtx_try_lock(pthread_mutex_t *m){assert(!held);int error=pthread_mutex_trylock(m);if(!error)held=1;return !error;}
static void lck_mtx_unlock(pthread_mutex_t *m){assert(held);held=0;assert(!pthread_mutex_unlock(m));}
LOCK
static uint32_t fsm,data;static uint16_t pmic;
static unsigned fault,polls,writes;static uintptr_t pwrap;
static uint32_t readreg(unsigned o){assert(held);polls++;if(o==0x9c)return fault==4?0:1;if(o==0xa4)return ((fault==1?7:fsm)<<16)|data;assert(0);return 0;}
static void writereg(unsigned o,uint32_t v){assert(held);writes++;
 if(o==0xa0){unsigned addr=((v>>16)&0x7fff)<<1;
  if(v&0x80000000){if(addr==0x502)pmic=(uint16_t)(v|0x8000);fsm=0;}
  else {data=addr==0x502?pmic:addr^0x55;fsm=fault==2?3:6;}
 }else {assert(o==0xa8&&v==1);fsm=fault==3?3:0;}
 sched_yield();
}
static uint32_t rd(uintptr_t b,unsigned o){(void)b;return readreg(o);}
static void wr(uintptr_t b,unsigned o,uint32_t v){(void)b;writereg(o,v);}
static void delay(unsigned u){assert(held);assert(u==10);}
#define PWRAP_WACS2_RDATA 0xa4
#define PWRAP_WACS2_CMD 0xa0
#define PWRAP_WACS2_VLDCLR 0xa8
#define PWRAP_FSM_SHIFT 16
#define PWRAP_FSM_MASK 7
#define PWRAP_FSM_IDLE 0
#define PWRAP_FSM_WFVLDCLR 6
#define PWRAP_TIMEOUT_US 10000
#define BIT31 0x80000000u
static uint32_t MmioRead32(uintptr_t a){return readreg((unsigned)a);}
static void MmioWrite32(uintptr_t a,uint32_t v){writereg((unsigned)a,v);}
static void MicroSecondDelay(unsigned u){assert(held&&u==1);}
FUNCTIONS
static uint32_t ur(void *c,unsigned o){(void)c;return readreg(o);}
static void uw(void *c,unsigned o,uint32_t v){(void)c;writereg(o,v);}
static void ud(void *c,unsigned u){(void)c;assert(u==10||u==300);}
static int ul(void *c){(void)c;return a04br3_pwrap_try_lock();}
static void uu(void *c){(void)c;a04br3_pwrap_unlock();}
static struct a04_usb_pwrap_io usb={0,0x1000f000,0x1000,ur,uw,ud,ul,uu};
static void *worker(void *arg){uintptr_t kind=(uintptr_t)arg;for(unsigned i=0;i<200;i++){
 uint16_t value=0;int e;
 if(kind==0){assert(!pw_write(0x530,0x60));e=pw_read(0x530,&value);assert(!e&&value==(0x530^0x55));}
 else if(kind==1){assert(!PwrapWrite16(0x450,0x1234));e=PwrapRead16(0x450,&value);assert(!e&&value==(0x450^0x55));}
 else {e=a04_usb_pwrap_read16(&usb,0x502,&value);assert(e==0||e==16);}
 }return 0;}
int main(){
 pthread_t threads[3];for(uintptr_t i=0;i<3;i++)assert(!pthread_create(&threads[i],0,worker,(void *)i));for(unsigned i=0;i<3;i++)assert(!pthread_join(threads[i],0));
 assert(!a04_usb_pwrap_vusb_enable(&usb));assert((pmic&0xc000)==0xc000);
 // All timeout exits release the SAME lock; no spin clearing another response.
 for(fault=1;fault<=3;fault++){
  fsm=0;polls=0;uint16_t value=0xaaaa;
  assert(pw_read(0x530,&value)==ETIMEDOUT);assert(polls<=30000);assert(a04br3_pwrap_try_lock());a04br3_pwrap_unlock();
  fsm=0;polls=0;assert(PwrapRead16(0x450,&value)==ETIMEDOUT);assert(polls<=30000);assert(a04br3_pwrap_try_lock());a04br3_pwrap_unlock();
  fsm=0;polls=0;value=0xaaaa;assert(a04_usb_pwrap_read16(&usb,0x502,&value)==60);assert(value==0xaaaa&&polls<=3001);assert(a04br3_pwrap_try_lock());a04br3_pwrap_unlock();
 }
 fault=4;fsm=0;unsigned old=writes;uint16_t value=0xaaaa;
 assert(a04_usb_pwrap_read16(&usb,0x502,&value)==6&&value==0xaaaa&&writes==old);
 fault=0;a04br3_pwrap_lock();assert(!pthread_create(&threads[0],0,worker,(void *)2));assert(!pthread_join(threads[0],0));a04br3_pwrap_unlock();
 // Try-lock cannot touch MMIO while another owner holds the common lock.
 puts("PASS actual touch/display/USB shared transaction mutex: 3threads/600loops, PMIC actual adapter, 3 timeout phases, uninitialized, contended USB; no HW");
}
'''.replace('LOCK',lock)
  names=['pw_wait','pw_read_unlocked','pw_write_unlocked','pw_read','pw_write']
  funcs=['static int '+function(touch,n) for n in names]
  funcs+=['static int '+function(display,n) for n in ['PwrapWaitForFsm','PwrapRead16Unlocked','PwrapRead16','PwrapWrite16Unlocked','PwrapWrite16']]
  source=source.replace('FUNCTIONS','\n'.join(funcs))
  with tempfile.TemporaryDirectory() as d:
   p=pathlib.Path(d);(p/'test.c').write_text(source)
   subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-pthread','-I',str(ROOT/'iokit/Kernel/arm/usb'),str(p/'test.c'),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],check=True,timeout=10)
 def test_arm64_actual_display_touch_transaction_sources(self):
  display=(ROOT/'bsd/dev/a04br3_display.c').read_text();touch=(ROOT/'bsd/dev/a04br3_touch.c').read_text()
  text='#include <stdint.h>\n#include "a04br3_pwrap_lock.h"\n#define ETIMEDOUT 60\n#define BIT31 0x80000000u\n'
  text+='typedef uintptr_t vm_offset_t; static vm_offset_t pwrap;\nextern uint32_t rd(vm_offset_t,unsigned);extern void wr(vm_offset_t,unsigned,uint32_t);extern void delay(unsigned);\n'
  text+='extern uint32_t MmioRead32(uintptr_t);extern void MmioWrite32(uintptr_t,uint32_t);extern void MicroSecondDelay(unsigned);\n'
  text+='#define PWRAP_WACS2_RDATA 0xa4\n#define PWRAP_WACS2_CMD 0xa0\n#define PWRAP_WACS2_VLDCLR 0xa8\n#define PWRAP_FSM_IDLE 0\n#define PWRAP_FSM_WFVLDCLR 6\n#define PWRAP_FSM_MASK 7\n#define PWRAP_FSM_SHIFT 16\n#define PWRAP_TIMEOUT_US 10000\n'
  for src,names in [(touch,['pw_wait','pw_read_unlocked','pw_write_unlocked','pw_read','pw_write']),(display,['PwrapWaitForFsm','PwrapRead16Unlocked','PwrapRead16','PwrapWrite16Unlocked','PwrapWrite16'])]:
   for n in names:text+='static int '+function(src,n)+'\n'
  text+='int run(uint16_t *v){return pw_read(0x530,v)+pw_write(0x50a,1)+PwrapRead16(0x450,v)+PwrapWrite16(0x450,1);}\n'
  with tempfile.TemporaryDirectory() as d:
   p=pathlib.Path(d);(p/'test.c').write_text(text)
   subprocess.run(['clang','-target','aarch64-none-elf','-ffreestanding','-std=c11','-Wall','-Wextra','-Werror','-I',str(ROOT/'bsd/dev'),'-c',str(p/'test.c'),'-o',str(p/'test.o')],check=True)
if __name__=='__main__':unittest.main()
