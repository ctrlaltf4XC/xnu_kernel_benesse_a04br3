# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Actual runtime/HIF+real crypto. Host injected firmware events are NOT HW proof.
GTK host acceptance requires actual synchronous HIF success; M4 TX_DONE gates IP.
"""
from pathlib import Path
import unittest
import test_wlan_psk4way as owner
ROOT=Path(__file__).resolve().parents[2]
class PersonalRuntime(unittest.TestCase):
 production=True
 run_c=owner.PersonalState.run_c
 def test_real_crypto_actual_tx_ptk_event_group_hif_acceptance(self):
  self.run_c('#include "'+str(ROOT/'iokit/Kernel/arm/sdio/A04BR3WLANPersonalRuntime.h')+'"\n'+
   '#include "'+str(ROOT/'iokit/Kernel/arm/sdio/A04BR3WLANCurrentNetwork.h')+'"\n'+
   '#include "'+str(ROOT/'iokit/Kernel/arm/sdio/A04BR3WLANNetworkProfile.h')+'"\n'+
   '#include "'+str(ROOT/'iokit/Kernel/arm/network/A04BR3NativeGET.h')+'"\n'+
   '#include "'+str(ROOT/'iokit/Kernel/arm/network/A04BR3NativePeerIdentity.h')+'"\n'+r'''
int main(void){struct card c;struct a04_sdio_io io;struct a04_sdio_transport t;struct a04_wlan_host h;
 setup(&c,&io,&t);assert(!a04_wlan_host_open(&h,&t));h.ready=1;assert(!a04_wlan_runtime_begin(&h));
 struct a04_wlan_runtime r;memset(&r,0,sizeof(r));local_profile(&r.station.local);ap_profile(&r.station.ap);
 memcpy(r.station.ap.rsn,rsn_ref,sizeof(rsn_ref));r.station.ap.rsn_length=sizeof(rsn_ref);r.station.ap.capability|=16;
 r.station.configured=r.station.selected=r.station.sta_active=r.station.secured=1;r.station.join.phase=4;r.station.join.aid=42;
 r.configured=r.admin=r.personal_admitted=1;r.auth_lower=1;r.auth_upper=8;r.association_generation=7;
 assert(!a04_psk_prepare(&r.credential,&crypto,(const uint8_t *)"password",8,1,(const uint8_t *)"test",4));
 assert(!a04_wlan_personal_start(&r,&crypto,100));
 struct a04_psk4way work=r.personal;assert(!a04_psk_rx(&work,one,sizeof(one),150));assert(!a04_wlan_personal_observed(&r,&work));
 assert(!a04_wlan_personal_drive(&r,&h,200));assert(r.data.pending&&r.data.eapol_kind==2&&r.personal.tx_pending&&!r.security.authorized);
 assert(!memcmp(h.tx+28+14,two,sizeof(two)));uint64_t cookie=r.personal_tx_cookie;unsigned pid=r.data.pid;
 uint8_t eventbytes[16]={0};eventbytes[0]=(uint8_t)pid;eventbytes[4]=(uint8_t)r.station.local.wlan;
 struct a04_wlan_event ev={0x0f,0,16,eventbytes};assert(!a04_wlan_runtime_event(&r,&h,&ev,210));
 assert(!r.data.pending&&r.security.phase==2&&r.personal.phase==A04_PSK_WAIT_M3&&!r.personal.tx_pending);
 assert(a04_psk_tx_done(&r.personal,cookie,0,220)); /* duplicate actual completion cannot advance */
 work=r.personal;assert(!a04_psk_rx(&work,three,sizeof(three),250));assert(!a04_wlan_personal_observed(&r,&work));
 unsigned txs=c.txs;assert(!a04_wlan_personal_drive(&r,&h,300));assert(c.txs==txs+1&&r.security.ptk_pending&&r.personal.ptk_state==1);
 assert(!a04_wlan_personal_drive(&r,&h,310)&&c.txs==txs+1); /* no reinstall while waiting real ACK */
 memset(eventbytes,0,16);eventbytes[0]=(uint8_t)r.station.local.bss;memcpy(eventbytes+2,r.station.ap.mac,6);
 ev.id=0x24;ev.length=8;assert(!a04_wlan_runtime_event(&r,&h,&ev,320));assert(r.security.ptk_confirmed&&r.personal.ptk_state==2);
 assert(!a04_wlan_personal_drive(&r,&h,330));assert(c.txs==txs+2&&r.security.gtk_accepted);
 assert(r.personal.gtk_state==2&&!r.data.pending&&!a04_psk_authenticated(&r.personal));
 assert(!a04_wlan_personal_drive(&r,&h,340)&&c.txs==txs+3);
 assert(a04_wlan_security_authorize(&r.security,0,350)==A04_WLAN_STATE&&!r.security.authorized);
 /* Duplicate PTK event cannot open port before genuine M4 TX_DONE. */
 assert(!a04_wlan_runtime_event(&r,&h,&ev,360));assert(r.security.gtk_accepted&&r.personal.gtk_state==2&&!r.security.authorized);
 r.scan_generation=1;r.scan_count=1;r.scan[0]=r.station.ap;r.scan_extra[0].generation=1;r.scan_extra[0].observed_us=100;
 struct a04_wlan_scan_record record;assert(!a04_wlan_current_record(&r,&record));
 struct a04_native_peer_identity peer;memset(&peer,0,sizeof(peer));peer.ssid_length=record.ap.ssid_length;memcpy(peer.ssid,record.ap.ssid,peer.ssid_length);memcpy(peer.bssid,record.ap.mac,6);
 uint8_t out[32];uint32_t bytes=32;assert(!a04_native_peer_encode(&peer,1,0,out,&bytes)&&bytes==4&&!memcmp(out,"test",4));
 bytes=6;assert(!a04_native_peer_encode(&peer,9,0,out,&bytes)&&!memcmp(out,r.station.ap.mac,6));
 struct a04_network_state state;memset(&state,0,sizeof(state));state.present=state.mac_valid=state.firmware_running=state.control_ready=state.radio_enabled=1;
 memcpy(state.mac,r.station.local.mac,6);a04_wlan_network_profile(&r,&state,1);
 assert(state.associated&&state.preauth_ready&&state.auth_profile_valid&&state.auth_upper==8&&!state.authorized&&!state.datapath_ready);
 bytes=16;memset(out,0xa5,sizeof(out));assert(!a04_native_get(&state,2,0,out,&bytes)&&bytes==16);
 assert(!out[0]&&out[4]==1&&out[8]==8&&!out[12]&&out[16]==0xa5);
 bytes=4;assert(!a04_native_get(&state,21,0,out,&bytes)&&!out[0]); /* associated but key handshake pending */
 assert(!a04_wlan_port_allowed(&r.station,&r.security,0)&&a04_wlan_port_allowed(&r.station,&r.security,1));
 r.station.sta_active=0;memset(&state,0,sizeof(state));state.control_ready=1;a04_wlan_network_profile(&r,&state,1);assert(!state.associated&&!state.preauth_ready&&!state.auth_profile_valid&&!state.authorized);
 assert(a04_wlan_personal_drive(&r,&h,r.personal.deadline)==A04_WLAN_TIMEOUT&&!a04_psk_authenticated(&r.personal));
 return 0;}
''')

# Actual synchronous HIF callback is simulated by the test card; no HW proof.
COMPLETE_FIXTURE=r'''
static void actual_runtime_to_keys(struct a04_wlan_runtime *r,struct a04_wlan_host *h){
 memset(r,0,sizeof(*r));local_profile(&r->station.local);ap_profile(&r->station.ap);
 memcpy(r->station.ap.rsn,rsn_ref,sizeof(rsn_ref));r->station.ap.rsn_length=sizeof(rsn_ref);r->station.ap.capability|=16;
 r->station.configured=r->station.selected=r->station.sta_active=r->station.secured=1;r->station.join.phase=4;r->station.join.aid=42;
 r->configured=r->admin=r->personal_admitted=1;r->auth_lower=1;r->auth_upper=8;r->association_generation=7;
 assert(!a04_psk_prepare(&r->credential,&crypto,(const uint8_t *)"password",8,1,(const uint8_t *)"test",4));
 assert(!a04_wlan_personal_start(r,&crypto,100));
 struct a04_psk4way work=r->personal;assert(!a04_psk_rx(&work,one,sizeof(one),150));assert(!a04_wlan_personal_observed(r,&work));
 assert(!a04_wlan_personal_drive(r,h,200));uint8_t done[16]={0};done[0]=(uint8_t)r->data.pid;done[4]=(uint8_t)r->station.local.wlan;
 struct a04_wlan_event e={0x0f,0,16,done};assert(!a04_wlan_runtime_event(r,h,&e,210));
 work=r->personal;assert(!a04_psk_rx(&work,three,sizeof(three),250));assert(!a04_wlan_personal_observed(r,&work));
}
static void host_complete_after_real_crypto(struct a04_wlan_runtime *r,struct a04_wlan_host *h){
 actual_runtime_to_keys(r,h);assert(!a04_wlan_personal_drive(r,h,300));uint8_t done[16]={0};done[0]=(uint8_t)r->station.local.bss;memcpy(done+2,r->station.ap.mac,6);
 struct a04_wlan_event e={0x24,0,8,done};assert(!a04_wlan_runtime_event(r,h,&e,320));assert(!a04_wlan_personal_drive(r,h,330));
 assert(r->security.gtk_accepted&&r->personal.gtk_state==2);
 assert(!a04_wlan_personal_drive(r,h,350));assert(r->data.eapol_kind==4&&r->data.pending);
 memset(done,0,16);done[0]=(uint8_t)r->data.pid;done[4]=(uint8_t)r->station.local.wlan;
 e.id=0x0f;e.length=16;assert(!a04_wlan_runtime_event(r,h,&e,360));
 assert(!a04_wlan_security_authorize(&r->security,0,370));assert(a04_psk_authenticated(&r->personal)&&r->security.authorized);
}
'''

class PersonalRuntimeRegression(unittest.TestCase):
 production=True
 run_c=owner.PersonalState.run_c
 def runtime(self,body):
  self.run_c('#include "'+str(ROOT/'iokit/Kernel/arm/sdio/A04BR3WLANPersonalRuntime.h')+'"\n'+COMPLETE_FIXTURE+body)
 def test_completed_idle_past_ten_seconds_then_actual_ip_tx(self):
  self.runtime(r'''
int main(void){struct card c;struct a04_sdio_io io;struct a04_sdio_transport t;struct a04_wlan_host h;
 setup(&c,&io,&t);assert(!a04_wlan_host_open(&h,&t));h.ready=1;assert(!a04_wlan_runtime_begin(&h));
 struct a04_wlan_runtime r;host_complete_after_real_crypto(&r,&h);uint64_t now=r.personal.deadline+1000000;
 assert(!a04_wlan_personal_drive(&r,&h,now));assert(!r.personal.failed&&r.security.authorized);
 assert(a04_wlan_backend_receive(&h,&r.station,&r.security,&r.data,now,0)==A04_WLAN_PENDING);
 assert(!h.failed&&a04_wlan_port_allowed(&r.station,&r.security,0));
 uint8_t eth[54]={0};memcpy(eth,r.station.ap.mac,6);memcpy(eth+6,r.station.local.mac,6);eth[12]=8;
 uint64_t deadline=0;assert(!a04_wlan_data_deadline(&r.security,0,now,&deadline)&&deadline==now+900000&&deadline>r.security.deadline_us);
 assert(!a04_wlan_ethernet_send(&h,&r.station,&r.security,&r.data,eth,sizeof(eth),a04_psk_verify,&r.personal,now,deadline));
 assert(r.data.pending&&!r.data.eapol_kind&&(h.tx[6]&0x80));uint8_t done[16]={0};done[0]=(uint8_t)r.data.pid;done[4]=(uint8_t)r.station.local.wlan;
 struct a04_wlan_event e={0x0f,0,16,done};assert(!a04_wlan_runtime_event(&r,&h,&e,now+100));
 assert(!r.data.pending&&!h.failed&&r.security.authorized&&!a04_wlan_personal_drive(&r,&h,now+200));
 assert(a04_wlan_data_deadline(&r.security,1,now,&deadline)==A04_WLAN_TIMEOUT); // old EAPOL window remains bounded
 struct a04_psk4way before=r.personal;assert(a04_psk_rx(&r.personal,three,sizeof(three),now)==A04_WLAN_PENDING);
 assert(!memcmp(&before,&r.personal,sizeof(before))); // late duplicate cannot reopen/reinstall/kill association
 actual_runtime_to_keys(&r,&h);assert(a04_wlan_personal_drive(&r,&h,r.personal.deadline)==A04_WLAN_TIMEOUT);
 return 0;}
''')
 def test_unbound_same_epoch_result_cannot_restore_failed_keys(self):
  self.runtime(r'''
int main(void){struct card c;struct a04_sdio_io io;struct a04_sdio_transport t;struct a04_wlan_host h;
 setup(&c,&io,&t);assert(!a04_wlan_host_open(&h,&t));h.ready=1;assert(!a04_wlan_runtime_begin(&h));
 struct a04_wlan_runtime r;actual_runtime_to_keys(&r,&h);struct a04_psk4way computed=r.personal;
 assert(a04_wlan_personal_commit_allowed(&r,7,11,11,1));
 assert(!a04_wlan_personal_commit_allowed(&r,7,11,12,1)); // bind/unbind consumer epoch
 assert(!a04_wlan_personal_commit_allowed(&r,7,11,11,0)); // old retained client no longer bound
 assert(!a04_wlan_personal_commit_allowed(&r,8,11,11,1)); // station epoch
 assert(a04_psk_fail(&r.personal,A04_WLAN_STATE)==A04_WLAN_STATE);
 assert(!a04_wlan_personal_commit_allowed(&r,7,11,11,1)); // same-generation poison alone suffices
 assert(a04_wlan_personal_observed(&r,&computed)==A04_WLAN_STATE);
 assert(r.personal.failed==A04_WLAN_STATE&&a04_psk_zero(r.personal.ptk,64)&&!a04_psk_authenticated(&r.personal));
 r.personal=computed;r.teardown.pending=1;assert(!a04_wlan_personal_commit_allowed(&r,7,11,11,1));assert(a04_wlan_personal_observed(&r,&computed));
 r.teardown.pending=0;r.security.failed=A04_WLAN_STATE;assert(!a04_wlan_personal_commit_allowed(&r,7,11,11,1));assert(a04_wlan_personal_observed(&r,&computed));
 return 0;}
''')

if __name__=='__main__':unittest.main()
