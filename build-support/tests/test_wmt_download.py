# SPDX-License-Identifier: MIT
"""Execute native-order patch startup against destructive SDIO/ACK model."""
from pathlib import Path
import json
import re
import unittest
import test_wmt_protocol as codecs

MODEL = r'''
#include "A04BR3WMTDownload.h"
#include <stdlib.h>
#include <stdio.h>
static struct a04_wmt_channel channel;
static struct a04_wmt_download_state state;
static struct a04_wmt_firmware_set files;
static unsigned calls,ack_reads,resets,step,credit,received,delay_count,part,offset,fragments;
static unsigned address_step,strap_seen,merge_seen,coex_seen,clock_seen,version_seen,function_enabled,common_enables;
static unsigned operations[256];
static unsigned fault_at,fault,fn1_timeout,wrong_hw;
static uint8_t response[32],rx[64];static unsigned response_length,rx_length;
static const struct a04_wmt_identity identity={0x6630,0x8a12,0x8a11};
static void delay(void *p,unsigned us){(void)p;assert(us==1000);assert(++delay_count<2000);}
static int direct(void *p,unsigned op,uint32_t arg,unsigned resp,uint32_t *r){
 (void)p;assert(op==52 && resp==1);unsigned fn=(arg>>28)&7,reg=(arg>>9)&0x1ffff;*r=0;
 if(fn==2){assert(reg==4 && (arg>>31) && (arg&255)==1);common_enables++;return 0;}
 assert(fn==0);assert(resets==files.count+1 && strap_seen && version_seen==2);
 if(reg==2){if(arg>>31){assert((arg&255)==6);function_enabled=1;}else *r=4;}
 else if(reg==3){if(!fn1_timeout)*r=6;}
 else {assert((reg==0x110 || reg==0x111) && (arg>>31));}
 return 0;
}
static void build_response(const uint8_t *c,unsigned length){
 assert(calls<=256);operations[calls-1]=c[1];
 memset(response,0,sizeof(response));response[0]=2;response[1]=c[1];response[2]=1;response_length=5;
 if(c[1]==8 && c[4]==1){
  if(c[7]==3){
   assert(resets==files.count+1 && length==44);merge_seen++;
   const uint8_t expected[44]={1,8,40,0,1,1,0,3,0x6c,0x50,2,0x80,0,0,0x10,0x11,0,0,0xf0,0xff,0x70,0x50,2,0x80,1,0,0,0,15,0,0,0,0,0x53,2,0x80,4,0,0,0,4,0,0,0};
   assert(!memcmp(c,expected,44));response[7]=3;
  }else{
   assert(part<files.count && resets==part && length==20 && c[5]==1 && c[6]==0 && c[7]==1);
   uint32_t address=a04_wmt_get32(c+8),value=a04_wmt_get32(c+12);
   assert(a04_wmt_get32(c+16)==0xffffffffu);
   if(!address_step){assert(address==0x020903d4 && !value);address_step=1;}
   else {assert(address_step==1 && address==0x020908fc && value==files.files[part].destination);address_step=2;}
   response[7]=1;
  }
  response[2]=4;response_length=8;
 }else if(c[1]==1){
  assert(part<files.count && address_step==2 && resets==part && length>=6);
  unsigned n=files.files[part].header.body.length-offset;if(n>1000)n=1000;
  assert(length==n+5 && a04_wmt_le16(c+2)==n+1);
  unsigned flag=offset+n==files.files[part].header.body.length?3:(offset?2:1);assert(c[4]==flag);
  assert(!memcmp(c+5,files.files[part].header.body.data+offset,n));offset+=n;fragments++;
 }else if(c[1]==7){
  assert(length==5 && c[4]==4);
  if(part<files.count){assert(offset==files.files[part].header.body.length && address_step==2);part++;offset=0;address_step=0;}
  else assert(resets==files.count);
  resets++;
 }else if(c[1]==0x10){assert(resets==files.count+1 && length==6 && c[4]==1 && c[5]==1);coex_seen++;}
 else if(c[1]==0x0a){assert(resets==files.count+1 && length==6 && c[4]==8 && c[5]==3);clock_seen++;}
 else if(c[1]==5){assert(resets==files.count+1 && length==6 && c[4]==2 && c[5]==2);strap_seen++;response[2]=2;response[5]=2;response_length=6;}
 else if(c[1]==8 && c[4]==2){
  assert(strap_seen && length==20 && version_seen<2);uint32_t address=a04_wmt_get32(c+8);
  assert(address==(version_seen?0x80000004u:0x80000000u));version_seen++;
  response[2]=12;response[7]=1;response_length=16;
  a04_wmt_put32(response+8,address);a04_wmt_put32(response+12,address==0x80000000u?(wrong_hw?0x8a13:0x8a12):0x8a11);
 }else assert(0); /* no UART STP switch and no invented FUNC_CTRL03 command */
 if(calls==fault_at && fault==2)response[4]=1;
 if(calls==fault_at && fault==4){response[2]++;response[response_length++]=0x33;}
 memset(rx,0,sizeof(rx));unsigned n=0;
 assert(!a04_stp_encode(0,4,0,0,0,response,response_length,rx+4,sizeof(rx)-4,&n));
 rx_length=4+((n+3)&~3u);a04_wmt_put16(rx,rx_length);
}
static int transfer(void *p,uint32_t arg,void *buffer,unsigned length){
 (void)p;assert(((arg>>28)&7)==2);unsigned reg=(arg>>9)&0x1ffff;uint8_t *b=(uint8_t *)buffer;
 if(arg>>31){
  if(reg==0x14)return 0;
  assert(reg==0x18 && !(arg&(1u<<26)));calls++;step=credit=received=0;delay_count=0;
  unsigned logical=a04_wmt_le16(b),fifo=(logical+3)&~3u,bus=fifo>512?(fifo+511)&~511u:fifo;
  assert(length==bus && channel.pending_cost==fifo && channel.free_bytes+fifo==2080);
  assert(!!(arg&(1u<<27))==(length>512));
  for(unsigned i=logical;i<length;i++)assert(!b[i]);
  struct a04_stp_frame f;assert(!a04_stp_parse(0,b+4,logical-4,&f) && f.task==4);
  build_response(f.payload.data,f.payload.length);
  if(calls==fault_at && fault==6)return A04_SDIO_DATA;
  return 0;
 }
 if(reg==0x1c){assert(length==rx_length && !(arg&(1u<<26)));memcpy(b,rx,length);ack_reads++;return 0;}
 assert(length==4 && (arg&(1u<<26)));uint32_t value=0;
 if(reg==4)value=0x101;
 else if(reg==0xc)value=0;
 else{
  assert(reg==0x10);
  if(calls && !received){
   step++;
   if(!(calls==fault_at && fault==1)){
    if(!credit && !(calls==fault_at && fault==3)){value|=0x14;credit=1;}
    if(step>=2){value|=(rx_length<<16)|2;received=1;}
   }
  }
 }
 a04_wmt_put32(b,value);return 0;
}
static void reset(void){
 calls=ack_reads=resets=step=credit=received=delay_count=part=offset=fragments=address_step=0;
 strap_seen=merge_seen=coex_seen=clock_seen=version_seen=function_enabled=common_enables=0;memset(operations,0,sizeof(operations));
 fault_at=fault=fn1_timeout=wrong_hw=0;memset(&channel,0,sizeof(channel));memset(&state,0,sizeof(state));
}
static inline void synthetic(uint8_t *a,unsigned an,uint8_t *b,unsigned bn){
 memset(a,0x11,an);memset(b,0x22,bn);memset(&files,0,sizeof(files));files.count=2;
 for(unsigned i=0;i<2;i++){
  files.files[i].count=2;files.files[i].sequence=i+1;files.files[i].destination=0x80000;
  files.files[i].header.hardware=files.files[i].header.software=0x8a11;
 }
 files.files[0].header.body.data=a;files.files[0].header.body.length=an;
 files.files[1].header.body.data=b;files.files[1].header.body.length=bn;
}
'''


class WMTDownload(unittest.TestCase):
    run_c = codecs.WMTProtocol.run_c

    def test_native_order_each_failure_poison_and_no_repeat(self):
        self.run_c(MODEL + r'''
int main(void){
 uint8_t a[1201],b[2200];synthetic(a,sizeof(a),b,sizeof(b));
 struct a04_sdio_io io={0,direct,delay};struct a04_sdio_transport t={&io,transfer,2,4,0};
 const struct a04_wmt_startup_options options={0,0,0,1,2,1};
 reset();assert(!a04_wmt_channel_init(&t,&channel));
 assert(!a04_wmt_download_run(&t,&channel,&files,&identity,&options,&state,0,0));
 const unsigned expected[16]={8,8,1,1,7,8,8,1,1,1,7,7,8,5,8,8};
 for(unsigned i=0;i<16;i++)assert(operations[i]==expected[i]);
 assert(common_enables==1);
 assert(calls==16 && state.commands==16 && state.acked==16 && ack_reads==16 && fragments==5);
 assert(resets==3 && state.reset_acks==3 && state.parts_acked==2 && merge_seen==1 && strap_seen==1 && !coex_seen && !clock_seen);
 assert(state.stp_ready && state.function1_ready && function_enabled && t.enabled==6 && state.phase==A04_WMT_DL_READY);
 unsigned complete_calls=calls;
 assert(a04_wmt_download_run(&t,&channel,&files,&identity,&options,&state,0,0)==A04_WMT_DOWNLOAD_STATE && calls==complete_calls);
 const unsigned faults[]={1,2,3,4,6};
 for(unsigned target=1;target<=16;target++)for(unsigned k=0;k<5;k++){
  reset();t.failed=0;t.enabled=4;assert(!a04_wmt_channel_init(&t,&channel));fault_at=target;fault=faults[k];
  int e=a04_wmt_download_run(&t,&channel,&files,&identity,&options,&state,0,0);
  assert(e && state.failed && t.failed && channel.failed && !state.function1_ready && calls==target);
  assert(state.acked==target-1 && !state.stp_ready);
  unsigned n=calls;
  assert(a04_wmt_download_run(&t,&channel,&files,&identity,&options,&state,0,0)==A04_WMT_DOWNLOAD_STATE && calls==n);
 }
 reset();t.failed=0;t.enabled=4;assert(!a04_wmt_channel_init(&t,&channel));fn1_timeout=1;
 assert(a04_wmt_download_run(&t,&channel,&files,&identity,&options,&state,0,0)==A04_SDIO_TIMEOUT && state.failed && !state.function1_ready);
 reset();t.failed=0;t.enabled=4;assert(!a04_wmt_channel_init(&t,&channel));wrong_hw=1;
 assert(a04_wmt_download_run(&t,&channel,&files,&identity,&options,&state,0,0)==A04_WMT_DOWNLOAD_IDENTITY && !function_enabled);
 reset();t.failed=0;t.enabled=4;assert(!a04_wmt_channel_init(&t,&channel));
 struct a04_wmt_startup_options configured={1,1,1,1,2,1};
 assert(!a04_wmt_download_run(&t,&channel,&files,&identity,&configured,&state,0,0));assert(coex_seen==1 && clock_seen==1 && calls==18);
 assert(operations[12]==0x10 && operations[13]==0x0a && operations[14]==8 && operations[15]==5 && common_enables==1);
 reset();t.failed=0;t.enabled=4;assert(!a04_wmt_channel_init(&t,&channel));files.files[1].sequence=1;
 assert(a04_wmt_download_run(&t,&channel,&files,&identity,&options,&state,0,0)==A04_WMT_FW_SET && !calls && !state.attempted);
 return 0;
}
''')

    def test_real_verified_bundle_all_fragments_and_three_resets(self):
        bundle = Path(__file__).resolve().parents[3] / 'out/wifi-worker/wmt-matched-cpu-bundle.bin'
        if not bundle.is_file():
            self.skipTest('external prepared CPU bundle absent')
        self.run_c(MODEL + 'static const char *path=' + json.dumps(str(bundle)) + ';\n' + r'''
int main(void){
 FILE *f=fopen(path,"rb");assert(f);assert(!fseek(f,0,SEEK_END));long n=ftell(f);assert(n==115456);rewind(f);
 uint8_t *owned=(uint8_t *)malloc((size_t)n);assert(owned && fread(owned,1,(size_t)n,f)==(size_t)n);fclose(f);
 assert(!a04_wmt_firmware_bundle(&identity,owned,(unsigned)n,&files));
 struct a04_sdio_io io={0,direct,delay};struct a04_sdio_transport t={&io,transfer,2,4,0};
 const struct a04_wmt_startup_options options={0,0,0,1,2,1};
 reset();assert(!a04_wmt_channel_init(&t,&channel));
 assert(!a04_wmt_download_run(&t,&channel,&files,&identity,&options,&state,0,0));
 assert(common_enables==1);
 assert(calls==127 && ack_reads==127 && fragments==116 && resets==3 && state.parts_acked==2);
 assert(state.stp_ready && state.function1_ready && channel.free_bytes==2080 && !channel.pending_cost);
 free(owned);return 0;
}
''')

    @unittest.skipUnless((codecs.BOARD / 'core/wmt_ic_6630.c').exists(), 'board source absent')
    def test_postpatch_vectors_and_sdio_conditionals_match_board(self):
        text = (codecs.BOARD / 'core/wmt_ic_6630.c').read_text()
        for name, expected in {
            'WMT_RESET_CMD': [1,7,1,0,4], 'WMT_RESET_EVT': [2,7,1,0,0],
            'WMT_SET_DAI_MODE_REG_EVT': [2,8,4,0,0,0,0,3],
            'WMT_STRAP_CONF_CMD_FM_COMM': [1,5,2,0,2,2],
            'WMT_CORE_CO_CLOCK_CMD': [1,10,2,0,8,3],
        }.items():
            raw = re.search(r'\b'+name+r'\[\]\s*=\s*\{([^}]+)\}', text).group(1)
            raw = re.sub(r'/\*.*?\*/', '', raw, flags=re.S)
            values = [int(v,16) for v in re.findall(r'0x[0-9a-fA-F]+', raw)]
            self.assertEqual(values, expected)
        self.assertIn('wmt_core_init_script(init_table_3, ARRAY_SIZE(init_table_3))', text)
        wifi = (codecs.BOARD / 'core/wmt_func.c').read_text()
        self.assertRegex(wifi, r'if \(wmt_detect_get_chip_type\(\) == WMT_CHIP_TYPE_COMBO\)\s*return wmt_func_wifi_ctrl\(FUNC_ON\)')
