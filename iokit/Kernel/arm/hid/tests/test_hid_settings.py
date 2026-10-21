# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Execute the production settings helper and complete provider entry point."""
from pathlib import Path
import subprocess
import tempfile
import unittest

HID = Path(__file__).resolve().parents[1]

class HIDSettingsTests(unittest.TestCase):
    def test_persistence_atomic_failures_and_identity_separation(self):
        source = (HID / 'A04BR3HID.cpp').read_text()
        start = source.index('IOReturn\nIOHIDEventService::setProperties(')
        end = source.index('\nUInt32\nIOHIDEventService::getPrimaryUsagePage()', start)
        function = source[start:end]
        harness = r'''
#include <map>
#include <string>
#include <cassert>
#include <cstdio>
static int alive,failCopy,failMerge;
struct OSObject {int refs=1;OSObject(){++alive;}virtual ~OSObject(){--alive;}void retain(){++refs;}void release(){assert(refs>0);if(--refs==0)delete this;}};
struct OSDictionary:OSObject {
 std::map<std::string,int> values;
 static OSDictionary *withDictionary(const OSDictionary *p){if(failCopy)return nullptr;auto *n=new OSDictionary;n->values=p->values;return n;}
 bool merge(const OSDictionary *p){if(failMerge){values["partial"]=-1;return false;}for(auto &v:p->values)values[v.first]=v.second;return true;}
 void removeObject(const char *s){values.erase(s);}
};
#define OSDynamicCast(t,p) dynamic_cast<t*>(p)
#define OSSafeReleaseNULL(p) do{if(p){(p)->release();p=nullptr;}}while(0)
using IOReturn=int;
enum {kIOReturnSuccess=0,kIOReturnBadArgument=1,kIOReturnNotReady=2,kIOReturnNoMemory=3};
struct IOLock {bool held=false;};
static void IOLockLock(IOLock *l){assert(!l->held);l->held=true;}
static void IOLockUnlock(IOLock *l){assert(l->held);l->held=false;}
#include "A04BR3HIDSettings.h"
struct IOHIDEventService {
 IOLock lock;IOLock *_clientLock=&lock;bool failPublish=false;
 std::map<std::string,OSObject*> properties;
 ~IOHIDEventService(){for(auto &p:properties)p.second->release();}
 OSObject *copyProperty(const char *s){assert(lock.held);auto i=properties.find(s);if(i==properties.end())return nullptr;i->second->retain();return i->second;}
 bool setProperty(const char *s,OSObject *p){assert(lock.held);assert(std::string(s)=="HIDEventServiceProperties");if(failPublish)return false;auto i=properties.find(s);if(i!=properties.end())i->second->release();p->retain();properties[s]=p;return true;}
 OSDictionary *settings(){auto i=properties.find(a04_hid_settings_key);return i==properties.end()?nullptr:dynamic_cast<OSDictionary*>(i->second);}
 IOReturn setProperties(OSObject *);
};
'''
        post = r'''
int main(){unsigned cases=0;
 {
 IOHIDEventService service;auto *identity=new OSObject;service.properties["DeviceUsagePairs"]=identity;
 auto *updates=new OSDictionary;updates->values={{"HIDClickTime",42},{"HIDResetKeyboard",1},{"HIDResetPointer",1},{"HIDDeviceParameters",1},{"PrimaryUsagePage",7}};
 auto input=updates->values;
 assert(service.setProperties(updates)==0);assert(service.settings()->values.at("HIDClickTime")==42);
 assert(service.settings()->values.size()==2&&updates->values==input);assert(service.properties["DeviceUsagePairs"]==identity);++cases;
 updates->values={{"HIDClickTime",43},{"HIDKeyboardMode",9}};
 assert(service.setProperties(updates)==0);assert(service.settings()->values.at("HIDClickTime")==43&&service.settings()->values.at("HIDKeyboardMode")==9&&service.settings()->values.at("PrimaryUsagePage")==7);++cases;
 assert(service.setProperties(identity)==kIOReturnBadArgument&&!service.lock.held);++cases;
 service._clientLock=nullptr;assert(service.setProperties(updates)==kIOReturnNotReady);service._clientLock=&service.lock;++cases;
 auto *before=service.settings();auto saved=before->values;
 failCopy=1;assert(service.setProperties(updates)==kIOReturnNoMemory&&service.settings()==before&&service.settings()->values==saved&&!service.lock.held);failCopy=0;++cases;
 failMerge=1;assert(service.setProperties(updates)==kIOReturnNoMemory&&service.settings()==before&&service.settings()->values==saved&&!service.lock.held);failMerge=0;++cases;
 service.failPublish=true;assert(service.setProperties(updates)==kIOReturnNoMemory&&service.settings()==before&&service.settings()->values==saved&&!service.lock.held);service.failPublish=false;++cases;
 auto *snapshot=service.settings();snapshot->retain();updates->values={{"HIDClickTime",44}};assert(service.setProperties(updates)==0&&snapshot->values==saved&&service.settings()!=snapshot);snapshot->release();++cases;
 updates->release();
 }
 assert(alive==0);
 {
 IOHIDEventService service;service.properties[a04_hid_settings_key]=new OSObject;
 auto *d=new OSDictionary;d->values={{"HIDClickTime",7}};assert(service.setProperties(d)==0&&service.settings()->values==d->values);d->release();++cases;
 }
 assert(alive==0);assert(a04_hid_settings_copy(nullptr,nullptr)==nullptr);++cases;
 printf("PASS %u settings persistence/rollback/ownership cases\n",cases);return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'A04BR3HIDSettings.h').write_bytes((HID / 'A04BR3HIDSettings.h').read_bytes())
            (root / 'test.cpp').write_text(harness + '\n' + function + '\n' + post)
            executable = root / 'settings-test'
            build = subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie',
                str(root / 'test.cpp'), '-o', str(executable)], capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stderr)
            run = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertIn('PASS 10 settings', run.stdout)

if __name__ == '__main__':
    unittest.main()
