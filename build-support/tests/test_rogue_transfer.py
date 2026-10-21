# SPDX-License-Identifier: BSD-2-Clause-Patent
from pathlib import Path
import subprocess,tempfile,unittest
GPU=Path(__file__).resolve().parents[2]/'iokit/Kernel/arm/rogue'
class TransferABI(unittest.TestCase):
 def test_original208_encoder_bounds_tail_and_task_envelope(self):
  code=r'''
#include <assert.h>
#include <string.h>
#include "A04BR3RogueTransferABI.h"
int main(){
 unsigned char record[0xe8]={};alignas(8) unsigned char payload[208],packet[248],saved[248];
 record[0x1c]=2;record[0x10]=8;
 record[0x14]=0x11;record[0x15]=0x22;record[0x16]=0x33;record[0x17]=0x44;
 for(unsigned n=0;n<200;n++)record[0x20+n]=(unsigned char)(n+17);
 for(unsigned hint=0;hint<8;hint++){
  assert(!a04_rgx_tq_encode_record(record,sizeof(record),hint,payload,sizeof(payload)));
  assert(!memcmp(payload,record+0x20,200) && a04_rgx_fw_u32(payload+0xc8)==0x44332211);
  assert(a04_rgx_fw_u32(payload+0xcc)==(1U|(hint<<3)));
  assert(!a04_rgx_tq_encode_packet(payload,208,packet,248));
  auto h=(RGXFWIF_CCB_CMD_HEADER*)packet;
  assert(h->eCmdType==0x2abc80cc && h->ui32CmdSize==208 && !h->ui32IntJobRef);
  assert(!memcmp(packet+40,payload,208));
 }
 memset(payload,0xa5,sizeof(payload));
 for(unsigned bytes=0;bytes<sizeof(record);bytes++){
  assert(a04_rgx_tq_encode_record(record,bytes,0,payload,208));
  for(auto c:payload)assert(c==0xa5);
 }
 assert(a04_rgx_tq_encode_record(nullptr,0xe8,0,payload,208));
 assert(a04_rgx_tq_encode_record(record,0xe8,8,payload,208));
 assert(a04_rgx_tq_encode_record(record,0xe8,0,payload,207));
 record[0x1c]=1;assert(a04_rgx_tq_encode_record(record,0xe8,0,payload,208));record[0x1c]=2;
 assert(a04_rgx_tq_encode_record(record,0xe8,0,record,208));
 memset(packet,0x5a,sizeof(packet));memcpy(saved,packet,sizeof(packet));
 assert(a04_rgx_tq_encode_packet(payload,208,packet,248)); // invalid producer flags
 assert(!memcmp(packet,saved,248));
 assert(a04_rgx_tq_encode_packet(payload,207,packet,248));
 assert(a04_rgx_tq_encode_packet(payload,208,packet,247));
 assert(a04_rgx_tq_encode_packet(packet,208,packet,248));
}
'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'test.cpp').write_text(code)
   subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(GPU),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],check=True)
