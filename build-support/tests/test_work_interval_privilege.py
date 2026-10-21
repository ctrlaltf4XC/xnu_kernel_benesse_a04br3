# SPDX-License-Identifier: MIT
from pathlib import Path
import subprocess,tempfile,unittest
class WorkIntervalPrivilege(unittest.TestCase):
 def test_scope(self):
  root=Path(__file__).resolve().parents[2];s=(root/'bsd/kern/a04br3_amfi.c').read_text();start=s.index('static int\na04br3_sandbox_priv_grant');body=s[start:s.index('static const struct mac_policy_ops a04br3_sandbox_mac_ops',start)]
  source=r'''
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#define __unused __attribute__((unused))
#define PRIV_WORK_INTERVAL 1008
#define PROC_NULL NULL
typedef void *kauth_cred_t;
struct proc{int platform;};static struct proc process,*current=&process;
static int a04br3_platform_root;
static struct proc *current_proc(void){return current;}
static int csproc_get_platform_binary(struct proc*p){return p->platform;}
static void proc_selfname(char*b,size_t n){snprintf(b,n,"test");}
'''+body+r'''
int main(void){assert(a04br3_sandbox_priv_grant(0,1008)==EPERM);a04br3_platform_root=1;
assert(a04br3_sandbox_priv_grant(0,1008)==EPERM);process.platform=1;
assert(a04br3_sandbox_priv_grant(0,1008)==0);assert(a04br3_sandbox_priv_grant(0,1009)==EPERM);
current=0;assert(a04br3_sandbox_priv_grant(0,1008)==EPERM);return 0;}
'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'test.c').write_text(source);subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
