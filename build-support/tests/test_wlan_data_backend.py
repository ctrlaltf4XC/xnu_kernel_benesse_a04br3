# SPDX-License-Identifier: MIT
import unittest
import test_wmt_protocol as codecs
from test_wlan_backend import SETUP

class WLANDataBackend(unittest.TestCase):
    run_c=codecs.WMTProtocol.run_c

    def test_tx_descriptor_completion_and_page_returns_are_independent(self):
        self.run_c(SETUP+r'''
int main(void){
 struct card c;struct a04_sdio_io io;struct a04_sdio_transport t;struct a04_wlan_host h;
 setup(&c,&io,&t);assert(!a04_wlan_host_open(&h,&t));h.ready=1;assert(!a04_wlan_runtime_begin(&h));
 struct a04_wlan_station s;memset(&s,0,sizeof(s));local_profile(&s.local);ap_profile(&s.ap);s.sta_active=1;s.join.phase=4;
 struct a04_wlan_security sec;struct a04_wlan_data_state d;memset(&sec,0,sizeof(sec));memset(&d,0,sizeof(d));
 uint8_t eth[1514];memset(eth,0x55,sizeof(eth));memcpy(eth,s.ap.mac,6);memcpy(eth+6,s.local.mac,6);eth[12]=8;eth[13]=0;
 assert(!a04_wlan_ethernet_send(&h,&s,&sec,&d,eth,sizeof(eth),0,0,100,900000));
 assert(d.pending && h.tc0_free==0 && h.tc0_used==13 && a04_wmt_le16(h.tx)==1542);
 assert(h.tx[2]==20 && h.tx[3]==0 && h.tx[4]==s.local.wlan && h.tx[5]==0x90 && !(h.tx[6]&0x80));
 assert(h.tx[7]==4 && h.tx[20]==1 && h.tx[21]==2 && a04_wmt_le16(h.tx+12)==0x800);
 assert(!memcmp(h.tx+28,eth,sizeof(eth)));
 unsigned txs=c.txs;assert(a04_wlan_ethernet_send(&h,&s,&sec,&d,eth,60,0,0,101,900000)==A04_WLAN_STATE && c.txs==txs);
 uint8_t status[16];memset(status,0,16);status[0]=1;status[4]=(uint8_t)s.local.wlan;
 struct a04_wlan_event v={0x0f,0,16,status};assert(!a04_wlan_data_tx_event(&h,&s,&sec,&d,&v,200));
 assert(!d.pending && d.confirmed==1 && h.tc0_free==0 && h.tc0_used==13);
 c.ac=13;c.ffa=13;
 assert(!a04_wlan_ethernet_send(&h,&s,&sec,&d,eth,60,0,0,300,900000));
 assert(h.tc0_free==12 && h.tc0_used==1 && d.pid==2);
 status[0]=2;status[1]=1;assert(a04_wlan_data_tx_event(&h,&s,&sec,&d,&v,400)==A04_WLAN_STATUS && h.failed && !sec.authorized);
 txs=c.txs;assert(a04_wlan_ethernet_send(&h,&s,&sec,&d,eth,60,0,0,500,900000)==A04_WLAN_STATE && c.txs==txs);
 return 0;
}
''')

    def test_rx_translation_group_bounds_controlled_port_and_deauth(self):
        self.run_c(SETUP+r'''
int main(void){
 struct a04_wlan_station s;memset(&s,0,sizeof(s));local_profile(&s.local);ap_profile(&s.ap);s.sta_active=1;s.join.phase=4;s.secured=1;
 struct a04_wlan_security sec;memset(&sec,0,sizeof(sec));sec.enabled=1;
 uint8_t p[160];struct a04_wlan_ethernet out;
 for(unsigned groups=0;groups<16;groups++)for(unsigned offset=0;offset<2;offset++){
  memset(p,0,sizeof(p));unsigned head=16+((groups&8)?16:0)+((groups&1)?16:0)+((groups&2)?8:0)+((groups&4)?24:0)+2*offset;
  unsigned n=head+60;a04_wmt_put16(p,n);a04_wmt_put16(p+2,0x4000|(groups<<9));p[6]=(uint8_t)(0x80|14|(offset<<6));
  p[8]=(uint8_t)s.local.wlan;p[9]=0x40;memcpy(p+head,s.local.mac,6);memcpy(p+head+6,s.ap.mac,6);p[head+12]=8;
  sec.authorized=0;assert(a04_wlan_ethernet_rx(&s,&sec,p,n,&out)==A04_WLAN_SECURITY);
  sec.authorized=1;sec.ptk_confirmed=1;sec.rx_pn[0]=0;
  if(groups&1){p[16+((groups&8)?16:0)]=1;
   assert(!a04_wlan_ethernet_rx(&s,&sec,p,n,&out) && out.data==p+head && out.length==60 && out.cipher==4);
   assert(a04_wlan_ethernet_rx(&s,&sec,p,n,&out)==A04_WLAN_REPLAY);
  }else assert(a04_wlan_ethernet_rx(&s,&sec,p,n,&out)==A04_WLAN_SECURITY);
  for(unsigned i=0;i<n;i++)assert(a04_wlan_ethernet_rx(&s,&sec,p,i,&out));
  p[9]=0;assert(a04_wlan_ethernet_rx(&s,&sec,p,n,&out)==A04_WLAN_SECURITY);p[9]=0x40;
  p[10]=0x10;assert(a04_wlan_ethernet_rx(&s,&sec,p,n,&out)==A04_WLAN_FORMAT);p[10]=0;
  p[8]++;assert(a04_wlan_ethernet_rx(&s,&sec,p,n,&out)==A04_WLAN_SEQUENCE);p[8]--;
  p[head+12]=0x88;p[head+13]=0x8e;sec.authorized=0;
  p[9]=0; /* pre-key EAPOL may be unencrypted, never normal data */
  assert(!a04_wlan_ethernet_rx(&s,&sec,p,n,&out) && out.eapol);
 }
 uint8_t deauth[30];response(deauth,&s,0xc0);a04_wmt_put16(deauth+24,3);sec.authorized=1;
 deauth[10]^=1;assert(a04_wlan_disconnect_rx(&s,&sec,deauth,26)==A04_WLAN_SEQUENCE && s.sta_active);deauth[10]^=1;
 assert(!a04_wlan_disconnect_rx(&s,&sec,deauth,26) && !s.sta_active && !sec.authorized && s.failed);
 assert(!a04_wlan_port_allowed(&s,&sec,1));
 return 0;
}
''')

    def test_empty_fifo_pending_deadline_poison_and_wrong_sta_event(self):
        self.run_c(SETUP+r'''
int main(void){
 struct card c;struct a04_sdio_io io;struct a04_sdio_transport t;struct a04_wlan_host h;
 setup(&c,&io,&t);assert(!a04_wlan_host_open(&h,&t));h.ready=1;assert(!a04_wlan_runtime_begin(&h));
 struct a04_wlan_station s;struct a04_wlan_security sec;struct a04_wlan_data_state d;
 memset(&s,0,sizeof(s));memset(&sec,0,sizeof(sec));memset(&d,0,sizeof(d));s.deadline_us=1000;
 for(unsigned now=1;now<100;now++)assert(a04_wlan_backend_receive(&h,&s,&sec,&d,now,now&1)==A04_WLAN_PENDING && !h.failed);
 assert(a04_wlan_backend_receive(&h,&s,&sec,&d,1000,0)==A04_WLAN_TIMEOUT && h.failed && !sec.authorized);
 unsigned reads=c.reads;assert(a04_wlan_backend_receive(&h,&s,&sec,&d,1001,0)==A04_WLAN_STATE && c.reads==reads);
 setup(&c,&io,&t);assert(!a04_wlan_host_open(&h,&t));memset(&s,0,sizeof(s));local_profile(&s.local);ap_profile(&s.ap);
 s.sta_pending=1;s.sta_sequence=3;s.deadline_us=1000;
 uint8_t payload[8];memcpy(payload,s.ap.mac,6);payload[6]=2;payload[7]=1;
 struct a04_wlan_event v={0x0c,4,8,payload};
 assert(a04_wlan_station_event(&s,&h,&v,10)==A04_WLAN_SEQUENCE && !s.sta_active && s.failed && h.failed);
 return 0;
}
''')

if __name__=='__main__':unittest.main()
