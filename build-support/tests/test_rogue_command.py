# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Execute production typed KCCB transport against a simulated firmware peer.
No hardware or binary analysis. Peer validates exact bytes before writing UFO.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest

GPU=Path(__file__).resolve().parents[2]/'iokit/Kernel/arm/rogue'

class RogueCommand(unittest.TestCase):
    def test_real_ring_bytes_response_order_bounds_and_fail_closed(self):
        source=r'''
#include <assert.h>
#include <string.h>
#include "A04BR3RogueCommand.h"
static unsigned char mainbuf[A04_RGX_MAIN_BYTES] __attribute__((aligned(4096)));
static unsigned char config[A04_RGX_CONFIG_BYTES] __attribute__((aligned(4096)));
static a04_rgx_fw_runtime r;static a04_rgx_command_state state;
static unsigned order,kicks,waits,acquires,mode,wantedDM;
static void publish(void *) {
 ++order;
 if(order==1) {assert(*r.command_sync==0);assert(r.kctl->ui32WriteOffset==r.kctl->ui32ReadOffset);}
}
static void acquire(void *) {++acquires;}
static void kick(void *,uint32_t reg,uint32_t value) {
 assert(order==2 && reg==0xb00 && value==0x10);++kicks;
 unsigned at=r.kctl->ui32ReadOffset;
 assert(r.kctl->ui32WriteOffset==((at+1)&127));
 RGXFWIF_KCCB_CMD expected={};expected.eCmdType=(RGXFWIF_KCCB_CMD_TYPE)0x2abc0068;
 expected.eDM=wantedDM;expected.uCmdData.sSyncData.sSyncObjDevVAddr=r.command_sync_addr;
 expected.uCmdData.sSyncData.uiUpdateVal=state.sequence;
 assert(!memcmp(&expected,&r.kcommands[at],56));
 assert(*r.command_sync==0);
}
static void wait(void *,uint32_t us) {
 assert(us==10);++waits;
 if(mode==0 && waits==2) {*r.command_sync=state.sequence;r.kctl->ui32ReadOffset=r.kctl->ui32WriteOffset;}
 if(mode==1) r.kctl->ui32ReadOffset=r.kctl->ui32WriteOffset; /* consume != execute */
 if(mode==2) *r.command_sync=state.sequence; /* UFO != consumption */
 if(mode==3) *r.command_sync=0xf00d; /* stale or rogue write */
 if(mode==4) r.kctl->ui32ReadOffset=128;
}
static a04_rgx_command_io io={nullptr,publish,acquire,kick,wait};
static void reset(){assert(!a04_rgx_fw_runtime_init(&r,mainbuf,0x80000000,config,0x81000000,26000000));
 state={};order=kicks=waits=acquires=mode=0;wantedDM=0;}
static int fence(){return a04_rgx_command_fence(&r,&state,&io,wantedDM,10,10);}
int main(){
 reset();
 /* >2 full ring wraps: every command consumed, exact UFO response required. */
 for(unsigned i=0;i<300;i++) {
  wantedDM=i%3==0?0:(i%3==1?2:3);order=waits=0;
  assert(!fence());assert(state.sequence==i+1);assert(waits==2);
 }
 assert(kicks==300 && !state.poisoned);
 for(unsigned bad=1;bad<=4;bad++){
  reset();mode=bad;assert(fence()==(bad<=2?A04_CCB_TIMEOUT:A04_CCB_CORRUPT));
  assert(state.poisoned);unsigned old=kicks;assert(fence()==A04_CCB_OFFLINE && kicks==old);
 }
 reset();*r.command_sync=1;mode=1; /* prior completion never satisfies next submission */
 assert(fence()==A04_CCB_TIMEOUT && waits==10);
 reset();r.kctl->ui32WriteOffset=127;assert(fence()==A04_CCB_FULL && !kicks && !state.sequence);
 reset();r.kctl->ui32ReadOffset=128;assert(fence()==A04_CCB_CORRUPT && !kicks);
 reset();r.kctl->ui32CmdSize=8;assert(fence()==A04_CCB_CORRUPT && !kicks);
 reset();r.kctl->ui32WrapMask=255;assert(fence()==A04_CCB_CORRUPT && !kicks);
 reset();r.kctl->ui32WriteOffset=2;assert(fence()==A04_CCB_CORRUPT && !kicks);
 reset();r.fctl->ui32WriteOffset=1;assert(fence()==A04_CCB_FW_REQUEST && !kicks && !state.sequence);
 reset();r.fctl->ui32ReadOffset=32;assert(fence()==A04_CCB_CORRUPT && !kicks);
 reset();wantedDM=4;assert(fence()==A04_CCB_INPUT && !kicks);
 reset();state.sequence=0xffffffff;assert(fence()==A04_CCB_INPUT && !kicks);
 reset();assert(a04_rgx_command_fence(&r,&state,&io,0,10001,10)==A04_CCB_INPUT && !kicks);
 reset();auto saved=r.init->psKernelCCB;
 r.init->psKernelCCB=r.init->psKernelCCBCtl;assert(fence()==A04_CCB_INPUT && !kicks);
 r.init->psKernelCCB.ui32Addr=0xfffffffc;assert(fence()==A04_CCB_INPUT && !kicks);
 r.init->psKernelCCB.ui32Addr=0xc0040000+r.main.used-4;assert(fence()==A04_CCB_INPUT && !kicks);
 r.init->psKernelCCB=saved;r.command_sync_addr.ui32Addr=r.init->psKernelCCB.ui32Addr;
 assert(fence()==A04_CCB_INPUT && !kicks);
 reset();auto ctl=r.kctl;r.kctl=nullptr;assert(fence()==A04_CCB_INPUT && !kicks);r.kctl=ctl;
 reset();io.kick=nullptr;assert(fence()==A04_CCB_INPUT && !kicks);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);(p/'test.cpp').write_text(source)
            subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                '-I',str(GPU),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)

if __name__=='__main__':unittest.main()
