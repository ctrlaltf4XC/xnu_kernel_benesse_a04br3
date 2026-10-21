# SPDX-License-Identifier: BSD-2-Clause-Patent
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class NativeGET(unittest.TestCase):
 def test_observed_power_and_driver_version(self):
  c=r'''
#include "A04BR3NativeGET.h"
#include <assert.h>
int main(){a04_network_state s={};uint8_t b[260];uint32_t n=sizeof(b);
memset(b,0x55,sizeof(b));assert(a04_native_get(&s,19,0,b,&n)==ENETDOWN && b[0]==0x55);
s.present=s.mac_valid=s.firmware_running=s.control_ready=1;s.mac[0]=2;
for(unsigned cap=0;cap<24;cap++){memset(b,0,sizeof(b));b[0]=1;n=cap;assert(a04_native_get(&s,19,0,b,&n)==EMSGSIZE && n==cap && b[0]==1);}
memset(b,0x55,sizeof(b));b[0]=1;b[1]=b[2]=b[3]=0;s.radio_enabled=1;n=sizeof(b);
assert(a04_native_get(&s,19,0,b,&n)==0 && n==24 && b[0]==1 && b[4]==1 && b[8]==1 && b[24]==0x55);
s.radio_enabled=0;n=24;assert(a04_native_get(&s,19,0,b,&n)==0 && b[8]==0);
b[0]=2;n=24;assert(a04_native_get(&s,19,0,b,&n)==EINVAL && b[0]==2);
n=260;assert(a04_native_get(&s,12,0,b,&n)==EOPNOTSUPP);assert(a04_native_get(&s,19,1,b,&n)==EOPNOTSUPP);
memset(b,0x55,sizeof(b));n=260;assert(a04_native_get(&s,43,0,b,&n)==0 && n==256 && b[255]==0 && b[256]==0x55);
assert(!strcmp((char *)b,"A04BR3 MT6630 source SDIO runtime"));return 0;}
'''
  with tempfile.TemporaryDirectory() as tmp:
   d=Path(tmp);(d/'test.cpp').write_text(c)
   subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(ROOT),str(d/'test.cpp'),'-o',str(d/'test')],check=True)
   subprocess.run([str(d/'test')],check=True)
if __name__=='__main__':unittest.main()
