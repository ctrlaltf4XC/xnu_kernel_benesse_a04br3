# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Production PSK owner + production PersonalCrypto, independent Python AP peer.

Host-only synthetic association, public test keys and deterministic SNonce.
ALL TX/key completion events here are HOST SIMULATED, not hardware ACK evidence.
No cryptographic-success mocks: PMK/PTK/MIC/unwrap callbacks invoke actual kernel
BearSSL-backed adapter; AP oracle uses Python hashlib/hmac + OpenSSL AES-wrap.
Requires Python stdlib, cc/clang, system OpenSSL CLI; no pip/local research tree.
"""
import ctypes as C
import hashlib
import hmac
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
INCLUDE = ROOT/'iokit/Kernel/arm/sdio'
BEAR = ROOT/'libkern/crypto/a04br3_bearssl'
SOURCES = [BEAR/'src'/p for p in ('hash/sha1.c', 'codec/dec32be.c', 'codec/enc32be.c',
                               'symcipher/aes_ct.c', 'symcipher/aes_ct_dec.c',
                               'symcipher/aes_ct_cbcdec.c')]
RSN = bytes.fromhex('30140100000fac040100000fac040100000fac020000')
AP = bytes.fromhex('02060708090a')
STA = bytes.fromhex('020102030405')
ANONCE = bytes(range(1, 33))
SNONCE = bytes(range(101, 133))
GTK = bytes(range(16))
RSC = bytes.fromhex('0605040302010000')
PASSPHRASE = b'actual-test-password'
SSID = b'peer\0binary-ssid'
WAIT_M1, M2_READY, WAIT_M3, KEYS, M4_READY, COMPLETE, FAILED = range(1, 8)

DRIVER = r'''
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "A04BR3WLANPSK4Way.h"
#include "A04BR3WLANPersonalCrypto.h"
/* One explicitly serialized HOST TEST session; no production runtime added. */
static struct {
 struct a04_psk4way state;
 struct a04_wlan_station station;struct a04_psk_prepared prepared;
 uint8_t nonce[32]; unsigned random_mode,calls[5],takes[2];
} fixture;
static int real_pmk(void *c,const uint8_t *p,unsigned n,const uint8_t *s,unsigned sn,uint8_t out[32]) {
 (void)c;fixture.calls[0]++;return a04_wlan_personal_pmk(p,n,s,sn,out);
}
static int fixture_random(void *c,uint8_t out[32]) {
 (void)c;fixture.calls[1]++;
 if(fixture.random_mode==1)return 77; /* explicit RNG failure, not success */
 if(fixture.random_mode==2)memset(out,0,32);
 else memcpy(out,fixture.nonce,32); /* declared deterministic TEST INPUT */
 return 0;
}
static int real_ptk(void *c,const uint8_t p[32],const uint8_t a[6],const uint8_t s[6],const uint8_t an[32],const uint8_t sn[32],uint8_t out[64]) {
 (void)c;fixture.calls[2]++;return a04_wlan_personal_ptk(p,a,s,an,sn,out);
}
static int real_mic(void *c,const uint8_t k[16],const uint8_t *p,unsigned n,uint8_t out[16]) {
 (void)c;fixture.calls[3]++;return a04_wlan_personal_mic(k,p,n,out);
}
static int real_unwrap(void *c,const uint8_t k[16],const uint8_t *p,unsigned n,uint8_t *out,unsigned cap,unsigned *bytes) {
 (void)c;fixture.calls[4]++;return a04_wlan_personal_unwrap(k,p,n,out,cap,bytes);
}
int fixture_begin(const uint8_t *secret,unsigned n,const uint8_t *ssid,unsigned sn,
 const uint8_t ap[6],const uint8_t sta[6],const uint8_t nonce[32],unsigned raw,
 uint64_t generation,unsigned random_mode) {
 memset(&fixture,0,sizeof(fixture));if(sn>32)return -1;
 memcpy(fixture.nonce,nonce,32);fixture.random_mode=random_mode;
 struct a04_psk_crypto crypto={0,real_pmk,fixture_random,real_ptk,real_mic,real_unwrap};
 /* CPU derivation BEFORE even constructing the synthetic association. */
 int e=a04_psk_prepare(&fixture.prepared,&crypto,secret,n,!raw,ssid,sn);
 if(e)return e;
 struct a04_wlan_station station;memset(&station,0,sizeof(station));
 memcpy(station.ap.mac,ap,6);memcpy(station.local.mac,sta,6);
 memcpy(station.ap.ssid,ssid,sn);station.ap.ssid_length=sn;
 unsigned rsn_bytes=0;const uint8_t *rsn=a04_wlan_rsn_selected(&rsn_bytes);
 memcpy(station.ap.rsn,rsn,rsn_bytes);station.ap.rsn_length=rsn_bytes;
 station.secured=station.sta_active=1;station.join.phase=4;
 fixture.station=station;
 return a04_psk_begin_prepared(&fixture.state,&station,&crypto,&fixture.prepared,generation,100,1000100);
}
int fixture_rx(const uint8_t *p,unsigned n,uint64_t now){return a04_psk_rx(&fixture.state,p,n,now);}
int fixture_tx(uint8_t *p,unsigned cap,unsigned *n,uint64_t *cookie,uint64_t now){
 const uint8_t *borrowed=0;unsigned bytes=0;
 assert(cap>=121);int e=a04_psk_tx_take(&fixture.state,now,&borrowed,&bytes,cookie);
 if(!e){assert(bytes<=cap);memcpy(p,borrowed,bytes);*n=bytes;}else *n=0;
 return e;
}
int fixture_packet_snapshot(uint8_t *out,unsigned cap){
 assert(cap>=121);assert(fixture.state.tx_bytes<=cap);
 memcpy(out,fixture.state.tx,fixture.state.tx_bytes);return (int)fixture.state.tx_bytes;
}
/* Explicit HOST-SIMULATED completion, never a claim of NIC TX completion. */
int fixture_tx_ack(uint64_t cookie,unsigned status,uint64_t now){return a04_psk_tx_done(&fixture.state,cookie,status,now);}
int fixture_key(unsigned pair,uint8_t out[16],uint8_t rsc[8],unsigned *id,uint64_t *cookie,uint64_t now){
 const uint8_t *key=0,*counter=0;
 int e=a04_psk_key_take(&fixture.state,pair,now,&key,id,&counter,cookie);
 if(!e){assert(pair<=1);fixture.takes[pair]++;memcpy(out,key,16);memcpy(rsc,counter,8);}return e;
}
/* Explicit HOST-SIMULATED correlated firmware key completion. */
int fixture_key_ack(unsigned pair,uint64_t cookie,unsigned status,uint64_t now){return a04_psk_key_done(&fixture.state,pair,cookie,status,now);}
int fixture_completion(void){return a04_psk_completion_take(&fixture.state);}
unsigned fixture_probe(unsigned field){
 struct a04_psk4way *s=&fixture.state;
 switch(field){
 case 0:return s->phase;case 1:return a04_psk_authenticated(s);
 case 2:return s->ptk_state;case 3:return s->gtk_state;case 4:return s->tx_pending;
 case 5:return s->tx_attempts;case 6:return fixture.takes[1];case 7:return fixture.takes[0];
 case 8:return s->m4_confirmed;case 9:return s->completion_reported;
 case 15:return fixture.prepared.ready;
 case 16:return !a04_psk_zero((const uint8_t *)&fixture.prepared,sizeof(fixture.prepared));
 default:return field>=10&&field<15?fixture.calls[field-10]:999;
 }
}
int fixture_secret(unsigned which,uint8_t out[64]){
 switch(which){case 0:memcpy(out,fixture.state.pmk,32);return 32;
 case 1:memcpy(out,fixture.state.ptk,64);return 64;
 case 2:memcpy(out,fixture.state.gtk,16);return 16;
 case 3:memcpy(out,fixture.state.snonce,32);return 32;default:return -1;}
}
/* Exercise V2 one-use preparation contract without changing production code. */
int fixture_admission(unsigned mode,unsigned *wiped,unsigned *reused,unsigned *active){
 struct a04_psk_prepared prepared;memset(&prepared,0,sizeof(prepared));
 struct a04_wlan_station sta=fixture.station;
 struct a04_psk4way state;memset(&state,0,sizeof(state));
 struct a04_psk_crypto crypto={0,real_pmk,fixture_random,real_ptk,real_mic,real_unwrap};
 int e=a04_psk_prepare(&prepared,&crypto,fixture.state.pmk,32,0,sta.ap.ssid,sta.ap.ssid_length);
 assert(!e);if(mode==1)sta.ap.ssid[5]^=1;
 if(mode==2)sta.sta_active=0;
 if(mode==4){
  unsigned before=fixture.calls[0];
  e=a04_psk_begin(&state,&sta,&crypto,fixture.state.pmk,32,1,1,100,1000100);
  assert(before==fixture.calls[0]);a04_psk_prepared_clear(&prepared);
 }else e=a04_psk_begin_prepared(&state,&sta,&crypto,&prepared,mode==3?0:1,100,1000100);
 *wiped=a04_psk_zero((const uint8_t *)&prepared,sizeof(prepared));
 *reused=(unsigned)a04_psk_begin_prepared(&state,&sta,&crypto,&prepared,2,100,1000100);
 *active=state.phase;return e;
}

'''


def peer_ptk(pmk, ap=AP, sta=STA, anonce=ANONCE, snonce=SNONCE):
    data=b''.join(sorted((ap,sta)))+b''.join(sorted((anonce,snonce)))
    return b''.join(hmac.digest(pmk,b'Pairwise key expansion\0'+data+bytes([i]),'sha1')
                    for i in range(4))[:64]


def peer_sign(packet, ptk):
    p=bytearray(packet);p[81:97]=bytes(16)
    p[81:97]=hmac.digest(ptk[:16],p,'sha1')[:16]
    return bytes(p)


def peer_packet(kind, replay, ptk=None, payload=b'', nonce=None, rsc=None):
    p=bytearray(99+len(payload));p[:2]=b'\x02\x03'
    p[2:4]=(len(p)-4).to_bytes(2,'big');p[4]=2
    p[5:7]={1:0x008a,2:0x010a,3:0x13ca,4:0x030a}[kind].to_bytes(2,'big')
    p[7:9]=(16 if kind in (1,3) else 0).to_bytes(2,'big')
    p[9:17]=replay.to_bytes(8,'big')
    p[17:49]=nonce if nonce is not None else SNONCE if kind==2 else ANONCE if kind!=4 else bytes(32)
    if rsc is not None:p[65:73]=rsc
    p[97:99]=len(payload).to_bytes(2,'big');p[99:]=payload
    return peer_sign(p,ptk) if kind!=1 else bytes(p)


def peer_wrap(kek, plain):
    openssl=shutil.which('openssl',path='/usr/bin:/bin') or shutil.which('openssl')
    if not openssl:raise RuntimeError('OpenSSL CLI with RFC3394 wrap support required')
    return subprocess.run([openssl,'enc','-id-aes128-wrap','-K',kek.hex(),'-iv','a6'*8],
                          input=plain,stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=True).stdout


def peer_plain(gtk=GTK, rsn=RSN, key_id=1):
    p=rsn+b'\xdd\x16\x00\x0f\xac\x01'+bytes([key_id,0])+gtk
    if len(p)%8:p+=b'\xdd'+bytes(7-len(p)%8)
    return p


class PSKRealCrypto(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp=tempfile.TemporaryDirectory(prefix='a04-real-psk-')
        cls.path=Path(cls.tmp.name);cls.src=cls.path/'owner_crypto.c';cls.src.write_text(DRIVER)
        cls.flags=['-I'+str(INCLUDE),'-I'+str(BEAR/'inc'),'-I'+str(BEAR/'src')]
        cls.inputs=list(map(str,SOURCES))
        subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-shared','-fPIC',
                        *cls.flags,str(cls.src),*cls.inputs,'-o',str(cls.path/'real.so')],check=True)
        cls.api=C.CDLL(str(cls.path/'real.so'));p=C.c_void_p;u=C.c_uint;q=C.c_uint64
        signatures={'begin':[p,u,p,u,p,p,p,u,q,u], 'rx':[p,u,q],
                    'tx':[p,u,C.POINTER(u),C.POINTER(q),q], 'tx_ack':[q,u,q],
                    'key':[u,p,p,C.POINTER(u),C.POINTER(q),q], 'key_ack':[u,q,u,q],
                    'probe':[u], 'completion':[], 'secret':[u,p], 'packet_snapshot':[p,u],
                    'admission':[u,C.POINTER(u),C.POINTER(u),C.POINTER(u)]}
        for name,args in signatures.items():
            f=getattr(cls.api,'fixture_'+name);f.argtypes=args;f.restype=C.c_int
        cls.pmk=hashlib.pbkdf2_hmac('sha1',PASSPHRASE,SSID,4096,32)
        cls.ptk=peer_ptk(cls.pmk)
        cls.m1=peer_packet(1,7)
        cls.m2=peer_packet(2,7,cls.ptk,RSN)
        cls.cipher=peer_wrap(cls.ptk[16:32],peer_plain())
        cls.m3=peer_packet(3,8,cls.ptk,cls.cipher,rsc=RSC)
        cls.m4=peer_packet(4,8,cls.ptk)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def begin(self, raw=False, generation=1, nonce=SNONCE, random_mode=0):
        secret=self.pmk if raw else PASSPHRASE
        self.assertEqual(self.api.fixture_begin(secret,len(secret),SSID,len(SSID),AP,STA,nonce,
                                               raw,generation,random_mode),0)
        self.now=101
        self.assertEqual(self.probe(0),WAIT_M1)
        self.assertEqual((self.probe(15),self.probe(16)),(0,0)) # consumed and wiped
        self.assertEqual(self.secret(0),self.pmk)
        self.assertFalse(self.probe(1))

    def tick(self):
        self.now+=1
        return self.now

    def probe(self,n):return self.api.fixture_probe(n)

    def secret(self,which):
        out=C.create_string_buffer(64);n=self.api.fixture_secret(which,out)
        self.assertGreater(n,0)
        return out.raw[:n]

    def rx(self,packet):return self.api.fixture_rx(packet,len(packet),self.tick())

    def tx(self):
        out=C.create_string_buffer(611);n=C.c_uint();cookie=C.c_uint64()
        result=self.api.fixture_tx(out,len(out),C.byref(n),C.byref(cookie),self.tick())
        return result,out.raw[:n.value],cookie.value

    def packet_snapshot(self):
        out=C.create_string_buffer(611)
        n=self.api.fixture_packet_snapshot(out,len(out))
        return out.raw[:n]

    def key(self,pair):
        key=C.create_string_buffer(16);rsc=C.create_string_buffer(8)
        idx=C.c_uint();cookie=C.c_uint64()
        e=self.api.fixture_key(pair,key,rsc,C.byref(idx),C.byref(cookie),self.tick())
        return e,key.raw,rsc.raw,idx.value,cookie.value

    def tx_ack(self,cookie,status=0):return self.api.fixture_tx_ack(cookie,status,self.tick())

    def key_ack(self,pair,cookie,status=0):return self.api.fixture_key_ack(pair,cookie,status,self.tick())

    def peer_check(self,packet,expected):
        self.assertEqual(packet,expected)
        zero=bytearray(packet);zero[81:97]=bytes(16)
        self.assertTrue(hmac.compare_digest(packet[81:97],hmac.digest(self.ptk[:16],zero,'sha1')[:16]))

    def to_m3(self,raw=False):
        self.begin(raw)
        self.assertEqual(self.rx(self.m1),0)
        self.assertEqual(self.secret(1),self.ptk)
        self.assertEqual(self.secret(0),bytes(32)) # PMK retired after real derivation
        e,m2,cookie=self.tx();self.assertEqual(e,0);self.peer_check(m2,self.m2)
        self.assertFalse(self.probe(1));self.assertEqual(self.tx_ack(cookie),0)
        self.assertEqual(self.probe(0),WAIT_M3)

    def keys_complete(self):
        self.assertEqual(self.probe(0),KEYS)
        self.assertNotEqual(self.key(0)[0],0) # no GTK before PTK completion
        e,key,rsc,idx,cookie=self.key(1)
        self.assertEqual((e,key,rsc,idx),(0,self.ptk[32:48],bytes(8),0))
        self.assertFalse(self.probe(1));self.assertNotEqual(self.key(1)[0],0)
        self.assertEqual(self.key_ack(1,cookie),0) # HOST SIMULATED firmware ACK
        self.assertFalse(self.probe(1))
        e,key,rsc,idx,cookie=self.key(0)
        self.assertEqual((e,key,rsc,idx),(0,GTK,RSC,1))
        self.assertEqual(self.key_ack(0,cookie),0) # HOST SIMULATED firmware ACK
        self.assertEqual(self.probe(0),M4_READY);self.assertFalse(self.probe(1))

    def finish(self):
        self.keys_complete();e,m4,cookie=self.tx();self.assertEqual(e,0)
        self.peer_check(m4,self.m4);self.assertFalse(self.probe(1))
        self.assertNotEqual(self.api.fixture_completion(),0)
        self.assertEqual(self.tx_ack(cookie),0) # HOST SIMULATED TX_DONE
        self.assertEqual(self.probe(0),COMPLETE);self.assertTrue(self.probe(1))
        self.assertEqual(self.api.fixture_completion(),0)
        self.assertNotEqual(self.api.fixture_completion(),0)

    def assert_poison(self):
        self.assertEqual(self.probe(0),FAILED);self.assertFalse(self.probe(1))
        for slot,n in ((0,32),(1,64),(2,16),(3,32)):
            self.assertEqual(self.secret(slot),bytes(n))
        self.assertNotEqual(self.tx()[0],0);self.assertNotEqual(self.key(1)[0],0)
        self.assertNotEqual(self.api.fixture_completion(),0)

    def test_preassociation_prepared_ssid_binding_consume_and_legacy_rejection(self):
        self.begin()
        for mode in range(5):
            wiped=C.c_uint();reused=C.c_uint();active=C.c_uint()
            before=self.probe(10)
            result=self.api.fixture_admission(mode,C.byref(wiped),C.byref(reused),C.byref(active))
            self.assertEqual(result==0,mode==0)
            self.assertEqual(wiped.value,1)
            self.assertNotEqual(reused.value,0)
            self.assertEqual(active.value,WAIT_M1 if mode==0 else 0)
            self.assertEqual(self.probe(10),before) # no PBKDF2 in begin/admission

    def test_full_peer_exchange_passphrase_and_raw_pmk(self):
        for raw in (False,True):
            with self.subTest(raw_pmk=raw):
                self.to_m3(raw)
                self.assertEqual(self.probe(10),0 if raw else 1)
                self.assertEqual((self.probe(11),self.probe(12)),(1,1))
                self.assertEqual(self.rx(self.m3),0)
                self.assertEqual(self.probe(14),1)
                self.assertEqual(self.secret(2),GTK)
                self.finish()
                self.assertEqual((self.probe(6),self.probe(7)),(1,1))

    def test_duplicate_m1_no_nonce_regeneration_or_borrowed_packet_change(self):
        self.begin();self.assertEqual(self.rx(self.m1),0)
        e,m2,cookie=self.tx();self.assertEqual(e,0)
        self.assertEqual(self.rx(self.m1),0)
        higher=peer_packet(1,8)
        self.assertNotEqual(self.rx(higher),0) # pending TX borrowed buffer immutable
        self.assertEqual(self.packet_snapshot(),self.m2)
        self.assertEqual((self.probe(11),self.probe(12)),(1,1))
        self.assertEqual(self.tx_ack(cookie),0)
        self.assertEqual(self.rx(higher),0)
        e,m2,cookie=self.tx();self.assertEqual(e,0)
        self.peer_check(m2,peer_packet(2,8,self.ptk,RSN))
        self.assertEqual(self.tx_ack(cookie),0)
        m3=peer_packet(3,9,self.ptk,self.cipher,rsc=RSC)
        self.assertEqual(self.rx(m3),0)
        self.assertEqual((self.probe(11),self.probe(12)),(1,1))

    def test_duplicate_m3_before_during_and_after_key_completion_no_reinstall(self):
        self.to_m3();self.assertEqual(self.rx(self.m3),0)
        unwraps=self.probe(14)
        self.assertEqual(self.rx(self.m3),0)
        e,key,rsc,idx,ptk_cookie=self.key(1);self.assertEqual(e,0)
        self.assertEqual(self.rx(self.m3),0)
        self.assertEqual(self.key_ack(1,ptk_cookie),0)
        e,key,rsc,idx,gtk_cookie=self.key(0);self.assertEqual(e,0)
        self.assertEqual(self.rx(self.m3),0)
        self.assertEqual(self.key_ack(0,gtk_cookie),0)
        self.assertEqual(self.rx(self.m3),0)
        e,m4,cookie=self.tx();self.assertEqual(e,0);self.peer_check(m4,self.m4)
        self.assertEqual(self.rx(self.m3),0);self.assertEqual(self.packet_snapshot(),self.m4)
        self.assertEqual(self.tx_ack(cookie),0)
        self.assertTrue(self.probe(1))
        self.assertEqual(self.api.fixture_completion(),0)
        self.assertEqual(self.rx(self.m3),0)
        e,m4,cookie=self.tx();self.assertEqual(e,0);self.peer_check(m4,self.m4)
        self.assertEqual(self.tx_ack(cookie),0)
        self.assertEqual(self.probe(14),unwraps)
        self.assertEqual((self.probe(6),self.probe(7)),(1,1))
        self.assertNotEqual(self.key(1)[0],0);self.assertNotEqual(self.key(0)[0],0)
        self.assertNotEqual(self.api.fixture_completion(),0)

    def test_authenticated_higher_replay_m3_same_keys_no_reinstall(self):
        self.to_m3();self.assertEqual(self.rx(self.m3),0);self.finish()
        m3=peer_packet(3,9,self.ptk,self.cipher,rsc=RSC)
        self.assertEqual(self.rx(m3),0)
        e,m4,cookie=self.tx();self.assertEqual(e,0)
        self.peer_check(m4,peer_packet(4,9,self.ptk))
        self.assertEqual(self.tx_ack(cookie),0)
        self.assertEqual((self.probe(6),self.probe(7)),(1,1))
        self.assertNotEqual(self.api.fixture_completion(),0)
        self.assertTrue(self.probe(1))

    def test_mic_and_wrap_integrity_corruptions_never_expose_keys(self):
        mutations=[]
        for offset in (81,96,99,len(self.m3)-1):
            p=bytearray(self.m3);p[offset]^=1;mutations.append(('bad_mic_'+str(offset),bytes(p),False))
        # Correct outer MIC, corrupt inner RFC3394 integrity: must reach real AES.
        bad=bytearray(self.cipher);bad[-1]^=1
        mutations.append(('bad_wrap',peer_packet(3,8,self.ptk,bytes(bad),rsc=RSC),True))
        wrong_kek=peer_wrap(bytes(x^1 for x in self.ptk[16:32]),peer_plain())
        mutations.append(('wrong_kek',peer_packet(3,8,self.ptk,wrong_kek,rsc=RSC),True))
        for name,p,unwrap_called in mutations:
            with self.subTest(name=name):
                self.to_m3();before=self.probe(14)
                self.assertNotEqual(self.rx(p),0);self.assert_poison()
                self.assertEqual(self.probe(14)-before,int(unwrap_called))
                self.assertEqual((self.probe(6),self.probe(7)),(0,0))

    def test_authenticated_nonce_replay_and_kde_changes_fail_closed(self):
        bad_rsn=bytearray(RSN);bad_rsn[-1]^=1
        invalid_plain=[('rsn',peer_plain(rsn=bytes(bad_rsn))),
                       ('missing_gtk',RSN+b'\xdd\0'),
                       ('unknown_kde',RSN+b'\xdd\x16\0\x0f\xac\x09\x01\0'+GTK+b'\xdd\0'),
                       ('duplicate_gtk',peer_plain()[:-2]+peer_plain()[22:-2]+b'\xdd\0'),
                       ('padding',peer_plain()[:-1]+b'\x01')]
        for name,plain in invalid_plain:
            if len(plain)%8:plain+=b'\xdd'+bytes(7-len(plain)%8)
            with self.subTest(kde=name):
                self.to_m3();cipher=peer_wrap(self.ptk[16:32],plain)
                self.assertNotEqual(self.rx(peer_packet(3,8,self.ptk,cipher,rsc=RSC)),0)
                self.assert_poison()
        for replay in (0,6,7):
            with self.subTest(replay=replay):
                self.to_m3()
                self.assertNotEqual(self.rx(peer_packet(3,replay,self.ptk,self.cipher,rsc=RSC)),0)
                self.assert_poison()
        self.to_m3()
        wrong_nonce=bytes(x^1 for x in ANONCE)
        self.assertNotEqual(self.rx(peer_packet(3,8,self.ptk,self.cipher,nonce=wrong_nonce,rsc=RSC)),0)
        self.assert_poison()

    def test_changed_duplicate_m3_cannot_replace_installed_keys_or_rsc(self):
        for change in ('same_replay_byte','new_gtk','new_rsc'):
            with self.subTest(change=change):
                self.to_m3();self.assertEqual(self.rx(self.m3),0);self.finish()
                if change=='same_replay_byte':
                    p=bytearray(self.m3);p[65]^=1;p=peer_sign(p,self.ptk)
                elif change=='new_gtk':
                    cipher=peer_wrap(self.ptk[16:32],peer_plain(gtk=bytes(reversed(GTK))))
                    p=peer_packet(3,9,self.ptk,cipher,rsc=RSC)
                else:p=peer_packet(3,9,self.ptk,self.cipher,rsc=bytes(8))
                self.assertNotEqual(self.rx(p),0);self.assert_poison()
                self.assertEqual((self.probe(6),self.probe(7)),(1,1))

    def test_correlated_completion_gate_failure_and_old_generation(self):
        self.begin(generation=1);self.assertEqual(self.rx(self.m1),0)
        e,p,old=self.tx();self.assertEqual(e,0)
        self.begin(generation=2);self.assertEqual(self.rx(self.m1),0)
        e,p,new=self.tx();self.assertEqual(e,0);self.assertNotEqual(old,new)
        self.assertNotEqual(self.tx_ack(old),0);self.assertFalse(self.probe(1))
        self.assertEqual(self.tx_ack(new),0)
        self.assertEqual(self.rx(self.m3),0)
        e,key,rsc,idx,cookie=self.key(1);self.assertEqual(e,0)
        self.assertNotEqual(self.key_ack(0,cookie),0)
        self.assertNotEqual(self.key_ack(1,cookie+1),0)
        self.assertEqual(self.key_ack(1,cookie),0)
        e,key,rsc,idx,cookie=self.key(0);self.assertEqual(e,0)
        self.assertNotEqual(self.key_ack(0,cookie,1),0);self.assert_poison()
        self.to_m3();self.assertEqual(self.rx(self.m3),0);self.keys_complete()
        e,p,cookie=self.tx();self.assertEqual(e,0)
        self.assertNotEqual(self.tx_ack(cookie,1),0);self.assert_poison()

    def test_nonce_failure_retries_deadline_and_truncation(self):
        for mode,nonce in ((1,SNONCE),(2,SNONCE),(0,ANONCE)):
            self.begin(random_mode=mode,nonce=nonce)
            self.assertNotEqual(self.rx(self.m1),0);self.assert_poison()
        self.begin();self.assertEqual(self.rx(self.m1),0)
        for attempt in range(4):
            e,p,cookie=self.tx();self.assertEqual(e,0);self.peer_check(p,self.m2)
            self.assertEqual(self.tx_ack(cookie),0)
            self.assertEqual(self.rx(self.m1),0)
        self.assertNotEqual(self.tx()[0],0);self.assert_poison()
        self.to_m3()
        self.assertNotEqual(self.api.fixture_rx(self.m3,len(self.m3),1000100),0)
        self.assert_poison()
        for n in (0,1,4,81,98,99,len(self.m3)-1):
            self.to_m3();self.assertNotEqual(self.rx(self.m3[:n]),0);self.assert_poison()

    def test_maximum_keydata_and_declared_length_descriptor_bounds(self):
        self.to_m3()
        # 504 plaintext ->512 wrapped bytes ->611-byte maximum EAPOL owner frame.
        padded=peer_plain()+bytes(504-len(peer_plain()))
        maximum=peer_packet(3,8,self.ptk,peer_wrap(self.ptk[16:32],padded),rsc=RSC)
        self.assertEqual(len(maximum),611)
        self.assertEqual(self.rx(maximum),0)
        self.assertEqual(self.secret(2),GTK)
        self.keys_complete()
        e,m4,cookie=self.tx();self.assertEqual(e,0);self.peer_check(m4,self.m4)
        self.assertEqual(self.tx_ack(cookie),0)
        self.to_m3()
        too_big=peer_packet(3,8,self.ptk,peer_wrap(self.ptk[16:32],padded+bytes(8)),rsc=RSC)
        self.assertNotEqual(self.rx(too_big),0);self.assert_poison()
        for offset in (0,1,2,4,6,7,49,71,97,98):
            with self.subTest(offset=offset):
                self.to_m3();packet=bytearray(self.m3);packet[offset]^=0xff
                # Valid outer MIC ensures semantic/length rejection is exercised.
                self.assertNotEqual(self.rx(peer_sign(packet,self.ptk)),0)
                self.assert_poison()

    def test_duplicate_m4_response_budget_no_key_reinstall(self):
        self.to_m3();self.assertEqual(self.rx(self.m3),0);self.finish()
        for _ in range(3):
            self.assertEqual(self.rx(self.m3),0)
            e,m4,cookie=self.tx();self.assertEqual(e,0);self.peer_check(m4,self.m4)
            self.assertEqual(self.tx_ack(cookie),0)
        self.assertEqual((self.probe(6),self.probe(7)),(1,1))
        self.assertEqual(self.rx(self.m3),0)
        self.assertNotEqual(self.tx()[0],0);self.assert_poison()

    def test_real_exchange_under_asan_ubsan(self):
        # Peer bytes below are generated by independent Python/OpenSSL above.
        def arr(name,data):return 'static const uint8_t '+name+'[]={'+','.join(map(str,data))+'};\n'
        vectors=''.join(arr(name,data) for name,data in [('pass',PASSPHRASE),('ssid',SSID),('ap',AP),
                    ('sta',STA),('sn',SNONCE),('one',self.m1),('two',self.m2),('three',self.m3),
                    ('four',self.m4),('ptk',self.ptk),('gtk',GTK),('counter',RSC)])
        main=r'''
int main(void){
 for(unsigned corrupt=0;corrupt<4;corrupt++){
  assert(!fixture_begin(pass,sizeof(pass),ssid,sizeof(ssid),ap,sta,sn,0,1,0));
  assert(!fixture_rx(one,sizeof(one),110));uint8_t packet[611],key[16],rsc[8];unsigned n,id;uint64_t c;
  assert(!fixture_tx(packet,sizeof(packet),&n,&c,120));assert(n==sizeof(two)&&!memcmp(packet,two,n));
  assert(!fixture_tx_ack(c,0,130));uint8_t input[sizeof(three)];memcpy(input,three,sizeof(input));
  if(corrupt){input[corrupt==1?81:corrupt==2?99:sizeof(input)-1]^=1;
   assert(fixture_rx(input,sizeof(input),140));assert(!fixture_probe(1));continue;}
  assert(!fixture_rx(input,sizeof(input),140));assert(!fixture_rx(input,sizeof(input),141));
  assert(!fixture_key(1,key,rsc,&id,&c,150));assert(!memcmp(key,ptk+32,16));assert(!fixture_key_ack(1,c,0,160));
  assert(!fixture_key(0,key,rsc,&id,&c,170));assert(id==1&&!memcmp(key,gtk,16)&&!memcmp(rsc,counter,8));
  assert(!fixture_key_ack(0,c,0,180));assert(!fixture_probe(1));
  assert(!fixture_tx(packet,sizeof(packet),&n,&c,190));assert(n==sizeof(four)&&!memcmp(packet,four,n));
  assert(!fixture_tx_ack(c,0,200));assert(fixture_probe(1));assert(!fixture_completion());
  assert(!fixture_rx(input,sizeof(input),210));assert(fixture_key(1,key,rsc,&id,&c,220));
  assert(fixture_probe(6)==1&&fixture_probe(7)==1);
 }
 return 0;
}
'''
        src=self.path/'asan.c';src.write_text(DRIVER+vectors+main);exe=self.path/'asan'
        subprocess.run(['clang','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
                        '-fsanitize=address,undefined','-fno-omit-frame-pointer',*self.flags,
                        str(src),*self.inputs,'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)

if __name__=='__main__':unittest.main()
