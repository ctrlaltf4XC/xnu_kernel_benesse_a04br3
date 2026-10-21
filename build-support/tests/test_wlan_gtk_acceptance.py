# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Actual Personal/HIF/crypto; card/PTK/TX events are HOST SIMULATED.
No synthetic GTK ACK: acceptance comes from the production synchronous send.
"""
from test_wlan_personal_runtime import ROOT, COMPLETE_FIXTURE
import test_wlan_psk4way as owner
import unittest

class GTKAcceptance(unittest.TestCase):
 production=True
 run_c=owner.PersonalState.run_c
 def runtime(self,body):
  self.run_c('#include "'+str(ROOT/'iokit/Kernel/arm/sdio/A04BR3WLANPersonalRuntime.h')+'"\n'+COMPLETE_FIXTURE+body)
 def test_hif_errors_and_timeout_poison_without_m4_or_retry(self):
  self.runtime(r'''
static int inject_error;
static int fault_transfer(void *ctx,uint32_t arg,void *buf,unsigned n){
 if((arg>>31)&&((arg>>9)&0x1ffff)==0x34&&inject_error)return inject_error;
 return transfer(ctx,arg,buf,n);
}
int main(void){for(unsigned mode=0;mode<3;mode++){
 struct card c;struct a04_sdio_io io;struct a04_sdio_transport t;struct a04_wlan_host h;
 setup(&c,&io,&t);assert(!a04_wlan_host_open(&h,&t));h.ready=1;assert(!a04_wlan_runtime_begin(&h));
 struct a04_wlan_runtime r;actual_runtime_to_keys(&r,&h);assert(!a04_wlan_personal_drive(&r,&h,300));
 uint8_t done[8]={0};done[0]=(uint8_t)r.station.local.bss;memcpy(done+2,r.station.ap.mac,6);
 struct a04_wlan_event e={0x24,0,8,done};assert(!a04_wlan_runtime_event(&r,&h,&e,320));
 const int errors[]={A04_SDIO_DATA,A04_SDIO_TIMEOUT,A04_SDIO_COMMAND};
 inject_error=errors[mode];t.transfer=fault_transfer;unsigned txs=c.txs;
 assert(a04_wlan_personal_drive(&r,&h,330));
 assert(r.security.failed&&r.personal.failed&&!r.security.gtk_accepted&&!r.security.authorized);
 assert(r.personal.gtk_state!=2&&!r.personal.m4_confirmed&&!r.data.pending&&c.txs==txs);
 inject_error=0;assert(a04_wlan_personal_drive(&r,&h,340));assert(c.txs==txs&&!a04_psk_authenticated(&r.personal));
 }return 0;}
''')
 def test_no_software_enqueue_or_stale_epoch_completion(self):
  self.runtime(r'''
int main(void){struct card c;struct a04_sdio_io io;struct a04_sdio_transport t;struct a04_wlan_host h;
 setup(&c,&io,&t);assert(!a04_wlan_host_open(&h,&t));h.ready=1;assert(!a04_wlan_runtime_begin(&h));
 struct a04_wlan_runtime r;actual_runtime_to_keys(&r,&h);
 assert(!a04_wlan_personal_drive(&r,&h,300));unsigned txs=c.txs;
 assert(r.personal.ptk_state==1&&!r.security.ptk_confirmed&&!r.security.gtk_accepted);
 assert(!a04_wlan_personal_drive(&r,&h,310)&&c.txs==txs); // PTK HIF alone cannot advance GTK
 uint8_t done[8]={0};done[0]=(uint8_t)r.station.local.bss;memcpy(done+2,r.station.ap.mac,6);
 struct a04_wlan_event e={0x24,0,8,done};assert(!a04_wlan_runtime_event(&r,&h,&e,320));
 struct a04_psk4way copy=r.personal;const uint8_t *key,*rsc;unsigned id;uint64_t cookie;
 assert(!a04_psk_key_take(&copy,0,325,&key,&id,&rsc,&cookie)); // pure reservation is not HIF
 assert(copy.gtk_state==1&&!r.security.gtk_accepted&&!a04_psk_authenticated(&copy));
 assert(a04_psk_key_done(&copy,0,cookie+1,0,326)==A04_WLAN_SEQUENCE&&copy.gtk_state==1);
 r.association_generation++;assert(a04_wlan_personal_drive(&r,&h,330)==A04_WLAN_STATE);
 assert(c.txs==txs&&!r.security.gtk_accepted&&!r.security.authorized);
 r.association_generation--;r.teardown.pending=1;assert(a04_wlan_personal_drive(&r,&h,340)==A04_WLAN_STATE&&c.txs==txs);
 return 0;}
''')
 def test_duplicate_m3_no_reinstall_and_m4_failure_never_opens_port(self):
  self.runtime(r'''
int main(void){struct card c;struct a04_sdio_io io;struct a04_sdio_transport t;struct a04_wlan_host h;
 setup(&c,&io,&t);assert(!a04_wlan_host_open(&h,&t));h.ready=1;assert(!a04_wlan_runtime_begin(&h));
 struct a04_wlan_runtime r;actual_runtime_to_keys(&r,&h);assert(!a04_wlan_personal_drive(&r,&h,300));
 uint8_t done[16]={0};done[0]=(uint8_t)r.station.local.bss;memcpy(done+2,r.station.ap.mac,6);
 struct a04_wlan_event e={0x24,0,8,done};assert(!a04_wlan_runtime_event(&r,&h,&e,320));
 assert(!a04_wlan_personal_drive(&r,&h,330)&&r.security.gtk_accepted&&r.personal.gtk_state==2);
 unsigned txs=c.txs;uint64_t pn=r.security.gtk_pn[r.security.gtk_id],cookie=r.personal_key_cookie;
 struct a04_psk4way work=r.personal;assert(!a04_psk_rx(&work,three,sizeof(three),340));assert(!a04_wlan_personal_observed(&r,&work));
 assert(c.txs==txs&&r.personal.gtk_state==2&&r.security.gtk_pn[r.security.gtk_id]==pn);
 assert(a04_psk_key_done(&r.personal,0,cookie,0,345)==A04_WLAN_SEQUENCE);
 assert(!r.security.authorized&&!a04_psk_authenticated(&r.personal));
 assert(!a04_wlan_personal_drive(&r,&h,350)&&c.txs==txs+1&&r.data.eapol_kind==4);
 memset(done,0,16);done[0]=(uint8_t)r.data.pid;done[4]=(uint8_t)r.station.local.wlan;done[1]=1;
 e.id=0x0f;e.length=16;assert(a04_wlan_runtime_event(&r,&h,&e,360));
 assert(!r.security.authorized&&!a04_psk_authenticated(&r.personal)&&!a04_wlan_port_allowed(&r.station,&r.security,0));
 return 0;}
''')

 def test_radio_cipher_metadata_is_not_eapol_encrypted_key_data(self):
  self.runtime(r'''
static uint8_t wire[512];static unsigned wire_bytes;
static int rx_transfer(void *ctx,uint32_t arg,void *buf,unsigned n){
 unsigned addr=(arg>>9)&0x1ffff;
 if(!(arg>>31)&&addr==0x10){assert(n==4);a04_wmt_put32((uint8_t *)buf,0);return 0;}
 if(!(arg>>31)&&(addr==0x50||addr==0x54)){assert(n>=wire_bytes);memset(buf,0,n);memcpy(buf,wire,wire_bytes);((struct card *)ctx)->rx_size=0;return 0;}
 return transfer(ctx,arg,buf,n);
}
int main(void){struct card c;struct a04_sdio_io io;struct a04_sdio_transport t;struct a04_wlan_host h;
 setup(&c,&io,&t);assert(!a04_wlan_host_open(&h,&t));h.ready=1;assert(!a04_wlan_runtime_begin(&h));
 struct a04_wlan_runtime r;actual_runtime_to_keys(&r,&h);assert(!a04_wlan_personal_drive(&r,&h,300));
 uint8_t done[8]={0};done[0]=(uint8_t)r.station.local.bss;memcpy(done+2,r.station.ap.mac,6);
 struct a04_wlan_event e={0x24,0,8,done};assert(!a04_wlan_runtime_event(&r,&h,&e,320));
 // Existing real crypto fixture M3 contains standard KEK-wrapped GTK and passed.
 assert(three[5]&0x10);
 // Distinct radio cipher metadata (group1 PN) is unsupported initial-only input.
 wire_bytes=32+14+sizeof(three);a04_wmt_put16(wire,wire_bytes);a04_wmt_put16(wire+2,0x4200);
 wire[6]=0x80|14;wire[8]=(uint8_t)r.station.local.wlan;wire[9]=0x40;wire[16]=1;
 memcpy(wire+32,r.station.local.mac,6);memcpy(wire+38,r.station.ap.mac,6);wire[44]=0x88;wire[45]=0x8e;
 memcpy(wire+46,three,sizeof(three));t.transfer=rx_transfer;c.rx_size=wire_bytes;r.begun=1;
 assert(a04_wlan_runtime_step(&r,&h,330,a04_psk_verify,&r.personal)==A04_WLAN_UNSUPPORTED);
 assert(r.failed&&!r.security.authorized&&!a04_psk_authenticated(&r.personal));
 return 0;}
''')

if __name__=='__main__':unittest.main()
