# SPDX-License-Identifier: MIT
from pathlib import Path
import subprocess,tempfile,unittest
class PowerWrapper(unittest.TestCase):
 def test_real_power_sequence_and_bounded_failure(self):
  root=Path(__file__).resolve().parents[2];s=(root/'bsd/dev/a04br3_touch.c').read_text();body=s[s.index('static int pw_wait'):s.index('static int transfer')]
  test=r'''
#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
static unsigned pwrap,init=1,status,reads,delays,stuck,held;
/* Single-thread power-sequence fixture. Actual cross-client lock definitions
 * and 3-client contention are executed by test_pwrap_shared.py. */
void a04br3_pwrap_lock(void){assert(!held);held=1;}
void a04br3_pwrap_unlock(void){assert(held);held=0;}
static uint16_t voltage=0x1f,enable=0x41;
static uint32_t rd(unsigned b,unsigned o){assert(b==pwrap);reads++;assert(reads<5000);if(o==0x9c)return init;assert(o==0xa4);return stuck?0x20000:status;}
static void wr(unsigned b,unsigned o,uint32_t value){assert(b==pwrap&&held);
 if(o==0xa8){assert(value==1);status=0;return;}
 assert(o==0xa0);unsigned addr=((value>>16)&0x7fff)*2;assert(addr==0x530 || addr==0x50a);
 uint16_t *v=addr==0x530?&voltage:&enable;
 if(value&0x80000000){*v=(uint16_t)value;status=0;}else status=0x60000|*v;
}
static void delay(unsigned us){assert(us==10);delays++;}
static void pause_ms(unsigned ms){assert(ms==20&&!held);}
'''+body+r'''
int main(void){
 assert(power_on()==0);assert(voltage==0x7f && enable==0x8041);
 init=0;assert(power_on()==ENXIO);
 init=1;stuck=1;reads=delays=0;assert(power_on()==ETIMEDOUT);assert(delays==1000);
 return 0;
}
'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'test.c').write_text(test);subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all',str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)

 def test_actual_board_mmio_initialization(self):
  root=Path(__file__).resolve().parents[2];s=(root/'bsd/dev/a04br3_touch.c').read_text();body=s[s.index('void a04br3_touch_init(void){'):s.rindex('#endif')]
  source=r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef uintptr_t vm_offset_t;typedef int thread_t;typedef int kern_return_t;
#define THREAD_NULL 0
#define KERN_SUCCESS 0
static unsigned started,maps,launches;
static vm_offset_t bus,gpio,top,infra,pwrap;
static vm_offset_t ml_io_map(vm_offset_t p,unsigned n){assert(n==0x1000);maps++;return p;}
static void touch_thread(void *p,int r){(void)p;(void)r;}
static kern_return_t kernel_thread_start(void (*fn)(void*,int),void *arg,thread_t *t){assert(fn==touch_thread && !arg);launches++;*t=1;return 0;}
static void thread_deallocate(thread_t t){assert(t==1);}
'''+body+r'''
int main(void){a04br3_touch_init();assert(maps==5 && launches==1);
assert(bus==0x11009000 && gpio==0x10005000 && top==0x10000000 && infra==0x10001000 && pwrap==0x1000f000);
a04br3_touch_init();assert(maps==5 && launches==1);return 0;}
'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'test.c').write_text(source);subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
