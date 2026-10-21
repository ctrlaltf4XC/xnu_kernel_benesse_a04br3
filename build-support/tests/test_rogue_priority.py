# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Native TA/3D ContextSetPriority protocol; never a draw/UFO proof."""
from pathlib import Path
import subprocess
import tempfile
import unittest

GPU=Path(__file__).resolve().parents[2]/'iokit/Kernel/arm/rogue'

class RoguePriority(unittest.TestCase):
    def test_priority_roundtrip_wrap_and_independent_firmware_response(self):
        code=r'''
#include <assert.h>
#include <string.h>
#include "A04BR3RogueRenderSubmit.h"
alignas(4096) static unsigned char pc[4096],pd[4096],pt[4096],scratch[65536],bank[65536];
alignas(4096) static unsigned char mainbuf[A04_RGX_MAIN_BYTES],config[A04_RGX_CONFIG_BYTES];
static a04_rgx_render_resources r;static a04_rgx_render_ring_state state;
static a04_rgx_fw_runtime kernel;static a04_rgx_render_observation terminal;
static bool tq;
static unsigned dm,target,mode,kicks,waits,published,padding;
static void publish(void *){++published;}
static void acquire(void *){}
static void kick(void *,uint32_t reg,uint32_t value){
 assert(reg==0xb00 && value==0x10 && !(published&1));++kicks;
 auto c=a04_rgx_render_context(&r,dm,tq);auto index=a04_rgx_render_index(dm,tq);
 auto ctl=r.control[index];unsigned at=ctl->ui32ReadOffset;
 auto header=(RGXFWIF_CCB_CMD_HEADER*)(r.commands[index]+at);
 if(header->eCmdType==A04_RGX_CLIENT_PADDING){
  assert(header->ui32CmdSize==4096-at-40);++padding;at=0;
  header=(RGXFWIF_CCB_CMD_HEADER*)r.commands[index];
 }
 RGXFWIF_CCB_CMD_HEADER expected={};expected.eCmdType=0x2abc00d9;expected.ui32CmdSize=8;
 assert(!memcmp(header,&expected,40)); // no NULL or TA/3D task header
 unsigned char payload[8]={};memcpy(payload,&target,4);
 assert(!memcmp((char*)header+40,payload,8));
 assert(ctl->ui32WriteOffset==ctl->ui32ReadOffset && c->ui32Priority!=target);
 RGXFWIF_KCCB_CMD expectedKick={};expectedKick.eCmdType=RGXFWIF_KCCB_CMD_KICK;expectedKick.eDM=dm;
 expectedKick.uCmdData.sCmdKickData.psContext.ui32Addr=a04_rgx_render_fw_address(&r,c);
 expectedKick.uCmdData.sCmdKickData.ui32CWoffUpdate=at+48;
 assert(!memcmp(&kernel.kcommands[kernel.kctl->ui32ReadOffset],&expectedKick,56));
 assert(*r.sync==0xa55a55aa); // CPU did not fabricate any UFO completion
}
static void wait(void *,uint32_t us){
 assert(us==10);++waits;auto c=a04_rgx_render_context(&r,dm,tq);
 auto ctl=r.control[a04_rgx_render_index(dm,tq)];
 if(mode==1)return;
 if(mode==5){ctl->ui32ReadOffset=4096;return;}
 if(mode!=3)kernel.kctl->ui32ReadOffset=kernel.kctl->ui32WriteOffset;
 if(mode==2)return;
 if(mode!=4)c->ui32Priority=target;
 if(mode==3)return;
 ctl->ui32ReadOffset=ctl->ui32WriteOffset=ctl->ui32DepOffset=state.write[a04_rgx_render_index(dm,tq)];
}
static a04_rgx_command_io io={nullptr,publish,acquire,kick,wait};
static void reset(){
 a04_rgx_bif bif={{pc,0x80000000,4096},{pd,0x80001000,4096},{pt,0x80002000,4096},
  {scratch,0x81000000,65536},A04_RGX_RENDER_SCRATCH_VA,65536,false};
 a04_rgx_render_buffer b={bank,0x82000000,65536};assert(!a04_rgx_render_build(&r,&b,&bif,1));
 assert(!a04_rgx_fw_runtime_init(&kernel,mainbuf,0x83000000,config,0x84000000,26000000));
 auto init=kernel.init;init->bFirmwareStarted=IMG_TRUE;auto &c=init->sRGXCompChecks;
 c.bUpdated=IMG_TRUE;c.ui32DDKVersion=0x1000b;c.ui32DDKBuild=5425693;c.ui32FWProcessorVersion=1;
 c.sFWBVNC.ui64BVNC=c.sHWBVNC.ui64BVNC=0x001600280036001eULL;
 *r.sync=0xa55a55aa;state={};terminal={};tq=false;dm=2;target=0;mode=kicks=waits=published=padding=0;
}
int main(){
 reset();
 for(unsigned n=0;n<400;n++){
  dm=n%2?3:2;target=(n/2)%2;
  assert(a04_rgx_render_submit_update(&r,&state,&kernel,&io,dm,&terminal,target,tq)==A04_CCB_OK);
  assert(a04_rgx_render_context(&r,dm,tq)->ui32Priority==target && *r.sync==0xa55a55aa);
 }
 assert(kicks==400 && padding>=4 && !terminal.valid);
 reset();tq=true;dm=3;
 assert(r.tq!=&r.context->s3DContext && r.tq->ui32ServerCommonContextID==4);
 for(unsigned n=0;n<200;n++) {
  target=n%2;assert(a04_rgx_render_submit_update(&r,&state,&kernel,&io,3,&terminal,target,true)==A04_CCB_OK);
  assert(r.tq->ui32Priority==target && r.context->s3DContext.ui32Priority==1);
 }
 assert(kicks==200 && !terminal.valid);
 reset();assert(a04_rgx_render_submit_update(&r,&state,&kernel,&io,3,&terminal,0xffffffffU,true)==A04_CCB_INPUT && !kicks);
 reset();assert(a04_rgx_render_submit_update(&r,&state,&kernel,&io,2,&terminal,0,true)==A04_CCB_INPUT && !kicks);
 for(unsigned f=1;f<=5;f++){
  reset();mode=f;
  assert(a04_rgx_render_submit_update(&r,&state,&kernel,&io,dm,&terminal,target,tq)==(f==5?A04_CCB_CORRUPT:A04_CCB_TIMEOUT));
  assert(terminal.valid && terminal.priorityExpected==target && terminal.dm==2);
  assert(terminal.ufo==0xa55a55aa && *r.sync==0xa55a55aa);
  assert(waits==(f==5?1U:10000U));auto snapshot=terminal;
  assert(a04_rgx_render_submit_update(&r,&state,&kernel,&io,dm,&terminal,target,tq)==A04_CCB_OFFLINE);
  assert(kicks==1 && !memcmp(&terminal,&snapshot,sizeof(snapshot)));
 }
 reset();assert(a04_rgx_render_submit_update(&r,&state,&kernel,&io,0,&terminal,0)==A04_CCB_INPUT && !kicks);
 reset();assert(a04_rgx_render_submit_update(&r,&state,&kernel,&io,2,&terminal,2)==A04_CCB_INPUT && !kicks);
 reset();assert(a04_rgx_render_submit_update(&r,&state,&kernel,&io,2,&terminal,1)==A04_CCB_INPUT && !kicks && !state.sequence);
 reset();kernel.fctl->ui32WriteOffset=1;
 assert(a04_rgx_render_submit_update(&r,&state,&kernel,&io,2,&terminal,0)==A04_CCB_FW_REQUEST && !kicks);
 reset();r.control[0]->ui32ReadOffset=8;
 assert(a04_rgx_render_submit_update(&r,&state,&kernel,&io,2,&terminal,0)==A04_CCB_BUSY && !kicks);
}
'''
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);(p/'test.cpp').write_text(code)
            subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                '-I',str(GPU),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)
