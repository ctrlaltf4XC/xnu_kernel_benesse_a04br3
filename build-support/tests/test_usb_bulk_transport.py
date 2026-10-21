# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Actual dormant MUSB PIO + CRC processor, typed MMIO model; no USB device I/O."""
from pathlib import Path
import ctypes,importlib.util,io,subprocess,tempfile,unittest,zlib
ROOT=Path(__file__).resolve().parents[2]
USB=ROOT/'iokit/Kernel/arm/usb'
HOST=ROOT.parent/'device/tools/usb_bulk_probe.py'
C=r'''
#include <assert.h>
#include <string.h>
#include "A04BR3MUSBPIO.h"
static unsigned ops,reads,writes,prepared,barriers,fifo_read,fifo_write;
static uint8_t ep_index=7,devctl=0x98,power=0x10,in[512],out[64];
static uint16_t tx=0x2000,rx,count,maxpacket=512;
static int ready(void *){prepared++;return 0;}
static uint8_t r8(void *,unsigned a){ops++;reads++;if(a==0x60)return devctl;if(a==1)return power;if(a==0xe)return ep_index;assert(a==0x24&&ep_index==1&&fifo_read<count);return in[fifo_read++];}
static uint16_t r16(void *,unsigned a){ops++;reads++;assert(ep_index==1);if(a==0x10||a==0x14)return maxpacket;if(a==0x12)return tx;if(a==0x16)return rx;assert(a==0x18);return count;}
static void w8(void *,unsigned a,uint8_t v){ops++;writes++;if(a==0xe){ep_index=v;return;}assert(a==0x24&&ep_index==1&&fifo_write<64);out[fifo_write++]=v;}
static void w16(void *,unsigned a,uint16_t v){ops++;writes++;assert(ep_index==1);
 if(a==0x16){assert((v&0x45)==0x44&&fifo_read==count);rx&=~1U;return;}
 assert(a==0x12&&fifo_write==64&&(v&0xa7)==0xa7&&(v&0x2000));tx=0x2001;
}
static void barrier(void *){barriers++;}
static a04_musb_io io={nullptr,0x11100000,0x1000,512,ready,r8,r16,w8,w16,barrier};
static unsigned frame(unsigned op,unsigned seq,unsigned offset,const uint8_t *data,unsigned n){
 memset(in,0,sizeof(in));a04_usb_put(in,0x34305541);in[4]=1;in[6]=(uint8_t)op;
 a04_usb_put(in+8,seq);a04_usb_put(in+12,offset);a04_usb_put(in+16,n);if(n)memcpy(in+32,data,n);
 a04_usb_put(in+20,~a04_usb_crc_update(~0U,in+32,n));a04_usb_put(in+24,a04_usb_header_crc(in));return n+32;
}
static void reply(unsigned op,unsigned seq,unsigned offset,const uint8_t *data,unsigned n,a04_musb_pio *s,unsigned status=0){
 count=(uint16_t)frame(op,seq,offset,data,n);fifo_read=fifo_write=0;rx=1;tx=0x2000;
 assert(!a04_musb_poll(s)&&ep_index==7&&fifo_read==count&&fifo_write==64&&!(rx&1)&&tx==0x2001);
 assert(a04_usb_u32(out+32)==status&&a04_usb_u32(out+24)==a04_usb_header_crc(out));
 assert(a04_usb_u32(out+20)==~a04_usb_crc_update(~0U,out+32,32));
}
int main(){
 assert(~a04_usb_crc_update(~0U,(const uint8_t *)"123456789",9)==0xcbf43926);
 assert(a04_musb_start(nullptr,0,nullptr)==A04_MUSB_DISABLED&&!ops&&!prepared);
 a04_musb_pio s={};assert(a04_musb_poll(&s)==A04_MUSB_DISABLED&&!ops&&!prepared);
 for(unsigned v=0;v<3;v++){if(v==1)continue;assert(a04_musb_start(&s,v,&io)==A04_MUSB_DISABLED&&!ops&&!prepared);}
 auto invalid=io;invalid.physical=0x11200000;assert(a04_musb_start(&s,1,&invalid)==A04_MUSB_NOT_READY&&!ops&&!prepared);
 devctl=0x9c;assert(a04_musb_start(&s,1,&io)==A04_MUSB_NOT_READY&&!writes);devctl=0x98;
 assert(!a04_musb_start(&s,1,&io)&&s.active&&s.session.next==1&&ep_index==7);
 reply(1,1,0,nullptr,0,&s);reply(2,2,0,nullptr,0,&s);
 uint8_t begin[8];a04_usb_put(begin,9);a04_usb_put(begin+4,0xcbf43926);
 reply(3,3,0,begin,8,&s);reply(4,4,0,(const uint8_t *)"123456789",9,&s);
 assert(s.session.received==9&&s.session.active);reply(5,5,9,nullptr,0,&s);assert(!s.session.active);
 reply(7,6,0,nullptr,0,&s,A04_USB_UNSUPPORTED);assert(s.session.next==6); // no flash/commit opcode
 reply(4,6,0,(const uint8_t *)"x",1,&s,A04_USB_BAD_STATE);
 reply(1,99,0,nullptr,0,&s,A04_USB_BAD_SEQUENCE);assert(s.session.next==6);
 tx=0x2001;rx=1;count=32;fifo_read=fifo_write=0;
 assert(a04_musb_poll(&s)==A04_MUSB_AGAIN&&!fifo_read&&!fifo_write&&ep_index==7&&(rx&1));
 tx=0x2000;count=(uint16_t)frame(1,6,0,nullptr,0);in[0]^=1;fifo_read=fifo_write=0;
 auto before=s.session;assert(a04_musb_poll(&s)==A04_MUSB_BAD_FRAME&&fifo_write==0&&ep_index==7&&!(rx&1)&&!memcmp(&before,&s.session,sizeof(before)));
 rx=1;count=513;fifo_read=fifo_write=0;assert(a04_musb_poll(&s)==A04_MUSB_FAULT&&!s.active&&!fifo_read&&!fifo_write&&ep_index==7);
 unsigned old=ops;assert(a04_musb_poll(&s)==A04_MUSB_DISABLED&&ops==old);
 for(unsigned fault=0;fault<4;fault++){
  tx=0x2000;rx=0;assert(!a04_musb_start(&s,1,&io));
  if(fault==0)tx|=0x1000;if(fault==1)rx|=0x2000;if(fault==2)rx|=4;if(fault==3)tx|=0x80;
  fifo_read=fifo_write=0;assert(a04_musb_poll(&s)==A04_MUSB_FAULT&&!s.active&&!fifo_read&&!fifo_write&&ep_index==7);
 }
 // Protocol failure preflight is atomic for every externally short buffer.
 a04_usb_session state;a04_usb_session_reset(&state);uint8_t response[64],saved[64];
 memset(response,0xa5,64);memcpy(saved,response,64);frame(1,1,0,nullptr,0);
 for(unsigned n=0;n<32;n++){assert(a04_usb_bulk_process(&state,in,n,response,64,512)==A04_USB_BAD_FRAME);assert(state.next==1&&!memcmp(response,saved,64));}
 for(unsigned n=0;n<64;n++)assert(a04_usb_bulk_process(&state,in,32,response,n,512)==A04_USB_BAD_FRAME);
 assert(a04_usb_bulk_process(&state,in,513,response,64,512)==A04_USB_BAD_FRAME);
 assert(a04_usb_bulk_process(&state,in,32,in,64,512)==A04_USB_BAD_FRAME);
 for(unsigned byte=0;byte<32;byte++){
  frame(1,1,0,nullptr,0);in[byte]^=1;
  assert(a04_usb_bulk_process(&state,in,32,response,64,512)==A04_USB_BAD_FRAME&&state.next==1&&!memcmp(response,saved,64));
 }
 // Stop/default paths never write PHY/clock/POWER/DEVCTL/IRQ/softconnect.
 old=ops;a04_musb_stop(&s);assert(ops==old&&!s.active);
}
'''
class USBBulk(unittest.TestCase):
 def test_actual_pio_protocol_failures_and_no_default_mmio(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'test.cpp').write_text(C)
   subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(USB),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],check=True)
 def test_host_client_against_actual_c_processor(self):
  spec=importlib.util.spec_from_file_location('usb_probe',HOST);host=importlib.util.module_from_spec(spec);spec.loader.exec_module(host)
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'bridge.c').write_text('#include "A04BR3USBBulk.h"\nstatic struct a04_usb_session state;\nvoid reset(void){a04_usb_session_reset(&state);}\nint run(const void *p,unsigned n,void *r,unsigned mps){return a04_usb_bulk_process(&state,p,n,r,64,mps);}\n')
   subprocess.run(['cc','-std=c11','-shared','-fPIC','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I',str(USB),str(p/'bridge.c'),'-o',str(p/'bridge.so')],check=True)
   lib=ctypes.CDLL(str(p/'bridge.so'));lib.run.argtypes=[ctypes.c_void_p,ctypes.c_uint,ctypes.c_void_p,ctypes.c_uint]
   for mps in (64,512):
    lib.reset()
    def exchange(frame):
     result=ctypes.create_string_buffer(64);self.assertEqual(lib.run(frame,len(frame),result,mps),0);return result.raw
    client=host.Client(exchange,mps);client.request(1);client.request(2)
    data=bytes((n*17)&255 for n in range(5003));result=client.crc_stream(io.BytesIO(data))
    self.assertEqual(result['received'],len(data));self.assertEqual(result['crc'],zlib.crc32(data));self.assertFalse(result['active'])
    with self.assertRaises(ValueError):client.crc_stream(io.BytesIO(b''))
    with self.assertRaises(RuntimeError):client.request(7)
    with self.assertRaises(ValueError):host.decode_reply(bytes(64),1,1)
 def test_actual_arm64_leaf_compile(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'call.c').write_text('#include "A04BR3MUSBPIO.h"\nint start(struct a04_musb_pio *s,unsigned e,const struct a04_musb_io *io){return a04_musb_start(s,e,io);}\nint poll(struct a04_musb_pio *s){return a04_musb_poll(s);}\n')
   subprocess.run(['clang','--target=arm64-apple-ios16.0','-std=c11','-ffreestanding','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wframe-larger-than=4096','-I',str(USB),'-c',str(p/'call.c'),'-o',str(p/'arm.o')],check=True)
if __name__=='__main__':unittest.main()
