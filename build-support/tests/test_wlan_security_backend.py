# SPDX-License-Identifier: MIT
"""Real HMAC-SHA1 verifier in test adapter; production requires native verifier.
Synthetic keys only. No keys, packets, or firmware from a real network.
"""
import hashlib
import hmac
import subprocess
import tempfile
import unittest
from pathlib import Path
import test_wmt_protocol as codecs
from test_wlan_backend import SETUP

RSN=bytes.fromhex('30140100000fac040100000fac040100000fac020000')
KCK=bytes(range(1,17))

def eapol(kind):
    info={1:0x008a,2:0x010a,3:0x13ca,4:0x030a}[kind]
    data=RSN if kind==2 else bytes(range(24)) if kind==3 else b''
    p=bytearray(99+len(data));p[0]=2;p[1]=3;p[2:4]=(len(p)-4).to_bytes(2,'big');p[4]=2
    p[5:7]=info.to_bytes(2,'big');p[7:9]=(16 if kind in (1,3) else 0).to_bytes(2,'big')
    p[9:17]=(1 if kind in (1,2) else 2).to_bytes(8,'big')
    if kind!=4:p[17:49]=bytes([0x22 if kind==2 else 0x11])*32
    p[97:99]=len(data).to_bytes(2,'big');p[99:]=data
    if kind!=1:p[81:97]=hmac.new(KCK,p,hashlib.sha1).digest()[:16]
    return p

VECTORS='\n'.join('static const uint8_t m%d[]={%s};'%(k,','.join(str(x) for x in eapol(k))) for k in range(1,5))
VERIFY=r'''
#include <openssl/hmac.h>
static inline int verify(void *ctx,const uint8_t *p,unsigned n,unsigned off,unsigned count){
 (void)ctx;assert(off==81 && count==16 && n<=2048);
 uint8_t input[2048],digest[EVP_MAX_MD_SIZE],key[16];unsigned size=0;
 memcpy(input,p,n);memset(input+off,0,count);for(unsigned i=0;i<16;i++)key[i]=(uint8_t)(i+1);
 assert(HMAC(EVP_sha1(),key,16,input,n,digest,&size) && size==20);
 int bad=memcmp(digest,p+off,16)!=0;a04_wlan_secret_clear(input,sizeof(input));a04_wlan_secret_clear(digest,sizeof(digest));return bad;
}
static inline void associated(struct a04_wlan_station *sta,struct a04_wlan_security *s){
 memset(sta,0,sizeof(*sta));memset(s,0,sizeof(*s));local_profile(&sta->local);ap_profile(&sta->ap);
 unsigned n=0;const uint8_t *rsn=a04_wlan_rsn_selected(&n);memcpy(sta->ap.rsn,rsn,n);sta->ap.rsn_length=n;
 sta->ap.capability|=16;sta->secured=1;sta->sta_active=1;sta->join.phase=4;
 assert(!a04_wlan_security_begin(s,sta,1000,1000000));
}
'''

class WLANSecurityBackend(unittest.TestCase):
    def run_c(self, body):
        source='#include <stdint.h>\n#include <assert.h>\n#include <string.h>\n'+SETUP+VECTORS+VERIFY+body
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);(p/'case.c').write_text(source)
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                            '-I',str(codecs.INCLUDE),str(p/'case.c'),'-lcrypto','-o',str(p/'case')],check=True)
            subprocess.run([str(p/'case')],check=True)
            subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-Wno-unused-const-variable',
                            '-I',str(codecs.INCLUDE),'-x','c++','-fsyntax-only',str(p/'case.c')],check=True)

    def test_verified_handshake_keys_and_actual_tx_done_port_gate(self):
        self.run_c(r'''
int main(void){
 struct card c;struct a04_sdio_io io;struct a04_sdio_transport t;struct a04_wlan_host h;
 setup(&c,&io,&t);assert(!a04_wlan_host_open(&h,&t));h.ready=1;assert(!a04_wlan_runtime_begin(&h));
 struct a04_wlan_station sta;struct a04_wlan_security sec;associated(&sta,&sec);
 assert(!a04_wlan_port_allowed(&sta,&sec,0) && a04_wlan_port_allowed(&sta,&sec,1));
 assert(!a04_wlan_security_rx(&sec,m1,sizeof(m1),verify,0,2000) && sec.phase==1);
 uint8_t eth[256];memcpy(eth,sta.ap.mac,6);memcpy(eth+6,sta.local.mac,6);eth[12]=0x88;eth[13]=0x8e;
 memcpy(eth+14,m2,sizeof(m2));struct a04_wlan_data_state d;memset(&d,0,sizeof(d));
 assert(!a04_wlan_ethernet_send(&h,&sta,&sec,&d,eth,14+sizeof(m2),verify,0,3000,900000));
 assert(sec.phase==1 && sec.pending_tx==2 && d.pending && h.tx[3]==0x88 && h.tx[11]==0x80 && !(h.tx[6]&0x80));
 uint8_t status[16];memset(status,0,16);status[0]=(uint8_t)d.pid;status[4]=(uint8_t)sta.local.wlan;
 struct a04_wlan_event v={0x0f,0,16,status};unsigned free_pages=h.tc0_free;
 assert(!a04_wlan_data_tx_event(&h,&sta,&sec,&d,&v,4000) && sec.phase==2 && h.tc0_free==free_pages);
 assert(!a04_wlan_security_rx(&sec,m3,sizeof(m3),verify,0,5000) && sec.phase==3);
 uint8_t key[16],rsc[8];memset(key,0x55,16);memset(rsc,0,8);
 assert(!a04_wlan_security_install(&sec,&sta,&h,1,0,key,rsc,5100) && sec.ptk_pending && !sec.ptk_confirmed);
 for(unsigned i=0;i<sizeof(h.tx);i++)assert(!h.tx[i]); /* never retain keys in FIFO scratch */
 assert(a04_wlan_security_authorize(&sec,0,7100)==A04_WLAN_STATE);
 uint8_t done[8];memset(done,0,8);done[0]=(uint8_t)sta.local.bss;memcpy(done+2,sta.ap.mac,6);
 v.id=0x24;v.length=8;v.payload=done;assert(!a04_wlan_security_key_event(&sec,&sta,&v,5200) && sec.ptk_confirmed);
 assert(a04_wlan_security_install(&sec,&sta,&h,1,0,key,rsc,5100)==A04_WLAN_STATE); /* no reinstall */
 assert(!a04_wlan_security_install(&sec,&sta,&h,0,1,key,rsc,5500) && sec.gtk_accepted);
 assert(!sec.authorized && !sec.m4_confirmed);
 memcpy(eth+14,m4,sizeof(m4));assert(!a04_wlan_ethernet_send(&h,&sta,&sec,&d,eth,14+sizeof(m4),verify,0,6000,900000));
 assert((h.tx[6]&0x80) && sec.pending_tx==4 && !sec.m4_confirmed);
 status[0]=(uint8_t)d.pid;v.id=0x0f;v.length=16;v.payload=status;
 assert(!a04_wlan_data_tx_event(&h,&sta,&sec,&d,&v,7000) && sec.m4_confirmed);
 /* Full synchronous GTK HIF acceptance plus real M4 completion permits IP. */
 assert(!a04_wlan_security_authorize(&sec,0,7100) && sec.authorized && sec.gtk_accepted && a04_wlan_port_allowed(&sta,&sec,0));
 return 0;
}
''')

    def test_bad_mic_replay_nonce_missing_verifier_and_wire_lengths(self):
        self.run_c(r'''
int main(void){
 for(unsigned fault=0;fault<5;fault++){
  struct a04_wlan_station sta;struct a04_wlan_security sec;associated(&sta,&sec);
  assert(!a04_wlan_security_rx(&sec,m1,sizeof(m1),verify,0,2000));
  unsigned kind=0;assert(!a04_wlan_security_tx_check(&sec,m2,sizeof(m2),verify,0,3000,&kind) && kind==2);
  sec.pending_tx=2;assert(!a04_wlan_security_tx_done(&sec,2,0));
  uint8_t bad[sizeof(m3)];memcpy(bad,m3,sizeof(bad));
  if(!fault)bad[81]^=1;
  if(fault==1)bad[16]=1;
  if(fault==2)bad[17]^=1;
  if(fault==3)bad[98]++;
  assert(a04_wlan_security_rx(&sec,bad,sizeof(bad),fault==4?0:verify,0,4000));
  assert(sec.failed && !sec.authorized && !sec.ptk_pending);
  assert(a04_wlan_security_rx(&sec,m3,sizeof(m3),verify,0,4000)==A04_WLAN_STATE);
 }
 struct a04_wlan_eapol v;
 for(unsigned n=0;n<sizeof(m3);n++)assert(a04_wlan_eapol_parse(m3,n,&v));
 uint8_t bad[sizeof(m2)];memcpy(bad,m2,sizeof(bad));bad[8]=16;
 assert(a04_wlan_eapol_parse(bad,sizeof(bad),&v)==A04_WLAN_SECURITY);
 assert(!a04_wlan_eapol_parse(m4,sizeof(m4),&v) && v.kind==4);
 return 0;
}
''')

if __name__=='__main__':unittest.main()
