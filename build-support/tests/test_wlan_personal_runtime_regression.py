# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Runtime regressions using real PSK owner/crypto and extracted production CPP.

Hardware association/TX/key ACKs, clock and lock scheduling are HOST SIMULATED.
No crypto-success mocks: independent Python/OpenSSL peer and actual providers
come from the frozen test_wlan_psk4way_crypto module, which is NOT modified.
No XNU SDK, device, SSH, timing sleeps, sibling checkout or research log needed.
"""
import hashlib
import ctypes.util
import sys
from pathlib import Path
import subprocess
import tempfile
import unittest
import test_wlan_psk4way_crypto as peer

CPP=peer.INCLUDE/'A04BR3SDIO.cpp'


def cpp_block(source, marker):
    """Copy balanced block verbatim; no rewrites of the tested statements."""
    start=source.index(marker)
    brace=source.index('{',start)
    depth=0
    # These selected blocks do not contain braces inside strings/comments.
    for end in range(brace,len(source)):
        if source[end]=='{':depth+=1
        elif source[end]=='}':
            depth-=1
            if not depth:return source[start:end+1]
    raise AssertionError('Unbalanced production block: '+marker)


def vector(name,data):
    return 'static const uint8_t '+name+'[]={'+','.join(map(str,data))+'};\n'


class PersonalRuntimeRegression(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp=tempfile.TemporaryDirectory(prefix='a04-runtime-regression-')
        cls.path=Path(cls.tmp.name)
        cls.flags=['-I'+str(peer.INCLUDE),'-I'+str(peer.BEAR/'inc'),'-I'+str(peer.BEAR/'src')]
        cls.objects=[]
        for n,source in enumerate(peer.SOURCES):
            obj=cls.path/f'provider{n}.o';cls.objects.append(str(obj))
            subprocess.run(['clang','-std=c11','-O1','-g','-fsanitize=address,undefined',
                            *cls.flags,'-c',str(source),'-o',str(obj)],check=True)
        pmk=hashlib.pbkdf2_hmac('sha1',peer.PASSPHRASE,peer.SSID,4096,32)
        ptk=peer.peer_ptk(pmk)
        data=[('pass',peer.PASSPHRASE),('ssid',peer.SSID),('ap',peer.AP),('sta',peer.STA),
              ('sn',peer.SNONCE),('ptk_ref',ptk),('one',peer.peer_packet(1,7)),
              ('three',peer.peer_packet(3,8,ptk,peer.peer_wrap(ptk[16:32],peer.peer_plain()),rsc=peer.RSC))]
        cls.vectors=''.join(vector(n,b) for n,b in data)
        source=CPP.read_text()
        # Execute the actual production unlock/crypto/relock branch, not a
        # separately implemented approximation of its cancellation checks.
        cls.pump=cpp_block(source,'   if(!e && r->personal_admitted && r->delivery_kind==A04_WLAN_DELIVERY_EAPOL)')
        cls.unbind=cpp_block(source,' int unbindNative()')

    @classmethod
    def tearDownClass(cls):cls.tmp.cleanup()

    def run_case(self, body, extracted=False):
        source=peer.DRIVER+self.vectors+COMMON
        if extracted:source+=SCHEDULER_PREFIX+self.unbind+SCHEDULER_MIDDLE+self.pump+SCHEDULER_SUFFIX
        source+='\nint main(){\n'+body+'\nreturn 0;\n}\n'
        path=self.path/(self._testMethodName+'.cpp');path.write_text(source)
        exe=path.with_suffix('')
        # Runtime-only Linux images may have libstdc++.so.6 without the dev
        # linker symlink. Use the discovered SYSTEM runtime, no private path.
        runtime=[]
        if sys.platform.startswith('linux'):
            soname=ctypes.util.find_library('stdc++')
            self.assertIsNotNone(soname,'System C++ ABI runtime required by Clang sanitizers')
            runtime=['-nostdlib++','-l:'+Path(soname).name]
        subprocess.run(['clang++','-std=c++14','-O1','-g','-Wall','-Wextra','-Werror',
                        '-Wno-unused-function','-Wno-unused-const-variable',
                        '-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-exceptions','-fno-rtti',*self.flags,
                        str(path),*self.objects,*runtime,'-o',str(exe)],check=True)
        result=subprocess.run([str(exe)],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)

    def test_completed_idle_does_not_expire_at_handshake_deadline(self):
        self.run_case(r'''
complete_fixture();struct a04_wlan_runtime r;struct a04_wlan_host h;
completed_runtime(&r,&h);
assert(!a04_wlan_personal_drive(&r,&h,1000099));
assert(!a04_wlan_personal_drive(&r,&h,1000100));
assert(!a04_wlan_personal_drive(&r,&h,1000100ULL+60000000));
assert(!r.personal.failed&&a04_psk_authenticated(&r.personal));
assert(r.personal.ptk_state==2&&r.personal.gtk_state==2);
assert(fixture_probe(6)==1&&fixture_probe(7)==1); /* no reinstall */
''')

    def test_unfinished_session_still_expires(self):
        self.run_case(r'''
start_fixture();struct a04_wlan_runtime r;struct a04_wlan_host h;
memset(&r,0,sizeof(r));memset(&h,0,sizeof(h));
r.personal=fixture.state;r.personal_admitted=1;
assert(a04_wlan_personal_drive(&r,&h,1000100)==A04_WLAN_TIMEOUT);
assert(!a04_psk_authenticated(&r.personal));
''')

    def test_fresh_relock_commits_real_m1_and_clears_scratch(self):
        self.run_case(r'''
start_fixture();PumpFixture f;f.setup();
assert(!f.process());
assert(f.live->runtime.personal.phase==A04_PSK_M2_READY);
assert(!memcmp(f.live->runtime.personal.ptk,ptk_ref,64));
assert(a04_psk_zero(f.live->runtime.personal.pmk,32));
assert(fixture.calls[1]==1&&fixture.calls[2]==1);
assert(!f.live->runtime.personal_busy);
assert(a04_psk_zero((const uint8_t *)&f.live->personalWork,sizeof(f.live->personalWork)));
assert(a04_psk_zero(f.live->personalRX,sizeof(f.live->personalRX)));
''',extracted=True)

    def test_authorized_data_deadline_survives_handshake_but_eapol_stays_bounded(self):
        self.run_case(r'''
complete_fixture();struct a04_wlan_runtime r;struct a04_wlan_host h;
completed_runtime(&r,&h);uint64_t deadline=0,now=1000100ULL+60000000;
assert(!a04_wlan_data_deadline(&r.security,0,now,&deadline));
assert(deadline==now+900000);
assert(a04_wlan_data_deadline(&r.security,1,now,&deadline)==A04_WLAN_TIMEOUT);
r.security.authorized=0;
assert(a04_wlan_data_deadline(&r.security,0,now,&deadline)==A04_WLAN_TIMEOUT);
''')

    def test_unbind_during_unlocked_crypto_cannot_restore_cleared_state(self):
        self.run_case(r'''
start_fixture();PumpFixture f;f.setup();f.action=1;
int result=f.process();(void)result;
assert(f.injected&&f.unlocks>=2&&!f.nativeClient.owner);
assert(f.live->runtime.personal.failed);
assert(a04_psk_zero(f.live->runtime.personal.ptk,64));
assert(!a04_psk_authenticated(&f.live->runtime.personal));
assert(!f.live->runtime.security.authorized);
''',extracted=True)

    def test_relock_uses_fresh_time_rejects_expired_crypto_result(self):
        self.run_case(r'''
start_fixture();PumpFixture f;f.setup();f.action=2;
int result=f.process();
assert(f.injected&&f.now==1000100);
assert(result==A04_WLAN_TIMEOUT || f.live->runtime.personal.failed || f.live->runtime.failed);
assert(f.live->runtime.personal.phase!=A04_PSK_M2_READY);
assert(a04_psk_zero(f.live->runtime.personal.ptk,64));
assert(!f.live->runtime.security.authorized);
''',extracted=True)


COMMON=r'''
#include <stdio.h>
#include "A04BR3WLANPersonalRuntime.h"
static void start_fixture(){
 assert(!fixture_begin(pass,sizeof(pass),ssid,sizeof(ssid),ap,sta,sn,0,1,0));
}
static void complete_fixture(){
 start_fixture();assert(!fixture_rx(one,sizeof(one),110));
 uint8_t packet[611],key[16],rsc[8];unsigned n,id;uint64_t cookie;
 assert(!fixture_tx(packet,sizeof(packet),&n,&cookie,120));assert(!fixture_tx_ack(cookie,0,130));
 assert(!fixture_rx(three,sizeof(three),140));assert(!fixture_key(1,key,rsc,&id,&cookie,150));
 assert(!fixture_key_ack(1,cookie,0,160));assert(!fixture_key(0,key,rsc,&id,&cookie,170));
 assert(!fixture_key_ack(0,cookie,0,180));assert(!fixture_tx(packet,sizeof(packet),&n,&cookie,190));
 assert(!fixture_tx_ack(cookie,0,200));assert(fixture_probe(1));
}
static void completed_runtime(struct a04_wlan_runtime *r,struct a04_wlan_host *h){
 memset(r,0,sizeof(*r));memset(h,0,sizeof(*h));
 r->personal=fixture.state;r->station=fixture.station;r->personal_admitted=1;
 r->admin=r->configured=r->begun=1;r->association_generation=1;
 /* Explicit simulated independent firmware/security completion state. */
 r->security.enabled=r->security.ptk_confirmed=r->security.gtk_accepted=1;
 r->security.m4_confirmed=r->security.authorized=1;
 r->security.phase=3;r->security.deadline_us=1000100;
}
'''

SCHEDULER_PREFIX=r'''
/* Minimal host boundary for the EXACT production blocks copied below.
 * Locks/time/client release are simulated; PSK/runtime/crypto are actual code. */
struct PumpFixture;
struct HostLock {PumpFixture *owner;};
struct HostOwner {unsigned releases=0;void release(){releases++;}};
struct a04_wlan_native_client {HostOwner *owner=nullptr;};
static void IOLockLock(HostLock *);
static void IOLockUnlock(HostLock *);
struct PumpFixture {
 struct Live {
  struct a04_wlan_runtime runtime;
  struct a04_psk4way personalWork;uint8_t personalRX[1514];unsigned count=0;
 } storage;
 Live *live=&storage;
 HostOwner retainedOwner;struct a04_wlan_native_client nativeClient,client;
 HostLock lock={this};HostLock *transportLock=&lock;
 bool liveStopping=false,held=true,injected=false;
 unsigned action=0,unlocks=0;uint64_t now=150,nativeEpoch=1;
 uint64_t liveNow(){return now;}
 void setup(){
  memset(&storage,0,sizeof(storage));
  storage.runtime.personal=fixture.state;storage.runtime.station=fixture.station;
  storage.runtime.personal_admitted=1;storage.runtime.association_generation=1;
  storage.runtime.configured=storage.runtime.admin=1;
  assert(!a04_wlan_security_begin(&storage.runtime.security,&storage.runtime.station,100,1000100));
  static uint8_t ethernet[14+sizeof(one)];memset(ethernet,0,sizeof(ethernet));memcpy(ethernet+14,one,sizeof(one));
  storage.runtime.delivery=ethernet;storage.runtime.delivery_length=sizeof(ethernet);
  storage.runtime.delivery_kind=A04_WLAN_DELIVERY_EAPOL;
  nativeClient.owner=&retainedOwner;client=nativeClient;
 }
'''
SCHEDULER_MIDDLE=r'''
 int process(){
  struct a04_wlan_runtime *r=&live->runtime;int e=0;uint64_t now=liveNow();
  for(unsigned once=0;once<1;once++){
'''
SCHEDULER_SUFFIX=r'''
  }
  return e;
 }
};
static void IOLockUnlock(HostLock *lock){
 assert(lock->owner->held);lock->owner->held=false;lock->owner->unlocks++;
}
static void IOLockLock(HostLock *lock){
 PumpFixture *f=lock->owner;assert(!f->held);
 if(!f->injected){
  f->injected=true;
  if(f->action==1)assert(!f->unbindNative()); /* scheduled BEFORE original relock */
  if(f->action==2)f->now=1000100; /* no sleeps, exact deadline */
 }
 f->held=true;
}
'''

if __name__=='__main__':unittest.main()
