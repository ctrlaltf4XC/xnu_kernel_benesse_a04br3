# SPDX-License-Identifier: MIT
"""Execute live runtime codec against independently injected FIFO/credits.
No actual calibration asset or real device MAC is used in these fixtures.
"""
import unittest
import re
from pathlib import Path
import test_wmt_protocol as codecs
from test_wlan_host import FAKE

LIVE = FAKE.replace('static inline int transfer(', 'static inline int initial_transfer(').replace('t->transfer=transfer;', 't->transfer=initial_transfer;') + r'''
#include "A04BR3WLANRuntime.h"
static uint8_t captured[32][600], inbound[4096];
static unsigned captures, inbound_size, inbound_port, interrupt_status, wasr_value, wasr_reads, wasr_fault;
static int live_transfer(void *ctx,uint32_t arg,void *buffer,unsigned n){
 struct card *c=(struct card *)ctx;uint8_t *p=(uint8_t *)buffer;
 unsigned addr=(arg>>9)&0x1ffff,w=arg>>31;
 if(!w && addr==0x20){wasr_reads++;if(wasr_fault)return 24;a04_wmt_put32(p,wasr_value);return 0;}
 if(w && addr==0x34 && a04_wmt_le16(p+2)==0x8000)assert(p[5]==0xa0);
 if(!w && addr==0x10){assert(n==4);a04_wmt_put32(p,interrupt_status);interrupt_status=0;return 0;}
 if(!w && addr==0x90){assert(n==4);a04_wmt_put32(p,inbound_size<<(inbound_port*16));return 0;}
 if(!w && (addr==0x50 || addr==0x54)){
  assert(inbound_size && addr==(inbound_port?0x54u:0x50u));assert(n>=inbound_size+4);
  memset(p,0,n);memcpy(p,inbound,inbound_size);inbound_size=0;return 0;
 }
 int e=initial_transfer(ctx,arg,buffer,n);
 if(!e && w && addr==0x34 && p[5]==0xa0){
  unsigned logical=a04_wmt_le16(p);assert(captures<32 && logical<=600);memcpy(captured[captures++],p,logical);
  c->cpu+=(logical+127)/128;c->ffa+=(logical+127)/128;interrupt_status|=1;
 }
 return e;
}
static void init_live(struct card *c,struct a04_sdio_io *io,struct a04_sdio_transport *t,struct a04_wlan_host *h,struct a04_wlan_runtime *r){
 setup(c,io,t);t->transfer=live_transfer;assert(!a04_wlan_host_open(h,t));h->ready=1;h->wcir=0x316630;
 memset(r,0,sizeof(*r));captures=inbound_size=interrupt_status=wasr_value=wasr_reads=wasr_fault=0;
}
static void board(uint8_t b[512]){
 for(unsigned i=0;i<512;i++){b[i]=(uint8_t)(i*7);}
 a04_wmt_put16(b,0x106);a04_wmt_put16(b+2,0);
 a04_wmt_put16(b+256,1);a04_wmt_put16(b+258,0);
 const uint8_t mac[]={2,3,4,5,6,7};memcpy(b+4,mac,6);b[10]=b[11]=0;b[196]=1;
 b[83]=b[198]=b[266]=0;b[202]=b[203]=0;memset(b+204,0,36);
}
static void event(unsigned id,unsigned seq,const uint8_t *payload,unsigned n,unsigned port){
 memset(inbound,0,sizeof(inbound));a04_wmt_put16(inbound,n+8);a04_wmt_put16(inbound+2,0xe000);
 inbound[4]=(uint8_t)id;inbound[5]=(uint8_t)seq;memcpy(inbound+8,payload,n);inbound_size=n+8;inbound_port=port;
}
static void capability(struct a04_wlan_runtime *r,unsigned invalid_mac){
 uint8_t p[116]={0};a04_wmt_put16(p,0x6630);a04_wmt_put16(p+2,0x1234);
 const uint8_t mac[]={2,9,8,7,6,5};if(!invalid_mac)memcpy(p+8,mac,6);event(1,r->cap_sequence,p,116,1);
}
'''

class WLANRuntimeBackend(unittest.TestCase):
    run_c=codecs.WMTProtocol.run_c

    def test_primary_native_wire_constant_and_nic_struct_layout(self):
        fixtures=Path(__file__).resolve().parent/'fixtures/mt6630_native'
        text=(fixtures/'nic_cmd_event_fixture.h').read_text()
        native=(fixtures/'nic_tx_command.inc').read_text()
        macro=re.search(r'^#define CMD_PACKET_TYPE_ID\s+([^\n]+)',text,re.M).group(0)
        # The header's inline0x20 comment is stale; actual final nicTxCmd
        # replaces it with the0xA0 macro immediately before FIFO copy/write.
        self.assertIn('prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;',native)
        decl='typedef uint8_t UINT_8;typedef uint16_t UINT_16;typedef uint32_t UINT_32;\n'+macro+'\n'
        for name in ('WIFI_CMD_T','EVENT_NIC_CAPABILITY_T'):
            match=re.search(r'typedef struct _'+name+r'\s*\{.*?\}\s*'+name+r'\s*,[^;]*;',text,re.S)
            self.assertIsNotNone(match);decl+=match.group(0)+'\n'
        self.run_c(LIVE+'\n#include <stddef.h>\n'+decl+r'''
int main(void){
 assert(CMD_PACKET_TYPE_ID==0xa0 && A04_WLAN_CMD_PACKET==CMD_PACKET_TYPE_ID);
 assert(sizeof(WIFI_CMD_T)==8 && offsetof(WIFI_CMD_T,ucPktTypeID)==5);
 assert(sizeof(EVENT_NIC_CAPABILITY_T)==116 && offsetof(EVENT_NIC_CAPABILITY_T,aucMacAddr)==8);
 assert(offsetof(EVENT_NIC_CAPABILITY_T,ucRfCalFail)==18 && offsetof(EVENT_NIC_CAPABILITY_T,ucBbCalFail)==19);
 struct card c;struct a04_sdio_io io;struct a04_sdio_transport t;struct a04_wlan_host h;struct a04_wlan_runtime r;
 init_live(&c,&io,&t,&h,&r);uint8_t nv[512];board(nv);assert(!a04_wlan_runtime_calibration(&r,nv,512));
 assert(a04_wlan_command(&h,0x80,1,0x20,0,0)==A04_WLAN_ARGUMENT && !c.txs && !h.failed);
 assert(!a04_wlan_live_begin(&r,&h,1));WIFI_CMD_T expected;memset(&expected,0,sizeof(expected));
 expected.u2TxByteCount=124;expected.u2PQ_ID=0x8000;expected.ucCID=0x80;expected.ucPktTypeID=CMD_PACKET_TYPE_ID;expected.ucSeqNum=1;
 assert(!memcmp(captured[0],&expected,sizeof(expected)) && h.last_cmd_type==0xa0 && h.last_cid==0x80);
 capability(&r,0);assert(!a04_wlan_runtime_step(&r,&h,2,0,0));
 for(unsigned i=0;i<captures;i++)assert(captured[i][5]==CMD_PACKET_TYPE_ID);
 assert(r.cap_received && r.last_event_id==1 && r.last_event_bytes==116 && r.phase==12);
 return 0;
}
''')

    def test_fatal_status_reads_reason_once_before_poison_and_distinguishes_phase(self):
        self.run_c(LIVE+r'''
int main(void){
 struct card c;struct a04_sdio_io io;struct a04_sdio_transport t;struct a04_wlan_host h;struct a04_wlan_runtime r;
 uint8_t nv[512];board(nv);init_live(&c,&io,&t,&h,&r);assert(!a04_wlan_runtime_calibration(&r,nv,512));assert(!a04_wlan_live_begin(&r,&h,1));
 capability(&r,0);assert(!a04_wlan_runtime_step(&r,&h,2,0,0));unsigned tx=c.txs;
 interrupt_status=0x40;wasr_value=0x10000;
 assert(a04_wlan_runtime_step(&r,&h,3,0,0)==A04_WLAN_STATUS && r.events==1 && r.cap_received);
 assert(h.status==0x40 && h.wasr==0x10000 && !h.wasr_error && wasr_reads==1 && r.phase==12);
 assert(h.last_cid==3 && h.last_cmd_type==0xa0 && h.last_cmd_set==1 && h.last_cmd_bytes==502);
 assert(a04_wlan_runtime_step(&r,&h,4,0,0)==A04_WLAN_STATE && wasr_reads==1 && c.txs==tx);
 /* events1 need not be NIC success: one unrelated event, then assert. */
 init_live(&c,&io,&t,&h,&r);assert(!a04_wlan_runtime_calibration(&r,nv,512));assert(!a04_wlan_live_begin(&r,&h,1));
 uint8_t other[4]={0};event(0x0d,0,other,4,1);assert(!a04_wlan_runtime_step(&r,&h,2,0,0));
 interrupt_status=0x80000000;wasr_fault=1;
 assert(a04_wlan_runtime_step(&r,&h,3,0,0)==A04_WLAN_STATUS && r.events==1 && !r.cap_received && r.phase==2);
 assert(r.last_event_id==0x0d && h.last_cid==0x80 && h.wasr_error==24 && wasr_reads==1);
 assert(a04_wlan_runtime_step(&r,&h,4,0,0)==A04_WLAN_STATE && wasr_reads==1);
 return 0;
}
''')

    def test_each_configure_send_failure_has_exact_stage_and_last_command(self):
        self.run_c(LIVE+r'''
int main(void){
 struct card c;struct a04_sdio_io io;struct a04_sdio_transport t;struct a04_wlan_host h;struct a04_wlan_runtime r;
 const unsigned expected_cid[]={0x24,0x0f,0x0f,0x48,2,0x11,0x0a,3};
 const unsigned expected_phase[]={4,7,8,10,11,11,11,12};
 for(unsigned failure=0;failure<8;failure++){
  uint8_t nv[512];board(nv);init_live(&c,&io,&t,&h,&r);assert(!a04_wlan_runtime_calibration(&r,nv,512));assert(!a04_wlan_live_begin(&r,&h,1));
  c.fail_tx=failure+2;capability(&r,0);assert(a04_wlan_runtime_step(&r,&h,2,0,0));
  assert(r.failed && h.failed && r.events==1 && r.cap_received && r.last_event_id==1 && r.last_event_bytes==116);
  assert(r.phase==expected_phase[failure] && h.last_cid==expected_cid[failure] && h.last_cmd_type==0xa0);
  unsigned tx=c.txs;assert(a04_wlan_runtime_step(&r,&h,3,0,0)==A04_WLAN_STATE && c.txs==tx);
 }
 return 0;
}
''')

    def test_live_query_original_board_commands_and_passive_scan(self):
        self.run_c(LIVE+r'''
int main(void){
 struct card c;struct a04_sdio_io io;struct a04_sdio_transport t;struct a04_wlan_host h;struct a04_wlan_runtime r;
 init_live(&c,&io,&t,&h,&r);uint8_t nv[512];board(nv);
 assert(!a04_wlan_runtime_calibration(&r,nv,512));
 /* Post-ready init counters are discarded by native reset-resource boundary. */
 c.cpu=3;c.ffa=3;interrupt_status=1;h.cpu_used=3;
 assert(!a04_wlan_live_begin(&r,&h,100) && r.cap_pending && h.runtime);
 assert(captures==1 && a04_wmt_le16(captured[0])==124 && captured[0][4]==0x80 && !captured[0][6]);
 for(unsigned i=8;i<124;i++)assert(!captured[0][i]);
 assert(a04_wlan_runtime_step(&r,&h,101,0,0)==A04_WLAN_PENDING && !r.configured);
 capability(&r,0);assert(!a04_wlan_runtime_step(&r,&h,102,0,0));
 assert(r.configured && r.mac_valid && r.mac_source==1 && r.calibration_sent && !r.station.sta_active && !r.security.authorized);
 const unsigned ids[]={0x80,0x24,0x0f,0x0f,0x48,2,0x11,0x0a,3};assert(captures==9);
 for(unsigned i=0;i<captures;i++)assert(captured[i][4]==ids[i] && captured[i][6]==(i?1:0));
 assert(!memcmp(captured[1]+8,nv+12,40) && !memcmp(captured[4]+8,nv,512));
 /* Compare wire domain entries to primary default domain, including zero padding. */
 const uint8_t normal[5][5]={{81,1,1,1,13},{115,2,4,36,4},{118,2,4,52,4},{121,2,4,100,12},{125,2,4,149,5}};
 assert(a04_wmt_le16(captured[2]+10)==1 && !a04_wmt_le16(captured[3]+10));
 for(unsigned i=0;i<5;i++)assert(!memcmp(captured[3]+12+8*i,normal[i],5));
 assert(!memcmp(captured[6]+12,r.mac,6));
 uint8_t *scan=captured[8]+8;assert(!scan[2] && !scan[5] && scan[382]==4 && scan[383]==13);
 assert(scan[7]==3 && !a04_wmt_le16(scan+370) && !a04_wmt_le16(scan+372) && !a04_wmt_le16(scan+374));
 for(unsigned i=0;i<13;i++)assert(scan[384+2*i]==1 && scan[385+2*i]==i+1);
 assert(!r.station.local.active_channels && r.station.scan.pending);
 /* Actual FIFO beacon and actual scan-done are separate observations. */
 uint8_t beacon[128]={0};unsigned off=40;beacon[off]=0x80;memset(beacon+off+4,255,6);
 const uint8_t ap[]={2,6,7,8,9,10};memcpy(beacon+off+10,ap,6);memcpy(beacon+off+16,ap,6);
 a04_wmt_put16(beacon+off+32,100);a04_wmt_put16(beacon+off+34,0x401);
 const uint8_t ies[]={0,4,'t','e','s','t',1,4,0x82,0x84,0x8b,0x96,3,1,6};memcpy(beacon+off+36,ies,sizeof(ies));
 unsigned n=off+36+sizeof(ies);a04_wmt_put16(beacon,n);a04_wmt_put16(beacon+2,0x4000|0x800);beacon[6]=24;beacon[5]=6;
 a04_wmt_put32(beacon+24,100u<<8);memcpy(inbound,beacon,n);inbound_size=n;inbound_port=0;
 assert(!a04_wlan_runtime_step(&r,&h,200,0,0));assert(r.scan_count==1 && r.scan[0].channel==6 && !r.station.scan.complete);
 uint8_t done[4]={(uint8_t)r.scan_sequence,0,0,0};event(0x0d,0,done,4,1);
 assert(!a04_wlan_runtime_step(&r,&h,300,0,0) && r.station.scan.complete && !r.station.deadline_us);
 assert(a04_wlan_runtime_step(&r,&h,6000000,0,0)==A04_WLAN_PENDING && !r.failed);
 return 0;
}
''')

    def test_board_rejection_real_mac_fallback_and_poison_no_retry(self):
        self.run_c(LIVE+r'''
int main(void){
 struct card c;struct a04_sdio_io io;struct a04_sdio_transport t;struct a04_wlan_host h;struct a04_wlan_runtime r;
 uint8_t nv[512];board(nv);init_live(&c,&io,&t,&h,&r);
 assert(a04_wlan_runtime_calibration(&r,nv,511)==A04_WLAN_ARGUMENT && !r.calibration_valid && !captures);
 nv[4]=1;assert(a04_wlan_runtime_calibration(&r,nv,512)==A04_WLAN_FORMAT && !captures);nv[4]=2;
 nv[10]='J';assert(a04_wlan_runtime_calibration(&r,nv,512)==A04_WLAN_UNSUPPORTED && !captures);nv[10]=0;
 assert(!a04_wlan_runtime_calibration(&r,nv,512));assert(!a04_wlan_live_begin(&r,&h,1));
 capability(&r,1);assert(!a04_wlan_runtime_step(&r,&h,2,0,0) && r.mac_source==2 && !memcmp(r.mac,nv+4,6));
 init_live(&c,&io,&t,&h,&r);assert(!a04_wlan_runtime_calibration(&r,nv,512));assert(!a04_wlan_live_begin(&r,&h,10));
 unsigned txs=c.txs;assert(a04_wlan_runtime_step(&r,&h,1000010,0,0)==A04_WLAN_TIMEOUT && r.failed && h.failed);
 assert(a04_wlan_live_begin(&r,&h,1000011)==A04_WLAN_STATE && c.txs==txs);
 init_live(&c,&io,&t,&h,&r);assert(!a04_wlan_runtime_calibration(&r,nv,512));assert(!a04_wlan_live_begin(&r,&h,10));
 capability(&r,0);inbound[5]++;assert(a04_wlan_runtime_step(&r,&h,11,0,0)==A04_WLAN_SEQUENCE && r.failed && !r.configured);
 init_live(&c,&io,&t,&h,&r);assert(!a04_wlan_runtime_calibration(&r,nv,512));assert(!a04_wlan_live_begin(&r,&h,10));
 capability(&r,0);c.fail_tx=2;assert(a04_wlan_runtime_step(&r,&h,11,0,0) && r.failed && !r.configured && !r.calibration_sent);
 txs=c.txs;assert(a04_wlan_runtime_step(&r,&h,12,0,0)==A04_WLAN_STATE && c.txs==txs);
 return 0;
}
''')

if __name__=='__main__':unittest.main()
