# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Production EP0/FIFO/console execution; mock bus ACK is not hardware proof."""
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[2]
USB=ROOT/'iokit/Kernel/arm/usb'
C=r'''
#include <assert.h>
#include <string.h>
#include "A04BR3USBDevice.h"
static unsigned idx,ops,waits,pushed,pulled,complete,disconnects;
static unsigned dev=0x98,power=0x10,bus,txevent,rxevent;
static uint16_t ep[2][16],addr[2][2];static uint8_t sizes[2][2],input[2][512],output[2][512];
static unsigned pos[2],used[2],count[2],busy;
static uint8_t r8(void*,unsigned a){ops++;if(a==14)return idx;if(a==0x60)return dev;if(a==1)return power;if(a==10)return bus;
 if(a==0x20 || a==0x24){unsigned e=(a-0x20)/4;assert(e==idx&&pos[e]<count[e]);return input[e][pos[e]++];}return 0;}
static uint16_t r16(void*,unsigned a){ops++;if(a==2)return txevent;if(a==4)return rxevent;if(a==0x18)return (uint16_t)count[idx];
 if(a>=0x10 && a<0x20)return ep[idx][(a-0x10)/2];return 0;}
static void w8(void*,unsigned a,uint8_t v){ops++;if(a==14){assert(v<2);idx=v;return;}if(a==1){power=v;return;}if(a==10){bus&=~v;return;}
 if(a==0){ep[0][15]=v;return;}if(a==0x62||a==0x63){sizes[idx][a-0x62]=v;return;}
 if(a==0x20||a==0x24){unsigned e=(a-0x20)/4;assert(idx==e&&used[e]<512);output[e][used[e]++]=v;}}
static void w16(void*,unsigned a,uint16_t v){ops++;if(a==2){txevent&=~v;return;}if(a==4){rxevent&=~v;return;}
 if(a==0x64||a==0x66){addr[idx][(a-0x64)/2]=v;return;}
 if(a<0x10||a>=0x20)return;
 if(idx==0&&a==0x12){if(v&0x40){pos[0]=count[0]=0;}ep[0][1]=v&0x2e;return;}
 if(idx==1&&a==0x12){ep[1][1]=(v&0x2001);return;}
 if(idx==1&&a==0x16){ep[1][3]=v&1;return;}
 ep[idx][(a-0x10)/2]=v;}
static void bar(void*){}
static void wait(void*,uint32_t n){assert(n==20000);waits++;}
static int push(void*,const uint8_t*,unsigned n){if(busy)return 1;pushed+=n;return 0;}
static unsigned pull(void*,uint8_t *p,unsigned n){pulled++;for(unsigned i=0;i<n;i++)p[i]=(uint8_t)i;return n;}
static void done(void*){complete++;}static void lost(void*){disconnects++;}
static void setup(a04_usb_device *s,unsigned bm,unsigned req,unsigned value,unsigned index,unsigned len){
 memset(input[0],0,8);input[0][0]=bm;input[0][1]=req;input[0][2]=value;input[0][3]=value>>8;
 input[0][4]=index;input[0][5]=index>>8;input[0][6]=len;input[0][7]=len>>8;count[0]=8;pos[0]=used[0]=0;ep[0][1]=1;
 a04_usb_device_ep0(s,1);}
static unsigned frame(uint8_t *p,unsigned op,unsigned seq,const uint8_t *data,unsigned n){
 memset(p,0,512);a04_usb_put(p,0x34305541);p[4]=1;p[6]=op;a04_usb_put(p+8,seq);a04_usb_put(p+16,n);
 if(n)memcpy(p+32,data,n);a04_usb_put(p+20,~a04_usb_crc_update(~0U,p+32,n));a04_usb_put(p+24,a04_usb_header_crc(p));return 32+n;}
int main(){
 assert(a04_usb_device_start(nullptr,0,nullptr,nullptr,nullptr)==A04_MUSB_DISABLED&&!ops);
 a04_usb_device s={};a04_musb_io io={nullptr,0x11100000,0x1000,512,nullptr,r8,r16,w8,w16,bar};
 a04_usb_device_policy policy={0x0e8d,0x20ff,0x80,50};a04_usb_console_ops cb={nullptr,push,pull,done,lost};
 a04_usb_device_bind_console(&s,&cb);dev=0x9c;assert(a04_usb_device_start(&s,1,&io,&policy,wait)==A04_MUSB_NOT_READY&&!waits);dev=0x98;
 assert(!a04_usb_device_start(&s,1,&io,&policy,wait)&&s.active&&waits==1&&(power&0x40));
 assert(addr[1][0]==8&&addr[1][1]==72&&sizes[1][0]==6&&s.mps==512);
 setup(&s,0x80,6,0x100,0,18);assert(used[0]==18&&output[0][8]==0x8d&&output[0][10]==0xff&&output[0][15]==2);
 setup(&s,0x80,6,0x200,0,255);assert(used[0]==32&&output[0][22]==0&&output[0][23]==2);
 setup(&s,0x80,6,0x302,0x409,255);assert(used[0]==46&&output[0][2]=='A');
 setup(&s,0,5,17,0,0);assert(!s.address&&s.pending==1&&ep[0][15]==0);
 a04_usb_device_ep0(&s,1);assert(!s.address);ep[0][1]=0;a04_usb_device_ep0(&s,0);assert(!s.address);
 a04_usb_device_ep0(&s,1);assert(s.address==17&&ep[0][15]==17);
 setup(&s,0,9,1,0,0);assert(!s.configured);ep[0][1]=0;a04_usb_device_ep0(&s,1);assert(s.configured);
 setup(&s,0x81,0,0,0,2);assert(used[0]==2&&!output[0][0]);
 setup(&s,0x82,0,0,0x81,2);assert(used[0]==2&&!output[0][0]);
 setup(&s,0x82,0,0,0x82,2);assert(ep[0][1]&0x20);
 setup(&s,0,5,2,0,0);assert(ep[0][1]&0x20); // address forbidden configured
 setup(&s,0x40,1,0,0,0);assert(ep[0][1]&0x20); // no vendor control execution
 setup(&s,0,9,2,0,0);assert(ep[0][1]&0x20);
 uint8_t in[512],out[512],saved[512],one=42,want[4];unsigned n,written;
 n=frame(in,7,1,&one,1);busy=1;assert(!a04_usb_console_process(&s.session,&s.console,in,n,out,512,512,&written));assert(a04_usb_u32(out+32)==7&&s.session.next==1&&!pushed);
 busy=0;assert(!a04_usb_console_process(&s.session,&s.console,in,n,out,512,512,&written)&&written==48&&pushed==1&&s.session.next==2);
 memcpy(saved,out,written);assert(!a04_usb_console_process(&s.session,&s.console,in,n,out,512,512,&written)&&pushed==1&&!memcmp(saved,out,written));
 a04_usb_put(want,464);n=frame(in,8,2,want,4);assert(!a04_usb_console_process(&s.session,&s.console,in,n,out,512,512,&written)&&written==512&&pulled==1);
 assert(a04_usb_u32(out+20)==~a04_usb_crc_update(~0U,out+32,480));
 assert(!a04_usb_console_process(&s.session,&s.console,in,n,out,512,512,&written)&&pulled==1);
 for(unsigned z=0;z<32;z++)assert(a04_usb_console_process(&s.session,&s.console,in,z,out,512,512,&written)==1);
 n=frame(in,7,3,&one,1);in[32]^=1;assert(a04_usb_console_process(&s.session,&s.console,in,n,out,512,512,&written)==1&&pushed==1&&s.session.next==3);
 a04_usb_put(want,17);n=frame(in,8,3,want,4);assert(!a04_usb_console_process(&s.session,&s.console,in,n,out,512,64,&written)&&a04_usb_u32(out+32)==4&&pulled==1);
 n=frame(in,7,3,&one,1);a04_usb_put(in+12,1);a04_usb_put(in+24,a04_usb_header_crc(in));
 assert(!a04_usb_console_process(&s.session,&s.console,in,n,out,512,512,&written)&&a04_usb_u32(out+32)==4&&s.session.next==3);
 n=frame(in,7,3,&one,1);assert(a04_usb_console_process(&s.session,&s.console,in,n,in,512,512,&written)==1);
 assert(a04_usb_console_process(&s.session,&s.console,in,n,out,47,512,&written)==1);
 // Actual owner bulk FIFO path, ACK notification only AFTER host completion.
 n=frame(input[1],8,3,(const uint8_t*)"\x04\0\0\0",4);count[1]=n;pos[1]=used[1]=0;ep[1][1]=0x2000;ep[1][3]=1;
 assert(!a04_usb_device_bulk(&s,0)&&s.tx_pending&&used[1]==52&&!complete&&pulled==2);
 assert(a04_usb_device_bulk(&s,2)==A04_MUSB_AGAIN&&!complete);ep[1][1]=0x2000;
 assert(a04_usb_device_bulk(&s,0)==A04_MUSB_AGAIN&&!complete);
 assert(a04_usb_device_bulk(&s,2)==A04_MUSB_AGAIN&&complete==1&&!s.tx_pending);
 // Suspend may coalesce with the already completed IN event: drain must
 // observe that ACK without consuming any suspended OUT data.
 s.suspended=1;s.tx_pending=1;ep[1][1]=0x2000;ep[1][3]=0;
 assert(a04_usb_device_bulk(&s,2)==A04_MUSB_AGAIN&&complete==2&&!s.tx_pending);
 s.suspended=0;
 // Backpressure leaves unread OUT intact, oversize faults before FIFO access.
 ep[1][1]=0x2001;ep[1][3]=1;count[1]=513;pos[1]=0;assert(a04_usb_device_bulk(&s,0)==3&&!pos[1]);
 ep[1][1]=0x2000;assert(a04_usb_device_bulk(&s,0)==4&&!pos[1]);
 ep[1][3]=0;bus=4;power&=~0x10U;assert(a04_usb_device_poll(&s,1)==3&&!s.configured&&s.session.next==1&&s.mps==64&&addr[1][1]==16);
 assert(a04_usb_device_poll(&s,0)==2&&a04_usb_device_poll(&s,9)==2);
 unsigned before=disconnects;a04_usb_device_stop(&s);assert(!s.active&&!(power&0x40)&&disconnects==before+1);
 unsigned old=ops;a04_usb_device_stop(&s);assert(ops==old);
}
'''
class Device(unittest.TestCase):
 def test_production_mock_usb_and_console_negative(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'t.cpp').write_text(C)
   subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(USB),str(p/'t.cpp'),'-o',str(p/'t')],check=True)
   subprocess.run([str(p/'t')],check=True)
 def test_freestanding_arm64_actual_api(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'t.c').write_text('#include "A04BR3USBDevice.h"\nint start(struct a04_usb_device*s,const struct a04_musb_io*i,const struct a04_usb_device_policy*p,void(*w)(void*,uint32_t)){return a04_usb_device_start(s,1,i,p,w);}\nint poll(struct a04_usb_device*s){return a04_usb_device_poll(s,8);}\nvoid stop(struct a04_usb_device*s){a04_usb_device_stop(s);}\n')
   subprocess.run(['clang','--target=arm64-apple-ios16.0','-std=c11','-ffreestanding','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wframe-larger-than=4096','-I',str(USB),'-c',str(p/'t.c'),'-o',str(p/'t.o')],check=True)
if __name__=='__main__':unittest.main()
