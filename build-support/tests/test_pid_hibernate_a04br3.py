# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Actual A04BR3 OSX pid_hibernate body: routing, permissions and VM dispatch.
VM sweep is a counted HOST SIMULATION, not real compression or device evidence.
Compiles actual production body with C/C++ and ARM64 declaration-only shims.
Requires Python stdlib and clang/clang++; no SDK, external source or packages.
"""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/'bsd/vm/vm_unix.c'
MASTER=ROOT/'bsd/kern/syscalls.master'

DECL=r'''
typedef int kern_return_t;
struct proc {int unused;};
struct pid_hibernate_args {int pid;};
#define __unused __attribute__((unused))
#define PROC_NULL ((struct proc *)0)
#define MAC_PROC_CHECK_HIBERNATE 2 /* security/mac.h */
extern int test_root,test_mac_error,test_compressor;
extern unsigned test_mac_calls,test_vm_calls;
extern int kauth_cred_get(void);
extern int kauth_cred_issuser(int);
extern int mac_proc_check_suspend_resume(struct proc *,int);
extern void vm_pageout_anonymous_pages(void);
#define VM_CONFIG_COMPRESSOR_IS_PRESENT test_compressor
'''
HOST=r'''
#include <assert.h>
#include <limits.h>
int test_root=1,test_mac_error=0,test_compressor=1;
unsigned test_mac_calls,test_vm_calls;
int kauth_cred_get(void){return 123;}
int kauth_cred_issuser(int cred){assert(cred==123);return test_root;}
int mac_proc_check_suspend_resume(struct proc *p,int action){
 assert(p==PROC_NULL&&action==MAC_PROC_CHECK_HIBERNATE);test_mac_calls++;return test_mac_error;
}
void vm_pageout_anonymous_pages(void){test_vm_calls++;}
static void check(int pid,int root,int mac,int compressor,int expected,unsigned expected_mac,unsigned expected_vm){
 test_root=root;test_mac_error=mac;test_compressor=compressor;
 test_mac_calls=test_vm_calls=0;
 struct pid_hibernate_args args={pid};int retval=-777;
 assert(pid_hibernate(PROC_NULL,&args,&retval)==expected);
 assert(retval==expected && test_mac_calls==expected_mac && test_vm_calls==expected_vm);
}
int main(void){
 #if CONFIG_MACF
 const unsigned mac_expected=1;
 #else
 const unsigned mac_expected=0;
 #endif
 check(-2,1,0,1,0,mac_expected,1);
 check(-2,1,0,0,ENOTSUP,mac_expected,0);
 check(-2,0,0,1,EPERM,0,0);
 check(-2,0,0,0,EPERM,0,0);
 const int unsupported[]={-1,0,1,123,INT_MIN,INT_MAX,-3};
 for(unsigned i=0;i<sizeof(unsupported)/sizeof(unsupported[0]);i++){
  check(unsupported[i],1,0,1,ENOTSUP,0,0);
  check(unsupported[i],0,0,1,EPERM,0,0);
 }
 #if CONFIG_MACF
 check(-2,1,EACCES,1,EPERM,1,0);
 check(-2,1,EPERM,0,EPERM,1,0);
 #endif
 return 0;
}
'''

class PIDHibernate(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source=SOURCE.read_text()
        start=source.index('#elif defined(ARM64_BOARD_CONFIG_A04BR3)',source.index('pid_hibernate(struct proc'))
        end=source.index('#endif /* !XNU_TARGET_OS_OSX / ARM64_BOARD_CONFIG_A04BR3 */',start)
        cls.body=source[start:end].split('\n',1)[1]
        cls.routing=re.search(r'#if CONFIG_EMBEDDED[^\n]*\n435\b.*?#endif',MASTER.read_text(),re.S).group()

    def test_actual_handler_mac_permissions_and_dispatch(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);src=p/'handler.c';src.write_text('#include <errno.h>\n'+DECL+self.body+HOST)
            for macf in (0,1):
                exe=p/('mac'+str(macf))
                subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror',
                                '-DCONFIG_MACF='+str(macf),'-fsanitize=address,undefined',
                                str(src),'-o',str(exe)],check=True)
                subprocess.run([str(exe)],check=True)

    def test_actual_435_routing_other_targets_unchanged(self):
        for embedded,board,expected in ((0,False,'nosys'),(0,True,'pid_hibernate'),(1,False,'pid_hibernate'),(1,True,'pid_hibernate')):
            flags=['-DCONFIG_EMBEDDED='+str(embedded)]
            if board:flags.append('-DARM64_BOARD_CONFIG_A04BR3')
            result=subprocess.run(['clang','-E','-P','-x','c',*flags,'-'],input=self.routing,
                                  text=True,stdout=subprocess.PIPE,check=True).stdout
            self.assertIn(expected+'(',result)
            self.assertEqual(result.count('435'),1)

    def test_real_syscall_generator_emits_handler_and_args(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)
            for mode in ('proto','table'):
                subprocess.run(['sh',str(ROOT/'bsd/kern/makesyscalls.sh'),str(MASTER),mode],
                               cwd=p,check=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            proto=(p/'sysproto.h').read_text()
            table=(p/'init_sysent.c').read_text()
            self.assertIn('struct pid_hibernate_args {',proto)
            self.assertIn('/* 435 = pid_hibernate */',table)
            self.assertIn('#if CONFIG_EMBEDDED || defined(ARM64_BOARD_CONFIG_A04BR3)',proto)
            self.assertIn('#if CONFIG_EMBEDDED || defined(ARM64_BOARD_CONFIG_A04BR3)',table)

    def test_arm64_c_cpp_actual_handler_declarations(self):
        # Actual Darwin errno values from the repository, not host numeric ABI.
        errors=(ROOT/'bsd/sys/errno.h').read_text()
        defines=''
        for name in ('EPERM','ENOTSUP'):
            match=re.search(r'^#define\s+'+name+r'\s+([^\n/]+)',errors,re.M)
            self.assertIsNotNone(match)
            defines+='#define '+name+' '+match.group(1)+'\n'
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);src=p/'handler.c';src.write_text(defines+DECL+self.body)
            for macf in (0,1):
                for language,compiler in (('c','clang'),('c++','clang++')):
                    subprocess.run([compiler,'--target=arm64-apple-ios16.7','-ffreestanding','-O2',
                                    '-Wall','-Wextra','-Werror','-x',language,'-DCONFIG_MACF='+str(macf),
                                    '-c',str(src),'-o',str(p/'out.o')],check=True)

if __name__=='__main__':unittest.main()
