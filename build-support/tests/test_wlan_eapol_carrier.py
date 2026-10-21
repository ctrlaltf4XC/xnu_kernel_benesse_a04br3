# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Existing real HMAC-SHA1 vectors; no fake MIC/association completion."""
from pathlib import Path
import unittest
import test_wlan_security_backend as security_tests
HEADER=Path(__file__).resolve().parents[2]/'iokit/Kernel/arm/sdio/A04BR3WLANEAPOLCarrier.h'
class EAPOLCarrier(unittest.TestCase):
 run_c=security_tests.WLANSecurityBackend.run_c
 def test_native_ethernet_boundary_and_crypto_preflight(self):
  self.run_c('#include "'+str(HEADER)+'"\n'+r'''
static unsigned sends;static int send_error;
static int send_checked(void *context,const uint8_t *p,unsigned n){
 assert(context==&sends&&p&&n==14+sizeof(m2));sends++;return send_error;
}
int main(void){
 struct a04_wlan_station sta;struct a04_wlan_security sec;associated(&sta,&sec);
 uint8_t eth[512];memcpy(eth,sta.ap.mac,6);memcpy(eth+6,sta.local.mac,6);eth[12]=0x88;eth[13]=0x8e;
 memcpy(eth+14,m2,sizeof(m2));unsigned n=14+sizeof(m2);
 struct a04_wlan_eapol_carrier v;
 assert(!a04_wlan_eapol_carrier_decode(&sta,eth,n,1,&v)&&v.kind==2&&v.eapol_bytes==sizeof(m2));
 assert(a04_wlan_eapol_carrier_decode(&sta,eth,n,0,&v)==A04_WLAN_SEQUENCE);
 eth[12]=0x81;assert(a04_wlan_eapol_carrier_decode(&sta,eth,n,1,&v)==A04_WLAN_UNSUPPORTED);eth[12]=0x88;
 eth[0]^=1;assert(a04_wlan_eapol_carrier_decode(&sta,eth,n,1,&v)==A04_WLAN_SEQUENCE);eth[0]^=1;
 for(unsigned len=0;len<n;len++)assert(a04_wlan_eapol_carrier_decode(&sta,eth,len,1,&v)!=0);
 assert(a04_wlan_eapol_carrier_decode(&sta,eth,1515,1,&v)==A04_WLAN_ARGUMENT);
 assert(!a04_wlan_security_rx(&sec,m1,sizeof(m1),verify,0,2000));
 struct a04_wlan_eapol_sender sender={&sends,send_checked};
 assert(a04_wlan_eapol_carrier_send(&sta,&sec,eth,n,0,0,3000,&sender)==A04_WLAN_ARGUMENT&&!sends);
 eth[14+81]^=1;assert(a04_wlan_eapol_carrier_send(&sta,&sec,eth,n,verify,0,3000,&sender)==A04_WLAN_SECURITY&&!sends);eth[14+81]^=1;
 assert(!a04_wlan_eapol_carrier_send(&sta,&sec,eth,n,verify,0,3000,&sender)&&sends==1);
 assert(sec.phase==1&&!sec.authorized&&!sec.pending_tx&&!sec.ptk_confirmed);
 send_error=A04_WLAN_STATUS;
 assert(a04_wlan_eapol_carrier_send(&sta,&sec,eth,n,verify,0,3000,&sender)==A04_WLAN_STATUS&&sends==2);
 assert(!sec.authorized&&!sec.pending_tx);
 assert(a04_wlan_eapol_carrier_send(&sta,&sec,eth,n,verify,0,sec.deadline_us,&sender)!=0&&sends==2);
 sta.sta_active=0;assert(a04_wlan_eapol_carrier_decode(&sta,eth,n,1,&v)==A04_WLAN_STATE);sta.sta_active=1;
 memcpy(eth,sta.local.mac,6);memcpy(eth+6,sta.ap.mac,6);memcpy(eth+14,m1,sizeof(m1));
 assert(!a04_wlan_eapol_carrier_decode(&sta,eth,14+sizeof(m1),0,&v)&&v.kind==1);
 memcpy(eth+14,m3,sizeof(m3));assert(!a04_wlan_eapol_carrier_decode(&sta,eth,14+sizeof(m3),0,&v)&&v.kind==3);
 assert(!sec.authorized);
 memcpy(eth,sta.ap.mac,6);memcpy(eth+6,sta.local.mac,6);memcpy(eth+14,m4,sizeof(m4));
 assert(!a04_wlan_eapol_carrier_decode(&sta,eth,14+sizeof(m4),1,&v)&&v.kind==4);
 assert(a04_wlan_eapol_carrier_send(&sta,&sec,eth,14+sizeof(m4),verify,0,3000,&sender)!=0&&sends==2);
 assert(!sec.authorized);return 0;
}
''')
if __name__=='__main__':unittest.main()
