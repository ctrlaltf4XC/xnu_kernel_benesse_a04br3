# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Execute the verbatim production callback dispatch block with real PSK state.
Callbacks/locks are host simulated; no iOS DHCP or hardware claim.
"""
import unittest
import test_wlan_personal_runtime_regression as review

class LinkCallback(unittest.TestCase):
 setUpClass=classmethod(review.PersonalRuntimeRegression.setUpClass.__func__)
 tearDownClass=classmethod(review.PersonalRuntimeRegression.tearDownClass.__func__)
 run_case=review.PersonalRuntimeRegression.run_case
 def test_actual_pump_order_retry_and_consumer_epoch(self):
  source=review.CPP.read_text()
  start=source.index('  if(changed && client.changed)client.changed(client.context);')
  end=source.index('  if(client.owner)client.owner->release();',start)
  block=source[start:end]
  self.run_case(r'''
struct {a04_wlan_runtime runtime;} storage;auto *live=&storage;
complete_fixture();struct a04_wlan_host h={};completed_runtime(&storage.runtime,&h);
uint64_t nativeEpoch=1,completionConsumer=1,authenticatedGeneration=1;bool liveStopping=false;unsigned changed=1;
bool held=false;unsigned lock=0;auto *transportLock=&lock;
auto IOLockLock=[&](unsigned *){assert(!held);held=true;};
auto IOLockUnlock=[&](unsigned *){assert(held);held=false;};
struct Context{unsigned order=0;bool fail=true,cancel=false;uint64_t *epoch;} context;
context.epoch=&nativeEpoch;
struct Client{void *owner,*context;void(*changed)(void*);int(*authenticated)(void*,uint64_t);} client,nativeClient;
client.owner=&storage;client.context=&context;
client.changed=[](void *p){((Context *)p)->order=1;};
client.authenticated=[](void *p,uint64_t epoch){auto *c=(Context *)p;assert(epoch==1&&c->order==1);c->order=2;if(c->cancel)(*c->epoch)++;return c->fail?EIO:0;};
nativeClient=client;
auto dispatch=[&](){
'''+block+r'''
};
assert(!storage.runtime.personal.completion_reported);dispatch();
assert(context.order==2&&!held&&!storage.runtime.personal.completion_reported); // failed link/auth publication retries
context.fail=false;context.cancel=true;dispatch();
assert(nativeEpoch==2&&!storage.runtime.personal.completion_reported); // same owner, changed consumer epoch
context.cancel=false;completionConsumer=nativeEpoch;dispatch();assert(storage.runtime.personal.completion_reported);
''')

if __name__=='__main__':unittest.main()
