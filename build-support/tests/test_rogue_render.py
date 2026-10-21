# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Pinned DDK BIF tables/native render context/client CCB execution tests."""
from pathlib import Path
import subprocess
import tempfile
import unittest

GPU=Path(__file__).resolve().parents[2]/'iokit/Kernel/arm/rogue'

class RogueRender(unittest.TestCase):
    def test_bif_context_client_kick_bytes_wrap_response_and_fail_closed(self):
        source=r'''
#include <assert.h>
#include <string.h>
#include "A04BR3RogueRenderSubmit.h"
static unsigned char pc[4096],pd[4096],pt[16384],data[0x402000],bank[65536];
static unsigned char mainbuf[A04_RGX_MAIN_BYTES],config[A04_RGX_CONFIG_BYTES];
static uint32_t table[8192];
static a04_rgx_render_resources r;static a04_rgx_render_ring_state state;
static a04_rgx_render_observation observed;
static a04_rgx_fw_runtime kernel;static a04_rgx_bif bif;
static unsigned dm=3,mode,waits,kicks,published,padding;
static void publish(void *) {++published;}
static void acquire(void *) {}
static void kick(void *,uint32_t reg,uint32_t value) {
 assert(reg==0xb00 && value==0x10 && !(published&1));++kicks;
 auto &k=kernel.kcommands[kernel.kctl->ui32ReadOffset];
 RGXFWIF_KCCB_CMD expected={};expected.eCmdType=RGXFWIF_KCCB_CMD_KICK;expected.eDM=dm;
 auto c=a04_rgx_render_context(&r,dm);
 expected.uCmdData.sCmdKickData.psContext.ui32Addr=a04_rgx_render_fw_address(&r,c);
 expected.uCmdData.sCmdKickData.ui32CWoffUpdate=state.write[a04_rgx_render_index(dm)];
 assert(!memcmp(&expected,&k,sizeof(k)));
 auto ctl=r.control[a04_rgx_render_index(dm)];unsigned at=ctl->ui32ReadOffset;
 auto header=(RGXFWIF_CCB_CMD_HEADER*)(r.commands[a04_rgx_render_index(dm)]+at);
 if(header->eCmdType==A04_RGX_CLIENT_PADDING){
  assert(header->ui32CmdSize==4096-at-40);++padding;at=0;
  header=(RGXFWIF_CCB_CMD_HEADER*)r.commands[a04_rgx_render_index(dm)];
 }
 if(dm==0){
  RGXFWIF_CCB_CMD_HEADER nullHeader={};nullHeader.eCmdType=0x2abc80cf;
  assert(!memcmp(header,&nullHeader,40)); /* GP only, native zero-payload NULL */
  header=(RGXFWIF_CCB_CMD_HEADER*)((char*)header+40);
 }
 RGXFWIF_CCB_CMD_HEADER eh={};eh.eCmdType=0x2abc00d6;eh.ui32CmdSize=8;
 assert(!memcmp(header,&eh,40));
 auto ufo=(RGXFWIF_UFO*)((char*)header+40);
 assert(ufo->puiAddrUFO.ui32Addr==r.sync_addr.ui32Addr && ufo->ui32Value==state.sequence);
 assert(*r.sync==0 && ctl->ui32WriteOffset==ctl->ui32ReadOffset);
}
static void wait(void *,uint32_t us) {
 assert(us==10);++waits;
 if(mode==8){
  kernel.fctl->ui32WriteOffset=1;
  auto commands=(RGXFWIF_FWCCB_CMD*)a04_rgx_command_memory(&kernel,kernel.init->psFirmwareCCB.ui32Addr,32*40);
  commands[0].eCmdType=(RGXFWIF_FWCCB_CMD_TYPE)0x2abc0067;return;
 }
 if(mode==1)return; /* FW unresponsive */
 if(mode==2){kernel.kctl->ui32ReadOffset=kernel.kctl->ui32WriteOffset;return;}
 if(mode==3){*r.sync=state.sequence;return;}
 if(mode==4){*r.sync=state.sequence+1;return;}
 if(mode==5){r.control[a04_rgx_render_index(dm)]->ui32ReadOffset=4096;return;}
 if(mode==6){r.control[a04_rgx_render_index(dm)]->ui32WriteOffset=4096;return;}
 if(mode==7){r.control[a04_rgx_render_index(dm)]->ui32DepOffset=1;return;}
 if(waits%2)return;
 auto ctl=r.control[a04_rgx_render_index(dm)];ctl->ui32ReadOffset=ctl->ui32WriteOffset=ctl->ui32DepOffset=state.write[a04_rgx_render_index(dm)];
 kernel.kctl->ui32ReadOffset=kernel.kctl->ui32WriteOffset;*r.sync=state.sequence;
}
static a04_rgx_command_io io={nullptr,publish,acquire,kick,wait};
static void reset() {
 bif={{pc,0x80000000,4096},{pd,0x80001000,4096},{pt,0x80002000,16384},
      {data,0x81000000,sizeof(data)},0x65c01ff000ULL,sizeof(data),false};
 a04_rgx_render_buffer b={bank,0x82000000,65536};
 assert(!a04_rgx_render_build(&r,&b,&bif,10));assert(a04_rgx_render_view_valid(&r));
 assert(!a04_rgx_fw_runtime_init(&kernel,mainbuf,0x83000000,config,0x84000000,26000000));
 kernel.init->bFirmwareStarted=IMG_TRUE;auto &c=kernel.init->sRGXCompChecks;
 c.bUpdated=IMG_TRUE;c.ui32DDKVersion=0x1000b;c.ui32DDKBuild=5425693;c.ui32FWProcessorVersion=1;
 c.sFWBVNC.ui64BVNC=c.sHWBVNC.ui64BVNC=0x001600280036001eULL;
 observed={};state={};mode=waits=kicks=published=padding=0;
}
int main() {
 reset();uint64_t pa=0;
 /* Actual three-level walk: nonzero PC/PD/PT indexes, multiple PT pages. */
 assert(((uint32_t*)pc)[0x197]==0x800011);
 assert(((uint64_t*)pd)[0]==0x80002001 && ((uint64_t*)pd)[1]==0x80003001);
 assert(!a04_rgx_bif_translate(&bif,bif.va,&pa) && pa==bif.data.pa);
 assert(!a04_rgx_bif_translate(&bif,bif.va+bif.map_bytes-1,&pa) && pa==bif.data.pa+bif.map_bytes-1);
 assert(a04_rgx_bif_translate(&bif,bif.va-1,&pa));
 assert(a04_rgx_bif_translate(&bif,bif.va+bif.map_bytes,&pa));
 assert(r.memory->sPCDevPAddr.uiAddr==bif.pc.pa && r.memory->uiPageCatBaseRegID==0xffffffff);
 assert(r.context->sTAContext.ui32ServerCommonContextID==10 && r.context->s3DContext.ui32ServerCommonContextID==11);
 assert(r.context->sTAContext.eDM==2 && r.context->s3DContext.eDM==3);
 assert(r.gp->eDM==0 && !r.gp->ui32Priority && !r.gp->psRFCmd.ui32Addr && !r.gp->psContextState.ui32Addr);
 assert(r.gp->psFWMemContext.ui32Addr==r.memory_addr.ui32Addr && r.gp->ui32ServerCommonContextID==12);
 assert(!a04_rgx_render_map_cold(&r,table,8192,0x85000000));
 assert(table[0x80]==0x42080017 && table[0x8f] && !table[0x90]);
 assert(a04_rgx_render_map_cold(&r,table,8192,0x85000000));
 for(unsigned n=0;n<600;n++) {
  dm=n%3==0?0:(n%3==1?2:3);assert(!a04_rgx_render_submit_update(&r,&state,&kernel,&io,dm));
 }
 assert(state.sequence==600 && padding>=6 && kicks==600);
 /* Both resource/ring and firmware response errors refuse replay. */
 for(unsigned f=1;f<=7;f++) {
  reset();mode=f;dm=3;
  int result=a04_rgx_render_submit_update(&r,&state,&kernel,&io,3,&observed);
  assert(result==(f<=3?A04_CCB_TIMEOUT:A04_CCB_CORRUPT));assert(state.poisoned);
  assert(observed.valid && observed.dm==3 && observed.expected==1 && observed.kexpected==1 && observed.cexpected==48);
  assert(observed.kread==(f==2?1U:0U) && observed.ufo==(f==3?1U:(f==4?2U:0U)));
  assert(observed.cread==(f==5?4096U:0U) && observed.cwrite==(f==6?4096U:0U) && observed.cdep==(f==7?1U:0U));
  assert(observed.polls==(f<=3?10000U:2U));
  auto saved=observed;
  assert(a04_rgx_render_submit_update(&r,&state,&kernel,&io,3,&observed)==A04_CCB_OFFLINE && kicks==1);
  assert(!memcmp(&saved,&observed,sizeof(saved)));
  if(f<=3)assert(waits==10000);
 }
 reset();dm=0;mode=8;kernel.trace->ui32KCCBCmdsExecuted=3;
 kernel.trace->sTraceBuf[0].ui32TracePointer=27;kernel.trace->ui64StartIdleTime=0x1234567887654321ULL;
 kernel.init->ui32FirmwareStartedTimeStamp=42;
 assert(a04_rgx_render_submit_update(&r,&state,&kernel,&io,0,&observed)==A04_CCB_TIMEOUT);
 assert(observed.valid && observed.dm==0 && observed.cexpected==88 && observed.polls==10000);
 assert(observed.fread==0 && observed.fwrite==1 && observed.ftype==0x2abc0067);
 assert(observed.kexecuted==3 && observed.tracePointer==27 && observed.startedTime==42);
 assert(observed.idleTime==0x1234567887654321ULL && kernel.fctl->ui32ReadOffset==0);
 auto terminal=observed;kernel.kctl->ui32ReadOffset=99;
 assert(a04_rgx_render_submit_update(&r,&state,&kernel,&io,0,&observed)==A04_CCB_OFFLINE);
 assert(!memcmp(&terminal,&observed,sizeof(terminal)) && kicks==1); /* snapshot once, no replay */
 reset();dm=0;
 assert(a04_rgx_render_submit_update(&r,&state,&kernel,&io,0,(a04_rgx_render_observation*)bank)==A04_CCB_INPUT && !kicks);
 reset();kernel.init->bFirmwareStarted=IMG_FALSE;
 assert(a04_rgx_render_submit_update(&r,&state,&kernel,&io,3)==A04_CCB_OFFLINE && !kicks);
 reset();kernel.init->sRGXCompChecks.ui32DDKBuild=1;
 assert(a04_rgx_render_submit_update(&r,&state,&kernel,&io,3)==A04_CCB_OFFLINE && !kicks);
 reset();r.commands[1]=bank;assert(a04_rgx_render_submit_update(&r,&state,&kernel,&io,3)==A04_CCB_INPUT && !kicks);
 reset();r.context=nullptr;assert(a04_rgx_render_submit_update(&r,&state,&kernel,&io,3)==A04_CCB_INPUT && !kicks);
 reset();r.context->s3DContext.psContextState.ui32Addr=0;assert(a04_rgx_render_submit_update(&r,&state,&kernel,&io,3)==A04_CCB_INPUT && !kicks);
 reset();kernel.kctl->ui32WriteOffset=127;assert(a04_rgx_render_submit_update(&r,&state,&kernel,&io,3)==A04_CCB_FULL && !kicks && !state.sequence);
 reset();kernel.fctl->ui32WriteOffset=1;assert(a04_rgx_render_submit_update(&r,&state,&kernel,&io,3)==A04_CCB_FW_REQUEST && !kicks);
 reset();r.control[1]->ui32ReadOffset=8;assert(a04_rgx_render_submit_update(&r,&state,&kernel,&io,3)==A04_CCB_BUSY && !kicks && !state.sequence);
 reset();r.context->s3DContext.psCCB.ui32Addr=0;RGXFWIF_KCCB_CMD cmd={};auto before=cmd;state.sequence=1;
 assert(a04_rgx_render_update(&r,&state,3,&cmd));assert(!memcmp(&cmd,&before,sizeof(cmd)));
 reset();bif.va=0xfffffff000ULL;bif.map_bytes=8192;memset(pc,0xa5,sizeof(pc));
 assert(a04_rgx_bif_build(&bif) && pc[0]==0xa5);
 reset();bif.pd.pa=bif.pc.pa;assert(a04_rgx_bif_build(&bif));
 reset();bif.pt.bytes=4096;assert(a04_rgx_bif_build(&bif));
 reset();bif.data.pa=0xfffff000;assert(a04_rgx_bif_build(&bif));
 reset();bif.va=0x803ffff000ULL;assert(a04_rgx_bif_build(&bif)); /* crosses1GiB */
 reset();bif.va=0xfffffff000ULL;bif.map_bytes=4096;bif.read_only=true;
 assert(!a04_rgx_bif_build(&bif));assert(((uint64_t*)pt)[511]==(bif.data.pa|11));
 assert(!a04_rgx_bif_translate(&bif,0xffffffffffULL,&pa) && pa==bif.data.pa+4095);
 reset();a04_rgx_render_buffer badbank={bank,0x82000000,65535};auto previous=r;
 assert(a04_rgx_render_build(&r,&badbank,&bif,10) && !memcmp(&r,&previous,sizeof(r)));
 assert(a04_rgx_render_submit_update(&r,&state,&kernel,&io,4)==A04_CCB_INPUT && !kicks);
 memset(table,0,sizeof(table));table[0x83]=1;uint32_t old=table[0x80];
 assert(a04_rgx_render_map_cold(&r,table,8192,0x85000000) && table[0x80]==old);
 assert(a04_rgx_render_map_cold(&r,table,8191,0x85000000));
}
'''
        # All CPU resource buffers are genuinely page aligned in this host test.
        source=source.replace('static unsigned char ', 'alignas(4096) static unsigned char ')
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);(p/'test.cpp').write_text(source)
            subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                '-I',str(GPU),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)

    def test_original_ddk_render_rf_and_bif_contract(self):
        # Resolve from the kernel repository, independent of cwd.
        ddk=Path(__file__).resolve().parents[2].parent.parent.parent/'port/gsi/kernel-4.14/drivers/misc/mediatek/gpu/gpu_rgx/m1.11ED5425693'
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory)
            (p/'original.c').write_text('''
#include <stddef.h>
#include "rgx_fwif_km.h"
#include "rgx_fwif_resetframework.h"
#include "rgxmmudefs_km.h"
#include "rgxheapconfig.h"
_Static_assert(RGX_GENERAL_BRN_65273_HEAP_BASE==0x65c0000000ULL,"BRN65273 general VA");
_Static_assert(sizeof(RGXFWIF_RF_CMD)==16,"RF native ABI");
_Static_assert(offsetof(RGXFWIF_RF_CMD,sFWRegisters)==8,"RF registers");
_Static_assert(sizeof(RGXFWIF_CCB_CMD_HEADER)==40,"client header");
_Static_assert(sizeof(RGXFWIF_UFO)==8,"UFO");
_Static_assert(RGXFWIF_CCB_CMD_TYPE_NULL==0x2abc80cf,"NULL native opcode");
_Static_assert(RGXFWIF_CCB_CMD_TYPE_UPDATE==0x2abc00d6,"UPDATE opcode");
_Static_assert(RGXFWIF_CCB_CMD_TYPE_PADDING==0x2abc00dd,"PADDING opcode");
_Static_assert(RGXFWIF_KCCB_CMD_KICK==0x2abc0065,"KICK opcode");
_Static_assert(RGX_FEATURE_NUM_ISP_IPP_PIPES==4,"3D suspend state count");
_Static_assert(RGX_MMUCTRL_VADDR_PC_INDEX_SHIFT==30,"PC index");
_Static_assert(RGX_MMUCTRL_VADDR_PD_INDEX_SHIFT==21,"PD index");
_Static_assert(RGX_MMUCTRL_VADDR_PT_INDEX_SHIFT==12,"PT index");
_Static_assert(RGX_MMUCTRL_ENTRY_SIZE_PC_VALUE==32,"PC entry");
_Static_assert(RGX_MMUCTRL_ENTRY_SIZE_PD_VALUE==64,"PD entry");
_Static_assert(RGX_MMUCTRL_ENTRY_SIZE_PT_VALUE==64,"PT entry");
_Static_assert(RGX_MMUCTRL_PC_DATA_PD_BASE_SHIFT==4,"PC encoding");
_Static_assert(RGX_MMUCTRL_PC_DATA_PD_BASE_ALIGNSHIFT==12,"PC alignment");
_Static_assert(RGX_MMUCTRL_PT_DATA_VALID_EN==1,"PTE valid");
_Static_assert(RGX_MMUCTRL_PT_DATA_SLC_BYPASS_CTRL_EN==8,"uncached PTE");
_Static_assert(RGX_MMUCTRL_PT_DATA_READ_ONLY_EN==2,"RO PTE");
int main(void){return 0;}
''')
            dirs=sorted({f.parent for f in ddk.rglob('*.h')}|{ddk/'hwdefs',ddk/'include/public'})
            subprocess.run(['cc','-std=gnu11','-include',str(ddk/'include/config_kernel_user_mt8167.h'),
                *[v for d in dirs for v in ('-I',str(d))],str(p/'original.c'),'-o',str(p/'abi')],check=True)
            subprocess.run([str(p/'abi')],check=True)

if __name__=='__main__':unittest.main()
