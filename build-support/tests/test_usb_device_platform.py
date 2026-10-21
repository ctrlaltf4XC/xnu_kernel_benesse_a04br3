# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Real source helper with MMIO/PMIC callbacks; no hardware readiness claim."""
import pathlib,subprocess,tempfile,unittest
HEADER=pathlib.Path(__file__).resolve().parents[2]/'iokit/Kernel/arm/usb'
CODE=r'''
#include "A04BR3USBDevicePlatform.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
struct Mock {unsigned char mem[3][4096];unsigned writes,reads,supply,delays,total_us;int clock_stuck,phy_stuck,supply_error,pmic_error,pmic_stuck,pmic_write_error;uint16_t pmic;unsigned pmic_writes;};
static uint32_t get(Mock *m,unsigned r,unsigned o){uint32_t v;memcpy(&v,m->mem[r]+o,4);return v;}
static void put(Mock *m,unsigned r,unsigned o,uint32_t v){memcpy(m->mem[r]+o,&v,4);}
static uint8_t r8(void *c,unsigned r,unsigned o){auto *m=(Mock *)c;assert(r<3&&o<4096);m->reads++;return m->mem[r][o];}
static uint32_t r32(void *c,unsigned r,unsigned o){auto *m=(Mock *)c;assert(r<3&&o+4<=4096&&!(o&3));m->reads++;return get(m,r,o);}
static void w8(void *c,unsigned r,unsigned o,uint8_t v){auto *m=(Mock *)c;assert(r<2&&o<4096);m->writes++;
 if(r==0){assert(o==1||o==0x60);if(o==1)assert(!(v&0x4c));else assert(!(v&3));}
 m->mem[r][o]=v;
}
static void w32(void *c,unsigned r,unsigned o,uint32_t v){auto *m=(Mock *)c;m->writes++;
 if(r==2){assert(o==0x9c||o==0x84);assert(v==(o==0x9c?0x81004000u:0x2000u));if(!m->clock_stuck)put(m,2,o==0x9c?0x3c:0x24,get(m,2,o==0x9c?0x3c:0x24)&~v);}
 else {assert(r==1&&(o==0x68||o==0x6c));if(o==0x6c)assert((v&0x3f3f)==0x202);if(!m->phy_stuck)put(m,r,o,v);}
}
static void barrier(void *){}
static void delay(void *c,unsigned us){auto *m=(Mock *)c;assert(us==50||us==800||us==300);m->delays++;m->total_us+=us;}
static int supply(void *c){auto *m=(Mock *)c;m->supply++;return m->supply_error;}
static int pr(void *c,unsigned a,uint16_t *v){auto *m=(Mock *)c;assert(a==0x502);if(m->pmic_error)return m->pmic_error;*v=m->pmic;return 0;}
static int pw(void *c,unsigned a,uint16_t v){auto *m=(Mock *)c;assert(a==0x502&&v==(uint16_t)(m->pmic|0x4000));m->pmic_writes++;if(m->pmic_write_error)return 110;if(!m->pmic_stuck)m->pmic=(uint16_t)(v|0x8000);return 0;}
static a04_usb_platform_io io(Mock *m){return {m,0x11100000,0x1000,0x11110800,0x800,0x10000000,0x1000,r8,w8,r32,w32,barrier,delay,supply};}
static Mock initial(){Mock m={};put(&m,2,0x24,0x12345678);put(&m,2,0x3c,0xffffffff);put(&m,1,0x68,0xffffffff);put(&m,1,0x6c,0xffffffff);m.mem[0][0x60]=0x99;m.mem[0][1]=0x7f;return m;}
int main(){unsigned cases=0;a04_usb_platform_result result={};
 for(unsigned k=0;k<12;k++){auto m=initial();auto v=io(&m);auto old=result;
  switch(k){case0:break;default:break;}
  // Invalid mapping/callback preflight must have no MMIO or supply effect.
  switch(k){case 0:v.mac_physical++;break;case 1:v.mac_span--;break;case 2:v.phy_physical++;break;case 3:v.phy_span--;break;case 4:v.top_physical++;break;case 5:v.top_span--;break;case 6:v.read8=nullptr;break;case 7:v.write8=nullptr;break;case 8:v.read32=nullptr;break;case 9:v.write32=nullptr;break;case 10:v.delay_us=nullptr;break;case 11:v.vusb_enable_verified=nullptr;break;}
  assert(a04_usb_device_prepare(&v,&result)==A04_USB_PLATFORM_INVALID);assert(!m.writes&&!m.reads&&!m.supply&&!memcmp(&result,&old,sizeof(result)));cases++;
 }
 {auto m=initial();auto v=io(&m);assert(!a04_usb_device_prepare(&v,&result));assert(m.supply==1&&m.total_us==850&&result.stage==4);assert(m.mem[0][0x60]==0x98&&!(m.mem[0][1]&0x4c));assert((get(&m,1,0x6c)&0x3f3f)==0x202);unsigned writes=m.writes;assert(!a04_usb_device_observe(&v,&result)&&m.writes==writes);cases++;}
 for(unsigned k=0;k<6;k++){auto m=initial();auto v=io(&m);int expected=0;
  switch(k){case 0:m.supply_error=110;expected=A04_USB_PLATFORM_POWER;break;case 1:m.clock_stuck=1;expected=A04_USB_PLATFORM_CLOCK;break;case 2:m.phy_stuck=1;expected=A04_USB_PLATFORM_PHY;break;case 3:m.mem[0][0x60]=0x80;expected=A04_USB_PLATFORM_NO_VBUS;break;case 4:m.mem[0][0x60]=0x9c;expected=A04_USB_PLATFORM_ROLE;break;case 5:m.mem[0][0x60]=0x18;expected=A04_USB_PLATFORM_ROLE;break;}
  assert(a04_usb_device_prepare(&v,&result)==expected);if(k==0)assert(!m.reads&&!m.writes&&result.supply_error==110);if(k==1)assert(m.writes==2);cases++;
 }
 {auto m=initial();auto v=io(&m);assert(!a04_usb_device_prepare(&v,&result));put(&m,1,0x6c,get(&m,1,0x6c)|0x2000);assert(a04_usb_device_observe(&v,&result)==A04_USB_PLATFORM_PHY);cases++;}
 for(unsigned k=0;k<6;k++){Mock m={};m.pmic=0x1234;a04_usb_vusb_io p={&m,pr,pw,delay};
  if(k==1)m.pmic|=0xc000;if(k==2)m.pmic_error=110;if(k==3)m.pmic_stuck=1;
  if(k==4){m.pmic|=0x4000;}if(k==5)m.pmic_write_error=1;
  int error=a04_usb_vusb_enable(&p);
  if(k<2){assert(!error&&(m.pmic&0x4000));assert(m.pmic_writes==(k==0));assert(m.total_us==(k==0?300u:0u));}
  else assert(error==((k==2||k==5)?110:A04_USB_PLATFORM_POWER));cases++;
 }
 printf("PASS %u actual source MMIO/PMIC cases; no hardware/physical power claim\n",cases);
}
'''.replace('  switch(k){case0:break;default:break;}\n','')
class Platform(unittest.TestCase):
 def test_actual_source_sequence_and_failures(self):
  with tempfile.TemporaryDirectory() as d:
   p=pathlib.Path(d);(p/'test.cpp').write_text(CODE)
   subprocess.run(['clang','-x','c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(HEADER),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],check=True)
 def test_arm64_freestanding_header(self):
  with tempfile.TemporaryDirectory() as d:
   p=pathlib.Path(d);(p/'a.c').write_text('#include "A04BR3USBDevicePlatform.h"\n#include "A04BR3USBPwrap.h"\nint check(const struct a04_usb_platform_io *i,struct a04_usb_platform_result *r){return a04_usb_device_prepare(i,r);}\n')
   subprocess.run(['clang','-target','aarch64-none-elf','-ffreestanding','-std=c11','-Wall','-Wextra','-Werror','-I',str(HEADER),'-c',str(p/'a.c'),'-o',str(p/'a.o')],check=True)
if __name__=='__main__':unittest.main()
