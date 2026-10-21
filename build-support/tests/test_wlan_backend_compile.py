# SPDX-License-Identifier: MIT
"""Emit new portable backend for arm64; no IOService/metaclass claim here."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import test_wmt_protocol as codecs

class WLANBackendCompile(unittest.TestCase):
    def test_arm64_new_backend_stack_and_footprint(self):
        clang=shutil.which('clang++')
        self.assertIsNotNone(clang)
        names=('slots_take','station_setup','station_scan','station_select','station_event','station_auth',
               'scan_observe','station_auth_rx','station_assoc','station_assoc_rx','ap_parse','security_begin',
               'security_rx','security_install','security_key_event','security_authorize',
               'ethernet_send','ethernet_rx','data_tx_event','backend_receive','disconnect_rx')
        source='#include <stdint.h>\n#include "A04BR3WLANData.h"\n'
        source+='static_assert(sizeof(a04_wlan_station)<2048,"station must be separately owned");\n'
        source+='static_assert(sizeof(a04_wlan_security)<512,"security budget");\n'
        source+='static_assert(sizeof(a04_wlan_data_state)<64,"single packet tracking budget");\n'
        for name in names:
            source+='auto backend_test_'+name+'=&a04_wlan_'+name+';\n'
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);(p/'case.cpp').write_text(source)
            subprocess.run([clang,'--target=arm64-apple-darwin','-std=c++14','-ffreestanding',
                            '-fno-exceptions','-fno-rtti','-O2','-Wall','-Wextra','-Werror',
                            '-Wframe-larger-than=4096','-fstack-usage','-I',str(codecs.INCLUDE),
                            '-c',str(p/'case.cpp'),'-o',str(p/'backend.o')],check=True)
            print((p/'backend.su').read_text(),end='')

if __name__=='__main__':unittest.main()
