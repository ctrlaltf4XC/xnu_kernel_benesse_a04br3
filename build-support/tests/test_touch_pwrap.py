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
static unsigned pwrap,init=1,status,reads,delays,stuck;
static uint16_t voltage=0x1f,enable=0x41;
static uint32_t rd(unsigned b,unsigned o){assert(b==pwrap);reads++;assert(reads<5000);if(o==0x9c)return init;assert(o==0xa4);return stuck?0x20000:status;}
static void wr(unsigned b,unsigned o,uint32_t value){assert(b==pwrap);
 if(o==0xa8){assert(value==1);status=0;return;}
 assert(o==0xa0);unsigned addr=((value>>16)&0x7fff)*2;assert(addr==0x530 || addr==0x50a);
 uint16_t *v=addr==0x530?&voltage:&enable;
 if(value&0x80000000){*v=(uint16_t)value;status=0;}else status=0x60000|*v;
}
static void delay(unsigned us){assert(us==10);delays++;}
static void pause_ms(unsigned ms){assert(ms==20);}
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
