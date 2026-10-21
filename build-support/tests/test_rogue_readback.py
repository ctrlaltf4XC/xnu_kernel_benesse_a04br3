# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Native global SLC flush followed by exact GP SYNC; no pixel simulation claim."""
from pathlib import Path
import subprocess
import tempfile
import unittest

GPU=Path(__file__).resolve().parents[2]/'iokit/Kernel/arm/rogue'

class RogueReadback(unittest.TestCase):
    def test_exact_ddk_batch_wrap_partial_completion_timeout_and_reject(self):
        source=r'''
#include <assert.h>
#include <string.h>
#include "A04BR3RogueReadback.h"
alignas(4096) static unsigned char mainbuf[A04_RGX_MAIN_BYTES],config[A04_RGX_CONFIG_BYTES];
static a04_rgx_fw_runtime r;static a04_rgx_command_state state;
static a04_rgx_readback_observation observed;
static unsigned mode,kicks,waits,published,start,expected;
static void publish(void *){++published;}static void acquire(void *){}
static void kick(void *,uint32_t reg,uint32_t value){
 assert(reg==0xb00 && value==0x10 && published==2);++kicks;
 expected=(start+2)&127;assert(r.kctl->ui32WriteOffset==expected);
 // Scalar ABI oracle, no production-generated expected structs.
 auto p=(unsigned char *)&r.kcommands[start];unsigned char wanted[56]={};
 wanted[0]=0x69;wanted[2]=0xbc;wanted[3]=0x2a;wanted[20]=1; // union16+4=bInval
 assert(!memcmp(p,wanted,56));
 p=(unsigned char *)&r.kcommands[(start+1)&127];
 assert(a04_rgx_fw_u32(p)==0x2abc0068 && a04_rgx_fw_u32(p+4)==0);
 assert(a04_rgx_fw_u32(p+16)==r.command_sync_addr.ui32Addr && a04_rgx_fw_u32(p+20)==state.sequence);
 for(unsigned i=8;i<16;i++)assert(!p[i]);for(unsigned i=24;i<56;i++)assert(!p[i]);assert(!*r.command_sync);
}
static void wait(void *,uint32_t us){
 assert(us==10);++waits;
 if(mode==1)return;
 if(mode!=3)r.kctl->ui32ReadOffset=expected;
 if(mode!=2)*r.command_sync=mode==4?state.sequence+1:state.sequence;
 if(mode==5)r.kctl->ui32WriteOffset=99;
}
static a04_rgx_command_io io={nullptr,publish,acquire,kick,wait};
static void reset(unsigned at=0){
 assert(!a04_rgx_fw_runtime_init(&r,mainbuf,0x83000000,config,0x84000000,26000000));
 r.init->bFirmwareStarted=IMG_TRUE;r.init->sRGXCompChecks.bUpdated=IMG_TRUE;
 auto &c=r.init->sRGXCompChecks;c.ui32DDKVersion=0x1000b;c.ui32DDKBuild=5425693;c.ui32FWProcessorVersion=1;
 c.sFWBVNC.ui64BVNC=c.sHWBVNC.ui64BVNC=0x001600280036001eULL;
 r.kctl->ui32ReadOffset=r.kctl->ui32WriteOffset=at;start=at;
 state={};observed={};mode=kicks=waits=published=0;
}
int main(){
 for(unsigned at: {0U,126U,127U}){
  reset(at);assert(!a04_rgx_command_readback(&r,&state,&io,&observed));
  assert(kicks==1 && waits==1 && observed.valid && observed.kread==((at+2)&127) && observed.ufo==1 && !state.poisoned);
 }
 for(unsigned m=1;m<=5;m++){
  reset();mode=m;int error=a04_rgx_command_readback(&r,&state,&io,&observed);
  assert(error==(m<4?A04_CCB_TIMEOUT:A04_CCB_CORRUPT));
  assert(kicks==1 && state.poisoned && observed.valid);
  if(m<4)assert(waits==10000 && observed.polls==10000);
  assert(a04_rgx_command_readback(&r,&state,&io,&observed)==A04_CCB_OFFLINE && kicks==1);
 }
 reset(15);state.sequence=6;*r.command_sync=6;r.fctl->ui32WriteOffset=2;
 assert(a04_rgx_command_readback(&r,&state,&io,&observed)==A04_CCB_FW_REQUEST && !kicks && state.sequence==6);
 assert(observed.valid && !observed.submitted && !observed.polls && observed.kread==15 && observed.kwrite==15 && observed.kexpected==15 && observed.fread==0 && observed.fwrite==2 && observed.ufo==6 && observed.expected==6);
 reset();r.kctl->ui32ReadOffset=1;auto before=r.kcommands[0];
 assert(a04_rgx_command_readback(&r,&state,&io,&observed)==A04_CCB_BUSY && !kicks && !memcmp(&before,r.kcommands,56));
 reset();r.kctl->ui32WrapMask=63;
 assert(a04_rgx_command_readback(&r,&state,&io,&observed)==A04_CCB_CORRUPT && !kicks && state.poisoned);
 reset();state.sequence=0xffffffffU;
 assert(a04_rgx_command_readback(&r,&state,&io,&observed)==A04_CCB_INPUT && !kicks);
 reset();assert(a04_rgx_command_readback(&r,&state,&io,(a04_rgx_readback_observation*)r.main.cpu)==A04_CCB_INPUT && !kicks);
 reset();r.init->sRGXCompChecks.ui32DDKBuild=0;
 assert(a04_rgx_command_readback(&r,&state,&io,&observed)==A04_CCB_OFFLINE && !kicks);
 reset();r.init->bFirmwareStarted=IMG_FALSE;
 assert(a04_rgx_command_readback(&r,&state,&io,&observed)==A04_CCB_OFFLINE && !kicks);
}
'''.replace('for(unsigned at: {0U,126U,127U})','const unsigned starts[]={0,126,127};for(unsigned at: starts)')
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);(p/'test.cpp').write_text(source)
            subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',
                            '-fsanitize=address,undefined','-I',str(GPU),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)

if __name__=='__main__':unittest.main()
