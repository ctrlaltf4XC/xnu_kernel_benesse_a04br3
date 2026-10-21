# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Actual state header, independent Python vectors and real OpenSSL test adapter.
Requires C/C++ compiler, Clang ARM64 codegen, OpenSSL CLI/development library,
and Python standard library. Synthetic public fixtures only; firmware events are explicit simulated inputs,
not claims that real firmware ACKed. Production crypto belongs to its adapter.
"""
import hashlib,hmac,subprocess,tempfile,unittest,shutil
from pathlib import Path
# Use system OpenSSL wrap support, not an undeclared Python crypto package.
def aes_key_wrap(key,plain):
 openssl=shutil.which('openssl',path='/usr/bin:/bin') or shutil.which('openssl')
 if not openssl:raise RuntimeError('OpenSSL CLI with RFC3394 wrap support required')
 return subprocess.run([openssl,'enc','-id-aes128-wrap','-K',key.hex(),'-iv','a6'*8],input=plain,stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=True).stdout
import test_wlan_backend as backend
import test_wmt_protocol as codecs
HEADER=codecs.INCLUDE/'A04BR3WLANPSK4Way.h'
RSN=bytes.fromhex('30140100000fac040100000fac040100000fac020000')
AP=bytes([2,6,7,8,9,10]);STA=bytes([2,1,2,3,4,5]);AN=bytes([0x11])*32;SN=bytes([0x22])*32
PMK=hashlib.pbkdf2_hmac('sha1',b'password',b'test',4096,32)
data=min(AP,STA)+max(AP,STA)+min(AN,SN)+max(AN,SN)
PTK=b''.join(hmac.new(PMK,b'Pairwise key expansion\0'+data+bytes([i]),hashlib.sha1).digest() for i in range(4))[:64]
GTK=bytes(range(16))
PLAIN=RSN+bytes.fromhex('dd16000fac010100')+GTK+b'\xdd\0'
assert len(PLAIN)==48

def packet(kind,data=b'',replay=None):
 p=bytearray(99+len(data));p[:2]=bytes([2,3]);p[2:4]=(len(p)-4).to_bytes(2,'big');p[4]=2
 p[5:7]={1:0x008a,2:0x010a,3:0x13ca,4:0x030a}[kind].to_bytes(2,'big');p[8]=16 if kind in (1,3) else 0
 p[9:17]=(replay if replay is not None else 1 if kind in (1,2) else 2).to_bytes(8,'big')
 if kind!=4:p[17:49]=SN if kind==2 else AN
 p[97:99]=len(data).to_bytes(2,'big');p[99:]=data
 if kind!=1:p[81:97]=hmac.new(PTK[:16],p,hashlib.sha1).digest()[:16]
 return bytes(p)

def arr(name,b):return 'static const uint8_t '+name+'[]={'+','.join(map(str,b))+'};\n'
VECTORS=''.join(arr(n,b) for n,b in [('pmk_ref',PMK),('ptk_ref',PTK),('gtk_ref',GTK),('one',packet(1)),('two',packet(2,RSN)),('three',packet(3,aes_key_wrap(PTK[16:32],PLAIN))),('four',packet(4))])
# Authenticated but rejected keydata: duplicates, modified RSN, absent GTK,
# unknown KDE, RFC3394 integrity failure, descriptor/key-info modifications.
for name,plain in [('dup',PLAIN[:-2]+PLAIN[22:-2]),('rsn_bad',bytes([49])+PLAIN[1:]),('no_gtk',RSN+b'\xdd\0'),('unknown',RSN+bytes.fromhex('dd16000fac090100')+GTK+b'\xdd\0')]:
 plain+=b'\xdd'+b'\0'*((7-len(plain))%8) if len(plain)%8 else b''
 VECTORS+=arr(name,packet(3,aes_key_wrap(PTK[16:32],plain)))
corrupt=bytearray(aes_key_wrap(PTK[16:32],PLAIN));corrupt[-1]^=1
VECTORS+=arr('unwrap_bad',packet(3,corrupt))
ADAPTER=r'''
#include <openssl/evp.h>
#include <openssl/hmac.h>
static unsigned random_calls,derive_calls;static int random_bad;
static int host_pmk(void *c,const uint8_t *pass,unsigned n,const uint8_t *ssid,unsigned sn,uint8_t out[32]){
 (void)c;return PKCS5_PBKDF2_HMAC((const char *)pass,(int)n,ssid,(int)sn,4096,EVP_sha1(),32,out)==1?0:91;
}
static int host_random(void *c,uint8_t out[32]){(void)c;random_calls++;memset(out,0x22,32);return random_bad;}
static int host_ptk(void *c,const uint8_t pmk[32],const uint8_t ap[6],const uint8_t sta[6],const uint8_t an[32],const uint8_t sn[32],uint8_t out[64]){
 (void)c;derive_calls++;uint8_t input[100],digest[20];unsigned size;
 const char *label="Pairwise key expansion";unsigned k=(unsigned)strlen(label)+1;memcpy(input,label,k);
 const uint8_t *lo=memcmp(ap,sta,6)<0?ap:sta,*hi=lo==ap?sta:ap;memcpy(input+k,lo,6);memcpy(input+k+6,hi,6);k+=12;
 lo=memcmp(an,sn,32)<0?an:sn;hi=lo==an?sn:an;memcpy(input+k,lo,32);memcpy(input+k+32,hi,32);k+=64;
 unsigned pos=0;for(unsigned i=0;i<4;i++){input[k]=(uint8_t)i;assert(HMAC(EVP_sha1(),pmk,32,input,k+1,digest,&size)&&size==20);
 unsigned copy=64-pos<20?64-pos:20;memcpy(out+pos,digest,copy);pos+=copy;}return 0;
}
static int host_mic(void *c,const uint8_t key[16],const uint8_t *p,unsigned n,uint8_t out[16]){
 (void)c;uint8_t digest[20];unsigned size;assert(HMAC(EVP_sha1(),key,16,p,n,digest,&size)&&size==20);memcpy(out,digest,16);return 0;
}
static int host_unwrap(void *c,const uint8_t key[16],const uint8_t *p,unsigned n,uint8_t *out,unsigned cap,unsigned *bytes){
 (void)c;assert(n>=24&&cap>=n-8);EVP_CIPHER_CTX *ctx=EVP_CIPHER_CTX_new();assert(ctx);int size=0,tail=0;
 EVP_CIPHER_CTX_set_flags(ctx,EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);
 int ok=EVP_DecryptInit_ex(ctx,EVP_aes_128_wrap(),0,key,0)==1&&EVP_DecryptUpdate(ctx,out,&size,p,(int)n)==1&&EVP_DecryptFinal_ex(ctx,out+size,&tail)==1;
 EVP_CIPHER_CTX_free(ctx);if(!ok)return A04_WLAN_SECURITY;*bytes=(unsigned)(size+tail);return 0;
}
static struct a04_psk_crypto crypto={0,host_pmk,host_random,host_ptk,host_mic,host_unwrap};
static void start(struct a04_psk4way *s,uint64_t generation){
 struct a04_wlan_station sta;memset(&sta,0,sizeof(sta));local_profile(&sta.local);ap_profile(&sta.ap);
 memcpy(sta.ap.rsn,rsn_ref,sizeof(rsn_ref));sta.ap.rsn_length=sizeof(rsn_ref);sta.secured=sta.sta_active=1;sta.join.phase=4;
 struct a04_psk_prepared prepared;memset(&prepared,0,sizeof(prepared));
 assert(!a04_psk_prepare(&prepared,&crypto,(const uint8_t *)"password",8,1,sta.ap.ssid,sta.ap.ssid_length));
 assert(prepared.ready&&!memcmp(prepared.pmk,pmk_ref,32));
 memset(s,0,sizeof(*s));assert(!a04_psk_begin_prepared(s,&sta,&crypto,&prepared,generation,100,1000100));
 assert(!prepared.ready&&a04_psk_zero(prepared.pmk,32));assert(!memcmp(s->pmk,pmk_ref,32));
}
static uint64_t take_tx(struct a04_psk4way *s,unsigned kind){const uint8_t *p;unsigned n;uint64_t cookie;
 assert(!a04_psk_tx_take(s,200,&p,&n,&cookie));assert(n==(kind==2?sizeof(two):sizeof(four)));assert(!memcmp(p,kind==2?two:four,n));return cookie;}
static void to_m3(struct a04_psk4way *s){start(s,1);assert(!a04_psk_rx(s,one,sizeof(one),150));assert(!memcmp(s->ptk,ptk_ref,64));
 uint64_t cookie=take_tx(s,2);assert(!a04_psk_tx_done(s,cookie,0,220));assert(s->phase==A04_PSK_WAIT_M3);}
static void resign(uint8_t *p,unsigned n){memset(p+81,0,16);assert(!host_mic(0,ptk_ref,p,n,p+81));}
'''
class PersonalState(unittest.TestCase):
 production=False
 def run_c(self,body):
  source='#include <stdint.h>\n#include <assert.h>\n#include <string.h>\n'+backend.SETUP+'\n#include "'+str(HEADER)+'"\n'+arr('rsn_ref',RSN)+VECTORS+ADAPTER+body
  extra=[];flags=[]
  if self.production:
   import test_wlan_personal_crypto as provider
   source=source.replace('#include "'+str(HEADER)+'"', '#include "'+str(HEADER)+'"\n#include "'+str(provider.ROOT/'iokit/Kernel/arm/sdio/A04BR3WLANPersonalCrypto.h')+'"')
   wrappers=r'''
static int real_pmk(void *c,const uint8_t *p,unsigned n,const uint8_t *ssid,unsigned sn,uint8_t out[32]){(void)c;return a04_wlan_personal_pmk(p,n,ssid,sn,out);}
static int real_ptk(void *c,const uint8_t p[32],const uint8_t a[6],const uint8_t s[6],const uint8_t an[32],const uint8_t sn[32],uint8_t out[64]){(void)c;return a04_wlan_personal_ptk(p,a,s,an,sn,out);}
static int real_mic(void *c,const uint8_t k[16],const uint8_t *p,unsigned n,uint8_t out[16]){(void)c;return a04_wlan_personal_mic(k,p,n,out);}
static int real_unwrap(void *c,const uint8_t k[16],const uint8_t *p,unsigned n,uint8_t *out,unsigned cap,unsigned *len){(void)c;return a04_wlan_personal_unwrap(k,p,n,out,cap,len);}
'''
   source=source.replace('int main(void){',wrappers+'\nint main(void){crypto.pmk=real_pmk;crypto.ptk=real_ptk;crypto.mic=real_mic;crypto.unwrap=real_unwrap;')
   extra=[str(provider.BEAR/'src'/p) for p in provider.SOURCES]
   flags=['-I'+str(provider.BEAR/'inc'),'-I'+str(provider.BEAR/'src')]
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'case.c').write_text(source)
   subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-const-variable','-fsanitize=address,undefined','-I',str(codecs.INCLUDE),*flags,str(p/'case.c'),*extra,'-lcrypto','-o',str(p/'case')],check=True)
   subprocess.run([str(p/'case')],check=True)
   subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-const-variable','-I',str(codecs.INCLUDE),'-x','c++','-fsyntax-only',*flags,str(p/'case.c')],check=True)
 def test_admission_randomness_and_group_ack_failure(self):
  self.run_c(r'''
int main(void){struct a04_psk4way s;to_m3(&s);assert(!a04_psk_rx(&s,three,sizeof(three),250));
 const uint8_t *key,*rsc;unsigned id;uint64_t c;
 assert(!a04_psk_key_take(&s,1,300,&key,&id,&rsc,&c));assert(!a04_psk_key_done(&s,1,c,0,310));
 assert(!a04_psk_key_take(&s,0,320,&key,&id,&rsc,&c));assert(a04_psk_key_done(&s,0,c,1,330));
 assert(!a04_psk_authenticated(&s)&&s.phase==A04_PSK_FAILED&&a04_psk_zero(s.gtk,16));
 start(&s,9);uint8_t bad[sizeof(one)];memcpy(bad,one,sizeof(bad));memset(bad+17,0,32);assert(a04_psk_rx(&s,bad,sizeof(bad),150));
 start(&s,10);assert(a04_psk_live(&s,s.deadline)==A04_WLAN_TIMEOUT&&a04_psk_zero(s.pmk,32));
 struct a04_wlan_station sta;memset(&sta,0,sizeof(sta));local_profile(&sta.local);ap_profile(&sta.ap);
 memcpy(sta.ap.rsn,rsn_ref,sizeof(rsn_ref));sta.ap.rsn_length=sizeof(rsn_ref);sta.secured=sta.sta_active=1;sta.join.phase=4;
 struct a04_psk_crypto saved=crypto;memset(&s,0,sizeof(s));crypto.mic=0;
 assert(a04_psk_begin(&s,&sta,&crypto,pmk_ref,32,0,1,100,1000));assert(!s.phase);crypto=saved;
 assert(a04_psk_begin(&s,&sta,&crypto,pmk_ref,32,0,0,100,1000));
 sta.sta_active=0;assert(a04_psk_begin(&s,&sta,&crypto,pmk_ref,32,0,1,100,1000));sta.sta_active=1;
 assert(!a04_psk_begin(&s,&sta,&crypto,pmk_ref,32,0,1,100,1000));assert(!memcmp(s.pmk,pmk_ref,32));
 assert(a04_psk_begin(&s,&sta,&crypto,pmk_ref,32,0,2,100,1000));return 0;}
''')
 def test_preassociation_prepared_binding_and_one_shot_consumption(self):
  self.run_c(r'''
static unsigned deriving,associated_now;static int fail_derivation;
static int outside_assoc_pmk(void *ctx,const uint8_t *p,unsigned n,const uint8_t *ssid,unsigned sn,uint8_t out[32]){
 assert(!associated_now);deriving++;if(fail_derivation)return 91;return host_pmk(ctx,p,n,ssid,sn,out);
}
int main(void){struct a04_psk_prepared ready;memset(&ready,0,sizeof(ready));
 struct a04_psk_crypto ops=crypto;ops.pmk=outside_assoc_pmk;
 assert(!a04_psk_prepare(&ready,&ops,(const uint8_t *)"password",8,1,(const uint8_t *)"test",4));assert(deriving==1);
 assert(a04_psk_prepare(&ready,&ops,(const uint8_t *)"password",8,1,(const uint8_t *)"test",4)); /* no overwrite */
 struct a04_wlan_station sta;memset(&sta,0,sizeof(sta));local_profile(&sta.local);ap_profile(&sta.ap);
 memcpy(sta.ap.rsn,rsn_ref,sizeof(rsn_ref));sta.ap.rsn_length=sizeof(rsn_ref);sta.secured=sta.sta_active=1;sta.join.phase=4;associated_now=1;
 struct a04_psk4way s;memset(&s,0,sizeof(s));
 assert(a04_psk_begin(&s,&sta,&ops,(const uint8_t *)"password",8,1,1,100,1000));assert(deriving==1&&!s.phase);
 assert(!a04_psk_begin_prepared(&s,&sta,&ops,&ready,1,100,1000));assert(deriving==1&&a04_psk_zero((const uint8_t *)&ready,sizeof(ready)));
 assert(a04_psk_begin_prepared(&s,&sta,&ops,&ready,1,100,1000));
 associated_now=0;assert(!a04_psk_prepare(&ready,&ops,pmk_ref,32,0,(const uint8_t *)"test",4));
 memset(&s,0,sizeof(s));sta.ap.ssid[0]='x';assert(a04_psk_begin_prepared(&s,&sta,&ops,&ready,2,100,1000)==A04_WLAN_SECURITY);
 assert(!s.phase&&a04_psk_zero((const uint8_t *)&ready,sizeof(ready)));sta.ap.ssid[0]='t';
 assert(!a04_psk_prepare(&ready,&ops,pmk_ref,32,0,(const uint8_t *)"test",4));sta.sta_active=0;
 assert(a04_psk_begin_prepared(&s,&sta,&ops,&ready,3,100,1000));assert(a04_psk_zero((const uint8_t *)&ready,sizeof(ready)));
 fail_derivation=1;assert(a04_psk_prepare(&ready,&ops,(const uint8_t *)"password",8,1,(const uint8_t *)"test",4)==91);assert(!ready.ready&&a04_psk_zero(ready.pmk,32));
 return 0;}
''')
 def test_arm64_actual_header_codegen(self):
  source='#include "'+str(HEADER)+'"\n'+r'''
int start(struct a04_psk4way *s,const struct a04_wlan_station *a,const struct a04_psk_crypto *c,const uint8_t *p,unsigned n){return a04_psk_begin(s,a,c,p,n,0,1,1,1000);}
int rx(struct a04_psk4way *s,const uint8_t *p,unsigned n){return a04_psk_rx(s,p,n,10);}
int tx(struct a04_psk4way *s,const uint8_t **p,unsigned *n,uint64_t *cookie){return a04_psk_tx_take(s,20,p,n,cookie);}
int done(struct a04_psk4way *s,uint64_t c,unsigned e){return a04_psk_tx_done(s,c,e,30);}
int key(struct a04_psk4way *s,unsigned pair,const uint8_t **k,unsigned *id,const uint8_t **r,uint64_t *c){return a04_psk_key_take(s,pair,40,k,id,r,c);}
int ack(struct a04_psk4way *s,unsigned pair,uint64_t c,unsigned e){return a04_psk_key_done(s,pair,c,e,50);}
int complete(struct a04_psk4way *s){return a04_psk_completion_take(s);}
'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'arm.c').write_text(source)
   for target in ('aarch64-none-elf','arm64-apple-ios16.7'):
    subprocess.run(['clang','--target='+target,'-ffreestanding','-O2','-std=c11','-Wall','-Wextra','-Werror','-c',str(p/'arm.c'),'-o',str(p/'out.o')],check=True)
   subprocess.run(['clang++','--target=arm64-apple-ios16.7','-ffreestanding','-O2','-std=c++14','-Wall','-Wextra','-Werror','-x','c++','-c',str(p/'arm.c'),'-o',str(p/'cpp.o')],check=True)
 def test_kernel_binding_actual_api_arm64_codegen(self):
  import re
  import test_wlan_personal_crypto as provider
  binding=HEADER.parent/'A04BR3WLANPSKBinding.h'
  # Declaration-only shim copied from actual XNU sys/random.h. No mock RNG
  # implementation is linked or run: this is ABI/codegen, NOT entropy proof.
  actual=(provider.ROOT/'bsd/sys/random.h').read_text()
  declaration=re.search(r'void read_random\(void\* buffer, u_int numBytes\);',actual)
  self.assertIsNotNone(declaration)
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'sys').mkdir()
   (p/'sys/types.h').write_text('typedef unsigned int u_int;\n')
   (p/'sys/random.h').write_text('#ifdef __cplusplus\nextern "C" {\n#endif\n'+declaration[0]+'\n#ifdef __cplusplus\n}\n#endif\n')
   (p/'string.h').write_text('#include <stddef.h>\n#ifdef __cplusplus\nextern "C" {\n#endif\nvoid *memcpy(void *,const void *,size_t);\nvoid *memset(void *,int,size_t);\nvoid *memmove(void *,const void *,size_t);\nint memcmp(const void *,const void *,size_t);\n#ifdef __cplusplus\n}\n#endif\n')
   (p/'binding.c').write_text('#define KERNEL 1\n#define ARM64_BOARD_CONFIG_A04BR3 1\n#include "'+str(binding)+'"\nstruct a04_psk_crypto real_ops(void){return a04_psk_native_crypto();}\n')
   flags=['--target=arm64-apple-ios16.7','-ffreestanding','-O2','-Wall','-Wextra','-Werror','-I'+str(p),'-I'+str(provider.BEAR/'inc')]
   for cc,lang,std in [('clang','c','c11'),('clang++','c++','c++14')]:
    subprocess.run([cc,*flags,'-std='+std,'-x',lang,'-c',str(p/'binding.c'),'-o',str(p/'binding.o')],check=True)
   # LLVM-produced Mach-O symbol inspection needs an actual Mach-O-capable nm;
   # codegen above is mandatory regardless of host binutils format support.
 def test_production_crypto_state_integration(self):
  self.production=True
  try:
   self.test_actual_mic_unwrap_wire_and_correlated_completions()
   self.test_replay_nonce_mic_descriptor_unwrap_kde_timeout_fail_closed()
  finally:self.production=False
 def test_actual_mic_unwrap_wire_and_correlated_completions(self):
  self.run_c(r'''
int main(void){struct a04_psk4way s;to_m3(&s);assert(!a04_psk_rx(&s,three,sizeof(three),250));
 assert(!memcmp(s.gtk,gtk_ref,16)&&s.gtk_id==1&&!a04_psk_authenticated(&s));
 const uint8_t *key,*rsc,*p;unsigned id,n;uint64_t cookie,tx;
 assert(a04_psk_tx_take(&s,300,&p,&n,&tx));
 assert(a04_psk_key_take(&s,0,300,&key,&id,&rsc,&cookie));
 assert(!a04_psk_key_take(&s,1,300,&key,&id,&rsc,&cookie)&&id==0&&!memcmp(key,ptk_ref+32,16));
 assert(a04_psk_key_done(&s,1,cookie+1,0,310)&&!a04_psk_authenticated(&s));
 assert(a04_psk_key_take(&s,1,310,&key,&id,&rsc,&tx));
 assert(!a04_psk_key_done(&s,1,cookie,0,320));
 assert(!a04_psk_key_take(&s,0,330,&key,&id,&rsc,&cookie)&&id==1&&!memcmp(key,gtk_ref,16));
 assert(a04_psk_tx_take(&s,340,&p,&n,&tx)); /* FIFO queue cannot open M4 */
 assert(!a04_psk_key_done(&s,0,cookie,0,350));tx=take_tx(&s,4);
 assert(!a04_psk_authenticated(&s)&&a04_psk_completion_take(&s));assert(a04_psk_tx_done(&s,tx+1,0,360));assert(!a04_psk_tx_done(&s,tx,0,370));assert(a04_psk_authenticated(&s));assert(!a04_psk_completion_take(&s));assert(a04_psk_completion_take(&s));
 assert(!a04_psk_rx(&s,three,sizeof(three),380));assert(a04_psk_key_take(&s,1,390,&key,&id,&rsc,&cookie));assert(a04_psk_key_take(&s,0,390,&key,&id,&rsc,&cookie));
 tx=take_tx(&s,4);assert(!a04_psk_tx_done(&s,tx,0,400)&&a04_psk_authenticated(&s)); /* cached M4 only, NO reinstall */
 uint8_t retry[sizeof(three)];memcpy(retry,three,sizeof(retry));retry[16]++;resign(retry,sizeof(retry));
 assert(!a04_psk_rx(&s,retry,sizeof(retry),410));assert(s.ptk_state==2&&s.gtk_state==2);
 assert(a04_psk_key_take(&s,1,420,&key,&id,&rsc,&cookie));assert(a04_psk_key_take(&s,0,420,&key,&id,&rsc,&cookie));
 assert(!a04_psk_tx_take(&s,430,&p,&n,&tx));assert(p[16]==3);assert(!a04_psk_verify(&s,p,n,81,16));
 assert(!a04_psk_tx_done(&s,tx,0,440)&&a04_psk_authenticated(&s));assert(a04_psk_completion_take(&s));
 return 0;}
''')
 def test_replay_nonce_mic_descriptor_unwrap_kde_timeout_fail_closed(self):
  self.run_c(r'''
int main(void){
 const uint8_t *bad[]={dup,rsn_bad,no_gtk,unknown,unwrap_bad};unsigned lengths[]={sizeof(dup),sizeof(rsn_bad),sizeof(no_gtk),sizeof(unknown),sizeof(unwrap_bad)};
 for(unsigned i=0;i<5;i++){struct a04_psk4way s;to_m3(&s);assert(a04_psk_rx(&s,bad[i],lengths[i],250));assert(!a04_psk_authenticated(&s)&&s.phase==A04_PSK_FAILED&&a04_psk_zero(s.ptk,64));}
 for(unsigned i=0;i<7;i++){struct a04_psk4way s;to_m3(&s);uint8_t b[sizeof(three)];memcpy(b,three,sizeof(b));
  if(i==0)b[81]^=1;
 if(i==1)b[16]=1;
 if(i==2)b[17]^=1;
 if(i==3)b[6]=0xc9;
 if(i==4)b[71]=1;
 if(i==5)b[98]++;
 if(i==6)b[4]=254;
 assert(a04_psk_rx(&s,b,sizeof(b),250)&&!a04_psk_authenticated(&s));}
 struct a04_psk4way s;to_m3(&s);assert(a04_psk_rx(&s,three,sizeof(three),s.deadline)==A04_WLAN_TIMEOUT);
 for(unsigned n=0;n<sizeof(three);n++){to_m3(&s);assert(a04_psk_rx(&s,three,n,250));}
 to_m3(&s);assert(!a04_psk_rx(&s,three,sizeof(three),250));uint8_t changed[sizeof(three)];memcpy(changed,three,sizeof(changed));changed[16]++;resign(changed,sizeof(changed));assert(!a04_psk_rx(&s,changed,sizeof(changed),260));assert(s.phase==A04_PSK_KEYS&&!s.ptk_state&&!s.gtk_state);
 changed[16]--;assert(a04_psk_rx(&s,changed,sizeof(changed),270)==A04_WLAN_REPLAY);
 return 0;}
''')
 def test_duplicate_m1_bounded_retry_failures_and_generation(self):
  self.run_c(r'''
int main(void){struct a04_psk4way s;start(&s,2);unsigned before=random_calls;
 assert(!a04_psk_rx(&s,one,sizeof(one),150));unsigned derived=derive_calls;
 uint64_t old=take_tx(&s,2);assert(!a04_psk_rx(&s,one,sizeof(one),160));assert(s.tx_pending&&random_calls==before+1&&derive_calls==derived);
 assert(!a04_psk_tx_done(&s,old,0,210));
 for(unsigned i=0;i<3;i++){assert(!a04_psk_rx(&s,one,sizeof(one),220));uint64_t c=take_tx(&s,2);assert(!a04_psk_tx_done(&s,c,0,230));}
 assert(!a04_psk_rx(&s,one,sizeof(one),240));const uint8_t *p;unsigned n;uint64_t c;assert(a04_psk_tx_take(&s,250,&p,&n,&c)==A04_WLAN_TIMEOUT);
 start(&s,3);assert(!a04_psk_rx(&s,one,sizeof(one),150));c=take_tx(&s,2);assert(c!=old&&a04_psk_tx_done(&s,old,0,210));assert(a04_psk_tx_done(&s,c,1,220)&&s.phase==A04_PSK_FAILED);
 start(&s,4);random_bad=91;assert(a04_psk_rx(&s,one,sizeof(one),150));assert(a04_psk_zero(s.pmk,32)&&a04_psk_zero(s.ptk,64));random_bad=0;
 to_m3(&s);assert(!a04_psk_rx(&s,three,sizeof(three),250));const uint8_t *key,*rsc;unsigned id;assert(!a04_psk_key_take(&s,1,300,&key,&id,&rsc,&c));assert(a04_psk_key_done(&s,1,c,1,310)&&!a04_psk_authenticated(&s));
 return 0;}
''')
if __name__=='__main__':unittest.main()
