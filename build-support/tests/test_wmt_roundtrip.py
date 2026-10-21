# SPDX-License-Identifier: MIT
"""Exercise production polled WMT channel on a destructive/read-clear SDIO fake."""
from pathlib import Path
import re
import unittest
import test_wmt_protocol as codecs


class WMTRoundtrip(unittest.TestCase):
    run_c = codecs.WMTProtocol.run_c

    def test_version_roundtrip_credits_aggregate_and_poison(self):
        self.run_c(r'''
#include "A04BR3WMTRoundtrip.h"
static struct a04_wmt_channel channel;
static unsigned mode,txs,reads,status_reads,delays,stage,last_rx_stage,initialized;
static unsigned credit_sent,rx_sent;
static uint32_t address,own,mask;
static uint8_t wire[2560],event[32];
static unsigned logical;
static void delay(void *p,unsigned us){(void)p;assert(us==1000);assert(++delays<=999);}
static int cmd(void *p,unsigned op,uint32_t arg,unsigned response,uint32_t *r){
 (void)p;assert(op==52 && response==1 && (arg>>31) && ((arg>>28)&7)==2);
 assert(((arg>>9)&0x1ffff)==4 && (arg&255)==1);*r=0;initialized=1;return 0;
}
static void make_rx(unsigned which){
 memset(wire,0xa5,sizeof(wire));
 uint8_t first[2005];unsigned n=0,offset=4;
 if(mode==27){
  uint8_t other[600];memset(other,0xee,sizeof(other));
  assert(!a04_stp_encode(0,0,0,0,0,other,sizeof(other),first,sizeof(first),&n));
  memcpy(wire+offset,first,n);offset+=(n+3)&~3u;
 }
 if(mode==14 || mode==16){
  assert(!a04_stp_encode(0,mode==16?5:0,0,0,0,event,5,first,sizeof(first),&n));
  memcpy(wire+offset,first,n);offset+=(n+3)&~3u;
 }
 const uint8_t *p=event;unsigned length=16;
 if(mode==13){p+=which?3:0;length=which?13:3;}
 assert(!a04_stp_encode(0,4,0,0,0,p,length,first,sizeof(first),&n));
 memcpy(wire+offset,first,n);offset+=n;
 if(mode==9){offset=(offset+3)&~3u;memcpy(wire+offset,first,n);offset+=n;}
 if(mode==10){offset=(offset+3)&~3u;wire[offset++]=0x80;}
 logical=offset;a04_wmt_put16(wire,logical);wire[2]=wire[3]=0;
 if(mode==7)wire[0]--;
 if(mode==8)wire[3]=1;
 if(mode==11)wire[4+3]=1;
 if(mode==12)wire[offset-1]=1;
}
static int transfer(void *p,uint32_t arg,void *buffer,unsigned length){
 (void)p;assert(((arg>>28)&7)==2);unsigned reg=(arg>>9)&0x1ffff;
 assert(!(arg&(1u<<27)) || reg==0x1c);
 if(arg&0x80000000u){
  if(reg==0x14){assert(length==4 && (arg&(1u<<26)));mask=a04_wmt_get32((uint8_t *)buffer);return 0;}
  assert(reg==0x18 && initialized && mask==0xfffe && !(arg&(1u<<26)) && length==32);
  assert(channel.pending_cost==32 && channel.free_bytes==2048);txs++;
  uint8_t *b=(uint8_t *)buffer;assert(a04_wmt_le16(b)==30 && b[2]==0 && b[3]==0);
  struct a04_stp_frame f;assert(!a04_stp_parse(0,b+4,26,&f));
  assert(f.task==4 && f.payload.length==20);const uint8_t *c=f.payload.data;
  assert(c[0]==1 && c[1]==8 && c[2]==16 && c[3]==0 && c[4]==2 && c[5]==1 && !c[6] && c[7]==1);
  assert(!a04_wmt_get32(c+12) && a04_wmt_get32(c+16)==0xffff);
  address=a04_wmt_get32(c+8);
  memset(event,0,sizeof(event));event[0]=2;event[1]=8;event[2]=12;event[7]=1;
  a04_wmt_put32(event+8,address);a04_wmt_put32(event+12,address==0x80000000u?0x8a11:0x100);
  if(mode==4)event[1]=7;
  if(mode==5)event[4]=1;
  if(mode==6)event[8]^=4;
  if(mode==17)event[2]=4; /* vendor template's stale size is rejected */
  credit_sent=rx_sent=stage=0;
  if(mode==18)return A04_SDIO_DATA;
  return 0;
 }
 if(reg==0x1c){
  unsigned bus=(logical+3)&~3u;if(bus>512)bus=(bus+511)&~511u;
  assert(!(arg&(1u<<26)) && length==bus);reads++;
  if(mode==19)return A04_SDIO_DATA;
  memcpy(buffer,wire,length);last_rx_stage=stage;return 0;
 }
 assert(length==4 && (arg&(1u<<26)));uint32_t value=0;
 if(reg==4)value=own;
 else if(reg==0xc)value=0;
 else{
  assert(reg==0x10);status_reads++;
  if(mode==20)return A04_SDIO_COMMAND;
  if(!txs){value=mode==21?2:mode==22?0x14:0;}
  else{
   stage++;
   if(mode==23)value=0x100;
   else if(mode==24)value=0x24;
   else if(mode==25)value=(2305u<<16)|2;
   else if(mode!=1){
    /* credit can precede the event or be returned afterwards. */
    unsigned credit_stage=mode==2?1:mode==3?4:2;
    if(!credit_sent && stage>=credit_stage && mode!=26){value|=0x14;credit_sent=1;}
    unsigned receive_stage=mode==2?3:1;
    if(stage>=receive_stage && (!rx_sent || (mode==13 && rx_sent==1 && stage>last_rx_stage))){
     make_rx(rx_sent);value|=(logical<<16)|2;rx_sent++;
    }
   }
  }
 }
 a04_wmt_put32((uint8_t *)buffer,value);return 0;
}
static void reset(unsigned fault){
 mode=fault;txs=reads=status_reads=delays=stage=last_rx_stage=initialized=0;credit_sent=rx_sent=0;own=0x101;mask=0;
}
static void setup(struct a04_sdio_transport *t){assert(!a04_wmt_channel_init(t,&channel));assert(channel.initialized && channel.free_bytes==2080);}
int main(void){
 struct a04_sdio_io io={0,cmd,delay};struct a04_sdio_transport t={&io,transfer,2,4,0};
 const unsigned success[]={0,2,3,13,14,27};
 for(unsigned i=0;i<6;i++){
  reset(success[i]);t.failed=0;setup(&t);uint32_t value=0;
  assert(!a04_wmt_read_version(&t,&channel,0x80000000u,&value));
  assert(value==0x8a11 && txs==1 && !t.failed && !channel.failed && channel.free_bytes==2080 && !channel.pending_cost);
  assert(channel.skipped==((mode==14 || mode==27)?1u:0u));assert(channel.rx_packets==(mode==13?2u:1u));
  /* A second serialized command uses restored credits and a different echo. */
  txs=0;delays=0;assert(!a04_wmt_read_version(&t,&channel,0x80000004u,&value));assert(value==0x100);
 }
 const unsigned failures[]={1,4,5,6,7,8,9,10,11,12,16,17,18,19,20,21,22,23,24,25,26};
 for(unsigned i=0;i<sizeof(failures)/sizeof(failures[0]);i++){
  reset(failures[i]);t.failed=0;setup(&t);uint32_t value=0xdeadbeef;
  int e=a04_wmt_read_version(&t,&channel,0x80000000u,&value);
  assert(e && t.failed && channel.failed && value==0xdeadbeef);
  unsigned count=txs+reads+status_reads;
  assert(a04_wmt_read_version(&t,&channel,0x80000000u,&value)==A04_WMT_CHANNEL_POISON);
  assert(txs+reads+status_reads==count); /* no retry and no status drain after poison */
  if(mode==1 || mode==26)assert(e==A04_WMT_CHANNEL_TIMEOUT && channel.polls==1000 && delays==999);
  if(mode==21 || mode==22 || mode==20)assert(!txs);
 }
 reset(0);t.failed=0;setup(&t);uint32_t value=123;
 assert(a04_wmt_read_version(&t,&channel,0x1234,&value)==A04_SDIO_ARGUMENT && !txs && !t.failed);
 own=0;assert(a04_wmt_read_version(&t,&channel,0x80000000u,&value)==A04_WMT_CHANNEL_OWN && !txs);
 return 0;
}
''')

    def test_large_command_fifo_charge_and_block_padding(self):
        self.run_c(r'''
#include "A04BR3WMTRoundtrip.h"
static struct a04_wmt_channel channel;
static unsigned sent,status_reads,charged,bus_count;
static int cmd(void *p,unsigned op,uint32_t a,unsigned r,uint32_t *out){(void)p;(void)op;(void)a;(void)r;*out=0;return 0;}
static void delay(void *p,unsigned us){(void)p;(void)us;}
static int transfer(void *p,uint32_t a,void *buffer,unsigned len){
 (void)p;unsigned reg=(a>>9)&0x1ffff;uint8_t *b=(uint8_t *)buffer;
 if(a>>31){
  if(reg==0x14)return 0;
  assert(reg==0x18 && len>512 && (a&(1u<<27)) && !(a&(1u<<26)));
  assert((a&511)==len/512);sent++;bus_count=len;charged=channel.pending_cost;
  assert(channel.free_bytes+charged==2080 && charged==2012 && bus_count==2048);
  assert(a04_wmt_le16(b)==2009);
  for(unsigned i=2009;i<len;i++)assert(!b[i]);
  return 0;
 }
 if(reg==0x1c){
  assert(len==16);memset(b,0,len);b[0]=15;
  const uint8_t event[]={2,9,1,0,0};unsigned n=0;
  return a04_stp_encode(0,4,0,0,0,event,5,b+4,12,&n);
 }
 uint32_t value=0;if(reg==4)value=0x101;
 if(reg==0x10){status_reads++;if(sent)value=(15u<<16)|0x16;}
 a04_wmt_put32(b,value);return 0;
}
int main(void){
 struct a04_sdio_io io={0,cmd,delay};struct a04_sdio_transport t={&io,transfer,2,4,0};
 assert(!a04_wmt_channel_init(&t,&channel));
 uint8_t parameters[1995],command[1999];memset(parameters,0x66,sizeof(parameters));unsigned n=0;
 assert(!a04_wmt_command(9,parameters,sizeof(parameters),command,sizeof(command),&n));
 assert(!a04_wmt_channel_exchange(&t,&channel,command,n));
 assert(sent==1 && status_reads==2 && channel.free_bytes==2080 && !channel.pending_cost);
 return 0;
}
''')

    @unittest.skipUnless((codecs.BOARD / 'core/wmt_core.c').exists(), 'board source not present')
    def test_vendor_read_template_and_startup_queries(self):
        text = (codecs.BOARD / 'core/wmt_core.c').read_text()
        raw = re.search(r'WMT_SET_REG_CMD\[\]\s*=\s*\{([^}]+)\}', text).group(1)
        raw = re.sub(r'/\*.*?\*/', '', raw, flags=re.S)
        values = [int(v, 16) for v in re.findall(r'0x[0-9a-fA-F]+', raw)]
        self.assertEqual(len(values), 20)
        fixture = 'static const uint8_t vendor[]={' + ','.join(map(str, values)) + '};\n'
        self.run_c('#include "A04BR3WMTRoundtrip.h"\n' + fixture + r'''
int main(void){
 uint8_t out[20],expected[20];memcpy(expected,vendor,20);
 expected[4]=2;a04_wmt_put32(expected+8,0x80000000u);a04_wmt_put32(expected+16,0xffff);
 assert(!a04_wmt_version_command(0x80000000u,out,20));assert(!memcmp(out,expected,20));
 const uint8_t evt[]={2,8,12,0,0,0,0,1,0,0,0,0x80,0x11,0x8a,0,0};
 uint32_t value=0;assert(!a04_wmt_version_reply(out,evt,16,&value) && value==0x8a11);
 assert(a04_wmt_version_command(0x80000010u,out,20)==A04_SDIO_ARGUMENT);
 return 0;
}
''')
        cpp = (codecs.INCLUDE / 'A04BR3SDIO.cpp').read_text()
        self.assertIn('a04_wmt_channel_init(&transport,&channel)', cpp)
        self.assertIn('a04_wmt_read_version(&transport,&channel', cpp)
        self.assertIn('0x80000000u,0x80000004u', cpp)
        self.assertNotIn('a04_wmt_patch_next', cpp)
