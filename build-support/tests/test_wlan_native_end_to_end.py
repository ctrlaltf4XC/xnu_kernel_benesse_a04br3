# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Standalone host integration of native SET20 through actual WPA2 and IP.

AP, CMD52/53 card, IOKit/DLIL and time are explicit HOST SIMULATIONS.
Production native codecs/dispatch, extracted SDIO admission/crypto/queue/
completion blocks, runtime auth/assoc, real BearSSL crypto, HIF encoders,
firmware event decoders and standard link publisher execute unmodified.
Independent AP oracle is test_wlan_psk4way_crypto (Python/OpenSSL), not a
crypto-success mock. No device, SDK, private research files or pip packages.
This does not boot XNU, run native iOS processes, or demonstrate DHCP success.
"""
import hashlib
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'iokit/Kernel/arm/network/tests'))
import test_ethernet_binding as binding
import test_wlan_backend as backend
import test_wlan_psk4way_crypto as peer
import test_wlan_personal_runtime_regression as extraction


def array(name,value):
 return 'static const uint8_t '+name+'[]={'+','.join(map(str,value))+'};\n'


def artifacts(fault=None):
 pmk=hashlib.pbkdf2_hmac('sha1',peer.PASSPHRASE,peer.SSID,4096,32)
 ptk=peer.peer_ptk(pmk)
 values=[('ap',peer.AP),('sta',peer.STA),('ssid',peer.SSID),('pass',peer.PASSPHRASE),
         ('sn',peer.SNONCE),('rsn',peer.RSN),('pmk',pmk),('ptk',ptk),('gtk',peer.GTK),
         ('m1',peer.peer_packet(1,7)),('m2',peer.peer_packet(2,7,ptk,peer.RSN)),
         ('m3',peer.peer_packet(3,8,ptk,peer.peer_wrap(ptk[16:32],peer.peer_plain()),rsc=peer.RSC)),
         ('m4',peer.peer_packet(4,8,ptk))]
 if fault:
  bad=bytearray(values[-2][1])
  if fault=='mic':bad[81]^=1
  else:
   bad[-1]^=1
   bad=bytearray(peer.peer_sign(bad,ptk)) # valid MIC, invalid RFC3394 integrity
  values.append(('bad_m3',bytes(bad)))
 return ''.join(array(n,v) for n,v in values)

API=r'''
#include "A04BR3WLANBackendAPI.h"
#include "A04BR3WLANPersonalRuntime.h"
#include "A04BR3WLANJoinAdmission.h"
#include "A04BR3WLANCurrentNetwork.h"
#include "A04BR3WLANNetworkProfile.h"
#include "A04BR3WLANPSKBinding.h"
static a04_wlan_native_client nativeClient={};
VECTORS
FAKE_CARD
static uint8_t inputWire[1024];static unsigned inputBytes;
static int card_transfer(void *ctx,uint32_t arg,void *out,unsigned n){
 unsigned addr=(arg>>9)&0x1ffff;
 if(!(arg>>31)&&addr==0x10){assert(n==4);a04_wmt_put32((uint8_t *)out,0);return 0;}
 if(!(arg>>31)&&(addr==0x50||addr==0x54)){
  assert(inputBytes&&n>=inputBytes);memset(out,0,n);memcpy(out,inputWire,inputBytes);
  ((card *)ctx)->rx_size=0;return 0;
 }
 return transfer(ctx,arg,out,n);
}
static unsigned rngCalls,rxInputs;static uint8_t expectedIP[40];
extern "C" void read_random(void *out,unsigned n){
 assert(!heldLocks&&n==32);rngCalls++;memcpy(out,sn,32); // deterministic HOST ONLY nonce
}
class A04BR3SDIO {
public:
 struct Live {a04_wlan_runtime runtime;struct a04_psk4way personalWork;
  uint8_t personalRX[1514];struct {uint8_t data[1514];unsigned bytes;} queue[8];unsigned count=0,head=0;
 } storage={};Live *live=&storage;
 struct card card={};a04_sdio_io io={};a04_sdio_transport transport={};a04_wlan_host host={};
 a04_wlan_host *wlanHost=&host;IOLock *transportLock=nullptr;
 bool liveStopping=false,networkInitialized=true,identityReady=true;
 struct {unsigned ready=1;} wlanDownload;
 a04_wlan_native_client nativeClient={};uint64_t nativeEpoch=1,now=1000100;unsigned schedules=0;
 uint64_t liveNow(){return now;}
 void scheduleLive(){schedules++;}
EXTRACTED_METHODS
 void init(){
  transportLock=IOLockAlloc();setup(&card,&io,&transport);transport.transfer=card_transfer;
  assert(!a04_wlan_host_open(&host,&transport));host.ready=1;assert(!a04_wlan_runtime_begin(&host));
  auto &r=live->runtime;r.begun=r.configured=r.admin=r.mac_valid=1;memcpy(r.mac,sta,6);
  a04_wlan_local local={};local_profile(&local);memcpy(local.mac,sta,6);
  assert(!a04_wlan_station_setup(&r.station,&host,&local));
  // Explicit simulated AP scan observation through the actual beacon parser.
  uint8_t beacon[256]={0};beacon[0]=0x80;memcpy(beacon+10,ap,6);memcpy(beacon+16,ap,6);
  a04_wmt_put16(beacon+32,100);a04_wmt_put16(beacon+34,0x411);unsigned n=36;
  beacon[n++]=0;beacon[n++]=sizeof(ssid);memcpy(beacon+n,ssid,sizeof(ssid));n+=sizeof(ssid);
  const uint8_t rates[]={1,4,0x82,0x84,0x8b,0x96,3,1,6};memcpy(beacon+n,rates,sizeof(rates));n+=sizeof(rates);
  memcpy(beacon+n,rsn,sizeof(rsn));n+=sizeof(rsn);
  assert(!a04_wlan_ap_parse(beacon,n,6,&r.scan[0]));r.scan_count=1;r.scan_generation=3;
  r.scan_extra[0].generation=3;r.scan_extra[0].observed_us=now;
  r.station.scan.complete=1;r.scan_completed_us=now;
 }
 void finish(){assert(!heldLocks);IOLockFree(transportLock);transportLock=nullptr;}
 void event(unsigned id,const uint8_t *p,unsigned n,unsigned sequence=0){
  struct a04_wlan_event e={id,sequence,n,p};now+=10;
  IOLockLock(transportLock);assert(!a04_wlan_runtime_event(&live->runtime,&host,&e,now));IOLockUnlock(transportLock);
 }
 int cryptoPump(){
  auto *r=&live->runtime;int e=0;auto client=nativeClient;uint64_t now=liveNow();
  IOLockLock(transportLock);
  for(unsigned once=0;once<1;once++){
CRYPTO_BLOCK
  }
  IOLockUnlock(transportLock);return e;
 }
 void drive(){now+=10;IOLockLock(transportLock);assert(!a04_wlan_personal_drive(&live->runtime,&host,now));IOLockUnlock(transportLock);}
 void txDone(){auto &r=live->runtime;assert(r.data.pending);uint8_t e[16]={0};e[0]=(uint8_t)r.data.pid;e[4]=(uint8_t)r.station.local.wlan;event(0x0f,e,16);}
 void input(const uint8_t *payload,unsigned bytes,unsigned etherType,unsigned cipher=0){
  auto &r=live->runtime;memset(inputWire,0,sizeof(inputWire));unsigned offset=cipher?32:16;
  inputBytes=offset+14+bytes;assert(inputBytes<sizeof(inputWire));a04_wmt_put16(inputWire,inputBytes);
  a04_wmt_put16(inputWire+2,cipher?0x4200:0x4000);inputWire[6]=0x80|14;
  inputWire[8]=(uint8_t)r.station.local.wlan;inputWire[9]=(uint8_t)(cipher<<4);if(cipher)inputWire[16]=1;
  memcpy(inputWire+offset,sta,6);memcpy(inputWire+offset+6,ap,6);inputWire[offset+12]=(uint8_t)(etherType>>8);inputWire[offset+13]=(uint8_t)etherType;
  memcpy(inputWire+offset+14,payload,bytes);card.rx_size=inputBytes;now+=10;
  IOLockLock(transportLock);int e=a04_wlan_runtime_step(&r,&host,now,a04_psk_verify,&r.personal);IOLockUnlock(transportLock);assert(!e);
 }
 void sendQueued(){auto *r=&live->runtime;auto client=nativeClient;uint64_t now=liveNow();int e=0;
  IOLockLock(transportLock);
QUEUE_BLOCK
  IOLockUnlock(transportLock);assert(!e);
 }
 void publish(){auto client=nativeClient;bool changed=true;uint64_t authenticatedGeneration=live->runtime.association_generation,completionConsumer=nativeEpoch;
DISPATCH_BLOCK
 }
} radio;
static int radioState(void *p,a04_network_state *out){return A04BR3SDIO::backendState(p,out);}
static int providerGet(void *,uint32_t t,uint32_t v,void *p,uint32_t *n){return nativeClient.get(nativeClient.context,t,v,p,n);}
static int providerSet(void *,uint32_t t,uint32_t v,const void *p,uint32_t n){return nativeClient.set(nativeClient.context,t,v,p,n);}
static int radioAdmin(void *,unsigned enabled){return enabled==radio.live->runtime.admin?0:EOPNOTSUPP;}
extern "C" int a04br3_wlan_acquire_backend(IOService *p,a04_network_backend *b){p->retain();*b={&radio,radioState,providerGet,radioAdmin,A04BR3SDIO::backendTransmit,providerSet};return 0;}
extern "C" void a04br3_wlan_release_backend(IOService *p){p->release();}
extern "C" int a04br3_wlan_scan_snapshot(IOService *,a04_wlan_scan_info *p){return radio.scanSnapshot(p);}
extern "C" int a04br3_wlan_scan_copy_record(IOService *,unsigned g,unsigned i,a04_wlan_scan_record *p){return radio.scanCopyRecord(g,i,p);}
extern "C" int a04br3_wlan_current_record(IOService *,a04_wlan_scan_record *p){return radio.currentRecord(p);}
extern "C" int a04br3_wlan_scan_mode(IOService *,unsigned,unsigned *){return EOPNOTSUPP;}
extern "C" int a04br3_wlan_disassociate(IOService *){return EOPNOTSUPP;}
extern "C" int a04br3_wlan_join_target(IOService *,const a04_wlan_join_target *){return EOPNOTSUPP;}
extern "C" int a04br3_wlan_join_personal(IOService *,const a04_wlan_join_target *p,a04_psk_prepared *s){return radio.requestPersonalJoin(p,s);}
extern "C" int a04br3_wlan_bind_native(IOService *,const a04_wlan_native_client *c){nativeClient=*c;radio.nativeClient=*c;c->owner->retain();return 0;}
extern "C" int a04br3_wlan_unbind_native(IOService *){nativeClient.owner->release();nativeClient={};radio.nativeClient={};return 0;}
'''

CASE=r'''
static void nativeCases(){
 (void)mockOtherGroup;(void)expectedIP;(void)rxInputs;reset();radio.init();auto &r=radio.live->runtime;IOService provider;IOService *owner=nullptr;
 assert(!a04br3_network_bind(&provider,&owner));nativeClient.ready(nativeClient.context,&provider);
 assert(current&&current->refs==2);current->flags|=IFF_UP;assert(!current->init.ioctl(current,SIOCSIFFLAGS,nullptr));
 assert(!linkUps&&!authMessages&&!(current->flags&IFF_RUNNING));
 // Native iOS16 raw SET20 producer, through real BSD ioctl + Rendezvous + admission.
 memset(userBuffer,0,592);userBuffer[0]=1;userBuffer[4]=2;userBuffer[6]=1;userBuffer[8]=8;
 userBuffer[12]=sizeof(ssid);memcpy(userBuffer+16,ssid,sizeof(ssid));
 userBuffer[56]=1;userBuffer[60]=sizeof(pass);userBuffer[64]=10;memcpy(userBuffer+72,pass,sizeof(pass));
 userBuffer[204]=sizeof(rsn);memcpy(userBuffer+206,rsn,sizeof(rsn));
 auto request=req();request.type=20;request.value=0;request.length=592;
 assert(!current->init.ioctl(current,A04_APPLE80211_SET64,&request));
 assert(r.personal_admitted&&r.auth_upper==8&&r.association_generation==1&&r.credential.ready);
 assert(!memcmp(r.credential.pmk,pmk,32)&&!r.station.sta_active&&!r.security.authorized);
 // Real channel grant -> auth encoder/response -> assoc encoder/response -> STA event.
 uint8_t e[16]={0};e[0]=(uint8_t)r.station.local.bss;e[1]=(uint8_t)r.station.channel.token;e[3]=6;e[5]=1;a04_wmt_put32(e+12,1000);
 radio.event(0x10,e,16);assert(!r.station.channel_pending&&r.station.grant_ms==1000);
 assert(!a04_wlan_station_auth(&r.station,&radio.host,radio.now));
 uint8_t responseBytes[30];response(responseBytes,&r.station,0xb0);a04_wmt_put16(responseBytes+26,2);
 assert(!a04_wlan_station_auth_rx(&r.station,&radio.host,responseBytes,30,radio.now));
 assert(!a04_wlan_station_assoc(&r.station,&radio.host,r.station.ap.capability,10,rsn,sizeof(rsn),radio.now));
 response(responseBytes,&r.station,0x10);a04_wmt_put16(responseBytes+28,0xc02a);
 assert(!a04_wlan_station_assoc_rx(&r.station,&radio.host,responseBytes,30,1,radio.now));
 memset(e,0,16);memcpy(e,ap,6);e[6]=(uint8_t)r.station.local.sta;e[7]=(uint8_t)r.station.local.bss;
 radio.event(0x0c,e,8,r.station.sta_sequence);assert(r.station.sta_active&&!r.station.deadline_us);
 auto crypto=a04_psk_native_crypto();assert(!a04_wlan_personal_start(&r,&crypto,radio.now));
 nativeClient.changed(nativeClient.context);assert(!linkUps&&!authMessages&&!(current->flags&IFF_RUNNING));
 // Real native GET1/9 metadata before keys; no assumed dummy BSSID.
 request.type=1;request.length=32;assert(!current->init.ioctl(current,A04_APPLE80211_GET64,&request)&&request.length==sizeof(ssid)&&!memcmp(userBuffer,ssid,sizeof(ssid)));
 request.type=9;request.length=6;assert(!current->init.ioctl(current,A04_APPLE80211_GET64,&request)&&!memcmp(userBuffer,ap,6));
 // Ordinary IP cannot bypass the preauthentication port gate.
 Packet blocked={false};blocked.length=blocked.used=54;memcpy(blocked.storage,ap,6);memcpy(blocked.storage+6,sta,6);blocked.storage[12]=8;
 assert(current->init.output(current,&blocked)==ENETDOWN&&blocked.freed&&!radio.live->count&&!linkUps);
 radio.input(m1,sizeof(m1),0x888e);assert(r.delivery_kind==A04_WLAN_DELIVERY_EAPOL);assert(!radio.cryptoPump());
 radio.drive();assert(r.data.pending&&!memcmp(radio.host.tx+42,m2,sizeof(m2)));radio.txDone();
 radio.input(m3,sizeof(m3),0x888e);assert(!radio.cryptoPump());assert(!memcmp(r.personal.ptk,ptk,64)&&!memcmp(r.personal.gtk,gtk,16)&&rngCalls==1);
 radio.drive();assert(r.security.ptk_pending&&r.personal.ptk_state==1);unsigned txs=radio.card.txs;
 radio.drive();assert(radio.card.txs==txs&&!r.security.gtk_accepted); // no fake PTK completion
 memset(e,0,16);e[0]=(uint8_t)r.station.local.bss;memcpy(e+2,ap,6);radio.event(0x24,e,8);
 radio.drive();assert(r.security.gtk_accepted&&r.personal.gtk_state==2&&r.personal.phase==A04_PSK_M4_READY);
 assert(!r.security.authorized&&!linkUps&&!authMessages);radio.drive();
 assert(r.data.pending&&r.data.eapol_kind==4&&!memcmp(radio.host.tx+42,m4,sizeof(m4)));
 assert(!r.security.m4_confirmed);radio.txDone();assert(r.security.m4_confirmed&&a04_psk_authenticated(&r.personal));
 assert(!a04_wlan_security_authorize(&r.security,0,radio.now));
 radio.publish();assert(linkUps==1&&authMessages==1&&(current->flags&IFF_RUNNING)&&r.personal.completion_reported);
 radio.publish();assert(linkUps==1&&authMessages==1); // real deduplication
 // Ordinary IPv4 wire packet, no actual DHCP service simulated or claimed.
 uint8_t ip[40]={0x45,0,0,40};ip[8]=64;ip[9]=17;ip[12]=192;ip[15]=2;ip[16]=192;ip[19]=1;
 ip[20]=0x30;ip[21]=0x39;ip[22]=0x27;ip[23]=0x0f;ip[25]=20;memcpy(ip+28,"host-ip-test",12);
 unsigned sum=0;for(unsigned i=0;i<20;i+=2)sum+=((unsigned)ip[i]<<8)|ip[i+1];while(sum>>16)sum=(sum&65535)+(sum>>16);
 sum=(~sum)&65535;ip[10]=(uint8_t)(sum>>8);ip[11]=(uint8_t)sum; // valid IPv4 + UDP checksum-zero packet
 memcpy(expectedIP,ip,sizeof(ip));
 inputObserver=[](ifnet_t p,mbuf_t m){
  assert(p==current&&p->refs==3&&mbuf_pkthdr_len(m)==sizeof(expectedIP));
  uint8_t got[40];assert(!mbuf_copydata(m,0,sizeof(got),got)&&!memcmp(got,expectedIP,sizeof(got)));
  auto *eth=(const uint8_t *)mbuf_pkthdr_header(m);assert(eth&&!memcmp(eth,sta,6)&&!memcmp(eth+6,ap,6)&&eth[12]==8&&!eth[13]);rxInputs++;
 };
 Packet packet={false};packet.length=packet.used=14+sizeof(ip);memcpy(packet.storage,ap,6);memcpy(packet.storage+6,sta,6);
 packet.storage[12]=8;memcpy(packet.storage+14,ip,sizeof(ip));assert(!current->init.output(current,&packet)&&packet.freed&&radio.live->count==1);
 radio.sendQueued();assert(r.data.pending&&!r.data.eapol_kind&&!memcmp(radio.host.tx+42,ip,sizeof(ip))&&(radio.host.tx[6]&0x80));radio.txDone();
 radio.input(ip,sizeof(ip),0x800,4);assert(r.delivery_kind==A04_WLAN_DELIVERY_ETHERNET);
 assert(r.delivery_length==14+sizeof(ip)&&!memcmp(r.delivery+14,ip,sizeof(ip)));int inputs=inputCalls;
 nativeClient.packet(nativeClient.context,r.delivery_kind,r.delivery,r.delivery_length);assert(inputCalls==inputs+1&&rxInputs==1);inputObserver=nullptr;
 assert(!a04br3_network_unbind(&provider,&owner)&&!owner&&!current&&linkDowns==1&&provider.refs==1);
 a04Stack->detach(&provider);a04Stack->release();a04Stack=nullptr;radio.finish();assert(!lockCount&&!childCount);
 printf("HOST ONLY: native SET20 -> real admission/auth/assoc/crypto/HIF/events -> LINK_ON -> completion -> IP TX/RX PASS\n");
}
'''

class NativeEndToEnd(unittest.TestCase):
 def test_native_personal_to_link_and_ip(self):
  self.run_integration()
 def test_real_crypto_mic_and_unwrap_failure_keep_link_and_ip_closed(self):
  for fault in ('mic','unwrap'):
   with self.subTest(fault=fault):self.run_integration(fault)
 def run_integration(self,fault=None):
  source=(peer.INCLUDE/'A04BR3SDIO.cpp').read_text()
  methods='\n'.join(extraction.cpp_block(source,m) for m in [
   ' static int backendState(', ' static int backendTransmit(', ' int scanSnapshot(',
   ' int scanCopyRecord(', ' int currentRecord(', ' int requestPersonalJoin('])
  crypto=extraction.cpp_block(source,'   if(!e && r->personal_admitted && r->delivery_kind==A04_WLAN_DELIVERY_EAPOL)')
  queue=extraction.cpp_block(source,'   if(!e && !r->teardown.pending && live->count && !r->data.pending)')
  a=source.index('  if(changed && client.changed)client.changed(client.context);')
  b=source.index('  if(client.owner)client.owner->release();',a)
  api=API.replace('VECTORS',artifacts(fault)).replace('FAKE_CARD',backend.SETUP)
  api=api.replace('EXTRACTED_METHODS',methods).replace('CRYPTO_BLOCK',crypto).replace('QUEUE_BLOCK',queue).replace('DISPATCH_BLOCK',source[a:b])
  saved=(binding.NATIVE_API,binding.NATIVE_CASE,binding.EXTRA,binding.CASE,binding.STUB)
  try:
   # Existing OPEN fixture fixed a leading-zero MAC and one allocation ref.
   # This isolated harness uses the oracle's actual unicast MAC and Apple's
   # separately retained adopted-ifnet reference; production source unchanged.
   binding.STUB=saved[4].replace('!((const uint8_t *)p->uniqueid)[0]', '!(((const uint8_t *)p->uniqueid)[0]&1)').replace('assert(p->refs==2 && !heldLocks);inputCalls++', 'assert(p->refs==3 && !heldLocks);inputCalls++')
   selected=CASE
   if fault:
    selected=CASE[:CASE.index(' radio.input(m3,sizeof(m3)')]+r'''
 unsigned txs=radio.card.txs;
 radio.input(bad_m3,sizeof(bad_m3),0x888e);int failure=radio.cryptoPump();assert(failure);
 // Actual surrounding production pump poisons the runtime on crypto failure.
 assert(a04_wlan_runtime_fail(&r,&radio.host,failure)==failure);
 assert(!r.security.ptk_pending&&!r.security.gtk_accepted&&!r.security.authorized&&!linkUps&&!authMessages);
 assert(radio.card.txs==txs&&!(current->flags&IFF_RUNNING)&&!a04_psk_authenticated(&r.personal));
 assert(!a04br3_network_unbind(&provider,&owner)&&!owner&&!current&&!linkDowns&&provider.refs==1);
 a04Stack->detach(&provider);a04Stack->release();a04Stack=nullptr;radio.finish();assert(!lockCount&&!childCount);
 printf("HOST ONLY: actual crypto rejects corrupted M3, no keys/link/completion/IP PASS\n");
}
'''
   binding.NATIVE_API=api;binding.NATIVE_CASE=selected;binding.EXTRA='static void bindingCases(){}\n'
   binding.CASE=saved[3].split('int main(){')[0]+'int main(){return 0;}\n'
   binding.EthernetBinding('test_adapter').test_adapter()
  finally:binding.NATIVE_API,binding.NATIVE_CASE,binding.EXTRA,binding.CASE,binding.STUB=saved

if __name__=='__main__':unittest.main()
