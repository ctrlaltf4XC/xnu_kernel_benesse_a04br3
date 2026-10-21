# SPDX-License-Identifier: MIT
from pathlib import Path
import subprocess,tempfile,unittest
class TouchProtocol(unittest.TestCase):
 def test_adc_release_noise_and_portrait_coordinates(self):
  inc=Path(__file__).resolve().parents[2]/'bsd/dev'
  source=r'''
#include <stdint.h>
#include <assert.h>
#include "a04br3_touch_protocol.h"
int main(void){
 unsigned x=33,y=44;
 assert(a04br3_touch_adc(0xab,0xc0)==2748);
 assert(a04br3_touch_adc(0xff,0xff)==4095);
 assert(a04br3_touch_point(2048,2048,0,&x,&y)==0 && x==33 && y==44);
 assert(a04br3_touch_point(2048,2048,4095,&x,&y)==0);
 assert(a04br3_touch_point(4096,0,100,&x,&y)==-1);
 assert(a04br3_touch_point(4095,2048,6,&x,&y)==-1);
 assert(a04br3_touch_point(2048,2048,2048,&x,&y)==1 && x==405 && y==641);
 assert(a04br3_touch_point(0,0,3000,&x,&y)==1 && x==799 && y==1279);
 assert(a04br3_touch_point(4095,4095,3000,&x,&y)==1 && x==0 && y==0);
 assert(a04br3_touch_point(150,320,200,&x,&y)==1 && x==799 && y==1279);
 assert(a04br3_touch_point(3950,3830,2300,&x,&y)==1 && x==0 && y==0);
 assert(a04br3_touch_point(148,3830,192,&x,&y)==1 && x==0 && y==1279);
 return 0;
}
'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'test.c').write_text(source);subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(inc),str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
