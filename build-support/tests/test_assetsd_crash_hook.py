# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Execute the actual exit-hook block: independent budgets and bounded user reads.

Mach register acquisition and copyin are host boundary fixtures; no live process,
UART, device, permission changes, or assertion about assetsd's abort cause.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]

HARNESS=r'''
#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
struct process {char p_name[64];int pid;};
struct thread {uint32_t uu_exception,uu_code;uint64_t uu_subcode;};
typedef struct process *proc_t;
typedef uintptr_t user_addr_t;
static _Thread_local proc_t selected, running;
static _Thread_local unsigned profile;
static _Thread_local uint64_t frames[40][2];
static unsigned captures[3], reads, printed;
static proc_t current_proc(void){return running;}
static int proc_getpid(proc_t p){return p->pid;}
static const char *strnstr(const char *haystack,const char *needle,size_t count){
 size_t n=strlen(needle);
 for(size_t i=0;i+n<=count&&haystack[i];i++)if(!strncmp(haystack+i,needle,n))return haystack+i;
 return NULL;
}
#define os_atomic_inc_orig(p,order) __atomic_fetch_add((p),1,__ATOMIC_RELAXED)
static uint64_t a04br3_crash_registers(const char *name,int pid,int signal,uint64_t code,uint64_t subcode){
 assert(selected==running&&pid==selected->pid&&signal==SIGABRT);
 assert(code==UINT64_C(0x1123456789)&&subcode==UINT64_C(0xfedcba9876543210));
 unsigned slot=!strcmp(name,"assetsd")?2:!strcmp(name,"mediaserverd")?1:0;
 __atomic_fetch_add(&captures[slot],1,__ATOMIC_RELAXED);
 for(unsigned i=0;i<40;i++){frames[i][0]=(uintptr_t)&frames[i+1<40?i+1:i];frames[i][1]=0xa000+i;}
 if(profile==0)return 0;
 if(profile==2)return (uintptr_t)frames+1;
 if(profile==4)frames[0][0]=(uintptr_t)frames;
 return (uintptr_t)frames;
}
static int copyin(user_addr_t address,void *out,size_t bytes){
 assert(selected==running&&bytes==sizeof(frames[0]));
 assert(address>=(uintptr_t)frames&&address+bytes<=(uintptr_t)(frames+40));
 __atomic_fetch_add(&reads,1,__ATOMIC_RELAXED);
 if(profile==3)return 14;
 memcpy(out,(void*)address,bytes);return 0;
}
static int hook_printf(const char *format,...){
 assert(!strcmp(format,"A04BR3_CRASH %s[%d] bt%u=0x%llx\n"));
 va_list ap;va_start(ap,format);const char *name=va_arg(ap,const char*);
 int pid=va_arg(ap,int);unsigned index=va_arg(ap,unsigned);unsigned long long lr=va_arg(ap,unsigned long long);
 va_end(ap);assert(!strcmp(name,selected->p_name)&&pid==selected->pid&&index<32&&lr==0xa000+index);
 __atomic_fetch_add(&printed,1,__ATOMIC_RELAXED);return 0;
}
#define printf hook_printf
static void hook(proc_t p){
 struct thread values={0x11,0x23456789,UINT64_C(0xfedcba9876543210)};
 struct thread *ut=&values;int rv=SIGABRT;
 (void)p;(void)ut;(void)rv;
HOOK
}
#undef printf
static void invoke(const char *name,unsigned mode,int current){
 struct process p={.pid=42},other={.pid=99};
 assert(strlen(name)<sizeof(p.p_name));strcpy(p.p_name,name);
 selected=&p;running=current?&p:&other;profile=mode;hook(&p);
 selected=running=NULL;
}
static unsigned count(unsigned slot){return __atomic_load_n(&captures[slot],__ATOMIC_RELAXED);}
static void *racer(void *ignored){(void)ignored;invoke("assetsd",0,1);return NULL;}
int main(int argc,char **argv){
 assert(argc==2);unsigned scenario=(unsigned)strtoul(argv[1],NULL,10);
 if(scenario==0){
  /* Non-target and non-exiting-thread invocations cannot spend any budget. */
  for(unsigned i=0;i<24;i++){invoke("assetsd",0,0);invoke("assetsd-helper",0,1);invoke("other",0,1);}
  assert(!count(0)&&!count(1)&&!count(2));
  for(unsigned i=0;i<24;i++)invoke("Setup",0,1);
  assert(count(0)==16&&!count(1)&&!count(2));
  for(unsigned i=0;i<8;i++)invoke("assetsd",0,1);
  assert(count(0)==16&&!count(1)&&count(2)==2);
  for(unsigned i=0;i<8;i++)invoke("mediaserverd",0,1);
  assert(count(0)==16&&count(1)==2&&count(2)==2&&!reads&&!printed);
 }else if(scenario==1){
  /* Reverse ordering also leaves the original shared and media budgets intact. */
  for(unsigned i=0;i<8;i++)invoke("assetsd",0,1);
  for(unsigned i=0;i<8;i++)invoke("mediaserverd",0,1);
  const char *names[]={"Setup","SpringBoard","backboardd","lsd","installd","installcoordinationd","locationd","com.apple.datamigrator"};
  for(unsigned i=0;i<24;i++)invoke(names[i%8],0,1);
  assert(count(0)==16&&count(1)==2&&count(2)==2);
 }else if(scenario==2){
  pthread_t threads[48];
  for(unsigned i=0;i<48;i++)assert(!pthread_create(&threads[i],NULL,racer,NULL));
  for(unsigned i=0;i<48;i++)assert(!pthread_join(threads[i],NULL));
  assert(count(2)==2&&!count(0)&&!count(1));
  for(unsigned i=0;i<17;i++)invoke("Setup",0,1);
  assert(count(0)==16&&count(2)==2);
 }else{
  unsigned mode=scenario-2;invoke("assetsd",mode,1);
  assert(count(2)==1);
  if(mode==1)assert(reads==32&&printed==32); /* strict frame bound */
  else if(mode==2)assert(!reads&&!printed); /* misaligned FP */
  else if(mode==3)assert(reads==1&&!printed); /* copyin fault */
  else if(mode==4)assert(reads==1&&printed==1); /* cyclic/backward chain */
  else assert(0);
 }
 puts("PASS actual hook: budgets/exiting-thread/atomic race/bounded stack");return 0;
}
'''

class AssetsdHook(unittest.TestCase):
    def test_actual_hook_independent_budget_and_fault_paths(self):
        source=(ROOT/'bsd/kern/kern_exit.c').read_text()
        start=source.index('#if defined(A04BR3)\n        /* Capture the actual exiting thread')
        block=source[start:source.index('#endif',start)+len('#endif')]
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);unit=p/'hook.c';unit.write_text(HARNESS.replace('HOOK',block))
            exe=p/'hook'
            subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-pthread',
                            '-fsanitize=address,undefined','-DA04BR3=1',str(unit),'-o',str(exe)],
                           check=True,capture_output=True)
            for scenario in range(7):
                with self.subTest(scenario=scenario):
                    subprocess.run([str(exe),str(scenario)],check=True,capture_output=True)

if __name__=='__main__':unittest.main()
