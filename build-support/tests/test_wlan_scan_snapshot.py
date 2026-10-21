# SPDX-License-Identifier: MIT
"""Primary MT6630 AIS scan wire and real service snapshot generation tests."""
from pathlib import Path
import unittest
from unittest.mock import patch
import test_wmt_protocol as codecs
from test_wlan_runtime_backend import LIVE
import test_wlan_service_backend as service

class WLANScanSnapshot(unittest.TestCase):
    run_c=codecs.WMTProtocol.run_c

    def test_native_v2_wire_and_sleepy_not_scan_done(self):
        native=(Path(__file__).resolve().parent/'fixtures/mt6630_native/scan_native_fixture.h').read_text()
        defs='#define MAC_ADDR_LEN 6\n'+'typedef uint8_t UINT_8;typedef uint16_t UINT_16;typedef uint32_t UINT_32;\n'
        self.run_c(LIVE+'\n#include <stddef.h>\n'+defs+native+r'''
int main(void){
 assert(SCAN_TYPE_PASSIVE_SCAN==0 && SCAN_TYPE_ACTIVE_SCAN==1);
 assert(EVENT_ID_SLEEPY_INFO==7 && EVENT_ID_SCAN_DONE==0x0d && sizeof(EVENT_SLEEPY_INFO_T)==4);
 assert(sizeof(CHANNEL_INFO_T)==2 && sizeof(PARAM_SSID_T)==36 && offsetof(CMD_SCAN_REQ_V2,aucIE)==494);
 assert(offsetof(CMD_SCAN_REQ_V2,arChannelList)==384 && offsetof(CMD_SCAN_REQ_V2,u2IELen)==492);
 struct card c;struct a04_sdio_io io;struct a04_sdio_transport t;struct a04_wlan_host h;struct a04_wlan_runtime r;
 uint8_t nv[512];board(nv);init_live(&c,&io,&t,&h,&r);assert(!a04_wlan_runtime_calibration(&r,nv,512));assert(!a04_wlan_live_begin(&r,&h,1));capability(&r,0);assert(!a04_wlan_runtime_step(&r,&h,2,0,0));
 CMD_SCAN_REQ_V2 expected;memset(&expected,0,sizeof(expected));expected.ucSeqNum=1;expected.ucBssIndex=r.station.local.bss;expected.ucStructVersion=3;memset(expected.aucBSSID,255,6);
 expected.ucChannelType=4;expected.ucChannelListNum=13;for(unsigned i=0;i<13;i++){expected.arChannelList[i].ucBand=1;expected.arChannelList[i].ucChannelNum=(uint8_t)(i+1);}
 assert(h.last_cid==3 && h.last_cmd_bytes==502 && !memcmp(captured[8]+8,&expected,offsetof(CMD_SCAN_REQ_V2,aucIE)));
 assert(r.scan_generation==1 && r.scan_started_us==2 && r.station.deadline_us==30000002);
 uint8_t sleepy[4]={1,0,0,0};for(unsigned i=0;i<12;i++){event(EVENT_ID_SLEEPY_INFO,0,sleepy,4,i&1);assert(!a04_wlan_runtime_step(&r,&h,3+i,0,0));}
 assert(r.sleepy_events==12 && r.sleepy_state==1 && !r.station.scan.complete && !r.scan_completed_us);
 assert(a04_wlan_runtime_step(&r,&h,6000000,0,0)==A04_WLAN_PENDING && !r.failed);
 uint8_t done[16]={1,0,0,0,13,0,3,0};event(EVENT_ID_SCAN_DONE,0,done,sizeof(done),1);
 assert(!a04_wlan_runtime_step(&r,&h,6000001,0,0) && r.station.scan.complete && r.scan_completed_us==6000001);
 assert(!r.scan_count && r.scan_generation==1); /* actual empty completion only */
 return 0;
}
''')

    def test_actual_cpp_full_ie_atomic_generation_and_freshness(self):
        body=r'''
static void beacon(void){
 memset(inbound,0,sizeof(inbound));unsigned off=40;inbound[off]=0x50;memset(inbound+off+4,255,6);
 const uint8_t mac[]={2,6,7,8,9,10},ies[]={0,4,'t','e','s','t',1,4,0x82,0x84,0x8b,0x96,3,1,6};
 memcpy(inbound+off+10,mac,6);memcpy(inbound+off+16,mac,6);a04_wmt_put16(inbound+off+32,100);a04_wmt_put16(inbound+off+34,0x401);
 memcpy(inbound+off+36,ies,sizeof(ies));unsigned n=off+36+sizeof(ies);
 for(unsigned i=0;i<5;i++){inbound[n++]=221;inbound[n++]=250;for(unsigned j=0;j<250;j++)inbound[n++]=(uint8_t)(i+j);}
 a04_wmt_put16(inbound,n);a04_wmt_put16(inbound+2,0x4800);inbound[5]=6;inbound[6]=24;inbound[25]=100;inbound_size=n;inbound_port=0;
}
int main(){
 A04BR3SDIO s;prepare(s);registrationError=EALREADY;s.deferLive();tick();capability(&s.live->runtime,0);tick();
 a04_wlan_scan_info info;assert(!a04br3_wlan_scan_snapshot(&s,&info) && info.generation==1 && !info.complete && !info.count);
 auto *out=(a04_wlan_scan_record *)IOMalloc(sizeof(a04_wlan_scan_record));assert(out);
 assert(a04br3_wlan_scan_copy_record(&s,1,0,out)==EAGAIN);
 beacon();uint8_t expected[1300];unsigned len=inbound_size-76;assert(len>1024);memcpy(expected,inbound+76,len);tick();uint64_t observed=mockNow;
 assert(!a04br3_wlan_scan_snapshot(&s,&info) && info.count==1 && !info.complete);
 uint8_t done[4]={1,0,0,0};event(0x0d,0,done,4,1);tick();
 assert(!a04br3_wlan_scan_snapshot(&s,&info) && info.complete && info.completed_us==mockNow);
 assert(!a04br3_wlan_scan_copy_record(&s,1,0,out) && out->observation.generation==1 && out->observation.observed_us==observed);
 assert(out->observation.from_probe && out->observation.ie_bytes==len && !memcmp(out->observation.ies,expected,len));
 for(unsigned i=len;i<A04_WLAN_SCAN_MAX_IE;i++)assert(!out->observation.ies[i]);
 assert(a04br3_wlan_scan_copy_record(&s,1,1,out)==ENOENT);
 // Source10s freshness: a stale observation never becomes a join attempt.
 mockNow=observed+10000000;unsigned tx=c.txs;assert(a04br3_wlan_join(&s,0,0)==EAGAIN && c.txs==tx);
 assert(!a04br3_wlan_scan(&s));assert(!a04br3_wlan_scan_snapshot(&s,&info) && info.generation==2 && !info.complete && !info.count);
 assert(a04br3_wlan_scan_copy_record(&s,1,0,out)==EAGAIN);
 beacon();tick();done[0]=2;event(0x0d,0,done,4,1);tick();
 assert(!a04br3_wlan_scan_copy_record(&s,2,0,out) && out->observation.generation==2);
 assert(!a04br3_wlan_join(&s,0,0) && s.live->runtime.station.channel_pending && !s.live->runtime.station.sta_active);
 IOFree(out,sizeof(*out));s.stop(nullptr);s.free();assert(!refs && !heldLocks);return 0;
}
'''
        with patch.object(service,'STUBS',service.STUBS.replace('int main(){','int original_lifecycle_main(){')+body):
            service.WLANServiceBackend('test_actual_cpp_deferred_boot_callbacks_and_stop').test_actual_cpp_deferred_boot_callbacks_and_stop()

    def test_actual_mode_api_never_substitutes_active_with_passive(self):
        body=r'''
int main(){
 A04BR3SDIO s;prepare(s);registrationError=EALREADY;s.deferLive();tick();capability(&s.live->runtime,0);tick();
 auto *r=&s.live->runtime;unsigned tx=c.txs,generation=r->scan_generation,accepted=999;
 static_assert(A04_WLAN_SCAN_MODE_API==1,"native adapter must select real mode API");
 int (*modeAPI)(IOService *,unsigned,unsigned *)=a04br3_wlan_scan_mode;
 assert(modeAPI(&s,1,nullptr)==EINVAL && accepted==999 && c.txs==tx);
 assert(modeAPI(nullptr,1,&accepted)==ENXIO && accepted==0);
 assert(a04br3_wlan_scan_mode(&s,1,&accepted)==EBUSY && accepted==0 && c.txs==tx && r->scan_generation==generation);
 uint8_t done[4]={1,0,0,0};event(0x0d,0,done,4,1);tick();
 assert(a04br3_wlan_scan_mode(&s,3,&accepted)==EINVAL && accepted==0 && c.txs==tx);
 r->calibration[10]='J';assert(a04br3_wlan_scan_mode(&s,1,&accepted)==EOPNOTSUPP && accepted==0 && c.txs==tx);r->calibration[10]=0;
 r->calibration_valid=0;assert(a04br3_wlan_scan_mode(&s,1,&accepted)==EIO && accepted==0 && c.txs==tx);r->calibration_valid=1;
 unsigned allowed=r->station.local.permitted_channels;r->station.local.permitted_channels=1u<<14;
 assert(a04br3_wlan_scan_mode(&s,1,&accepted)==EOPNOTSUPP && accepted==0 && c.txs==tx);r->station.local.permitted_channels=allowed;
 atomic_generation=&accepted;expected_generation=2;check_generation_unlock=true;
 assert(!r->station.local.active_channels && !a04br3_wlan_scan_mode(&s,1,&accepted) && c.txs==tx+1 && accepted==2);
 const uint8_t *wire=captured[captures-1]+8;
 assert(wire[2]==1 && wire[3]==1 && wire[4]==0 && wire[5]==0 && wire[7]==3 && wire[383]==13);
 for(unsigned i=0;i<13;i++)assert(wire[384+2*i]==1 && wire[385+2*i]==i+1);
 assert(!r->station.local.active_channels); /* permission not left enabled */
 a04_wlan_scan_info info;assert(!a04br3_wlan_scan_snapshot(&s,&info) && info.mode==1 && info.generation==2 && !info.complete);
 tx=c.txs;assert(a04br3_wlan_scan(&s)==EBUSY && c.txs==tx); // not coalesced with active
 done[0]=2;event(0x0d,0,done,4,1);tick();
 assert(!a04br3_wlan_scan(&s) && c.txs==tx+1 && captured[captures-1][10]==0);
 assert(!a04br3_wlan_scan_snapshot(&s,&info) && info.mode==0 && info.generation==3);
 done[0]=3;event(0x0d,0,done,4,1);tick();
 c.fail_tx=c.txs+1;assert(a04br3_wlan_scan_mode(&s,1,&accepted)==EIO && accepted==0 && r->failed && s.wlanHost->failed);
 tx=c.txs;assert(a04br3_wlan_scan_mode(&s,1,&accepted)==ENXIO && accepted==0 && c.txs==tx);
 s.stop(nullptr);s.free();assert(!refs && !heldLocks);return 0;
}
'''
        stubs=service.STUBS.replace('int main(){','int original_lifecycle_main(){')
        stubs=stubs.replace('struct IOLock{bool held;};','static unsigned *atomic_generation;static unsigned expected_generation;static bool check_generation_unlock;\nstruct IOLock{bool held;};')
        stubs=stubs.replace('assert(p->held);p->held=false;','assert(p->held);if(check_generation_unlock){assert(*atomic_generation==expected_generation);check_generation_unlock=false;}p->held=false;')
        with patch.object(service,'STUBS',stubs+body):
            service.WLANServiceBackend('test_actual_cpp_deferred_boot_callbacks_and_stop').test_actual_cpp_deferred_boot_callbacks_and_stop()

if __name__=='__main__':unittest.main()
