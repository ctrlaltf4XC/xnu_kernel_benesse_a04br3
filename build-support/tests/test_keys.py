# SPDX-License-Identifier: MIT
from pathlib import Path
import subprocess,tempfile,unittest
class Keys(unittest.TestCase):
 def test_active_low_home_volume_and_bounce(self):
  root=Path(__file__).resolve().parents[2];s=(root/'bsd/dev/a04br3_keys.c').read_text();body=s[s.index('static void key_thread'):s.index('void a04br3_keys_init(void){')]
  source=r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <setjmp.h>
typedef int wait_result_t;
#define NSEC_PER_MSEC 1000000
#define THREAD_UNINT 0
#define THREAD_CONTINUE_NULL 0
static int wait_chan;
static jmp_buf done;
static unsigned index_,count;
static const uint16_t trace[]={0xffff,0xfffd,0xfffd,0xffff,0xfffd,0xfffd,0xfffd,0xfffd,0xffff,0xffff,0xffff,0xfffe,0xfffe,0xfffe,0xffff,0xffff,0xffff};
static const int expected[][2]={{3,1},{3,0},{1,1},{1,0}};
static uint16_t kr(unsigned o){if(o==4)return trace[index_];assert(o==0x1c||o==0x20);return 0;}
static void kw(unsigned o,uint16_t v){assert((o==0x18&&v==1024)||(o==0x24&&v==1));}
static void a04br3_hid_key(int key,int down){assert(count<4);assert(key==expected[count][0]&&down==expected[count][1]);count++;}
static void clock_interval_to_deadline(unsigned n,unsigned scale,uint64_t *d){assert(n==10&&scale==1000000);*d=1;}
static void assert_wait_deadline(void *p,int flag,uint64_t d){assert(p==&wait_chan&&flag==0&&d==1);}
static void thread_block(int c){assert(c==0);if(++index_==sizeof(trace)/sizeof(trace[0]))longjmp(done,1);}
'''+body+r'''
int main(void){if(!setjmp(done))key_thread(0,0);assert(count==4);return 0;}
'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'test.c').write_text(source);subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all',str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
