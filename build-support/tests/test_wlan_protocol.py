# SPDX-License-Identifier: MIT
import unittest
import test_wmt_protocol as codecs
from test_wlan_host import FAKE

class WLANProtocol(unittest.TestCase):
    run_c = codecs.WMTProtocol.run_c

    def test_scan_channel_bytes_send_async_match(self):
        self.run_c(FAKE + r'''
#include "A04BR3WLANProtocol.h"
int main(void){
 struct card c;struct a04_sdio_io io;struct a04_sdio_transport t;struct a04_wlan_host h;
 setup(&c,&io,&t);assert(!a04_wlan_host_open(&h,&t));h.ready=1;assert(!a04_wlan_runtime_begin(&h));
 const uint8_t ssid[]={'t','e','s','t'},ie[]={221,2,0xaa,0xbb},channels[]={1,1,1,6,1,11};
 struct a04_wlan_scan req;memset(&req,0,sizeof(req));
 req.sequence=43;req.bss=1;req.active=1;req.probes=2;req.min_dwell=20;req.dwell=40;req.timeout=1000;
 req.ssid=ssid;req.ssid_length=4;req.ie=ie;req.ie_length=4;
 req.channels=channels;req.channel_count=3;req.channel_type=4;
 uint8_t p[714];unsigned n=0;assert(!a04_wlan_scan_encode(&req,p,sizeof(p),&n));
 assert(n==118 && p[0]==43 && p[3]==4 && a04_wmt_le16(p+6)==20 && a04_wmt_le16(p+10)==1000);
 assert(!memcmp(p+12,ssid,4) && p[44]==4 && p[45]==3 && !memcmp(p+48,channels,6));
 assert(a04_wmt_le16(p+112)==4 && !memcmp(p+114,ie,4));
 struct a04_wlan_scan_state s;memset(&s,0,sizeof(s));
 assert(!a04_wlan_scan_begin(&h,&s,&req) && s.pending && c.txs==1);
 assert(h.tx[4]==0x1a && h.tx[5]==0xa0 && h.tx[6]==1 && h.tx[7]==1 && !memcmp(h.tx+8,p,n));
 uint8_t evt[24];memset(evt,0,sizeof(evt));a04_wmt_put16(evt,12);a04_wmt_put16(evt+2,0xe000);evt[4]=0x0d;evt[8]=43;
 struct a04_wlan_event v;assert(!a04_wlan_event_parse(evt,12,&v));
 assert(!a04_wlan_scan_done(&s,&v) && s.complete && !s.pending);
 assert(a04_wlan_scan_begin(&h,&s,&req)==A04_WLAN_STATE && c.txs==1);
 struct a04_wlan_channel ch={1,73,6,1,400};
 assert(!a04_wlan_channel_send(&h,&ch,0) && c.txs==2 && h.tx[4]==0x1c && h.tx[6]==1);
 assert(h.tx[8]==1 && h.tx[9]==73 && h.tx[11]==6 && h.tx[13]==1 && a04_wmt_get32(h.tx+20)==400);
 a04_wmt_put16(evt,24);evt[4]=0x10;memset(evt+8,0,16);
 evt[8]=1;evt[9]=73;evt[11]=6;evt[13]=1;a04_wmt_put32(evt+20,300);
 assert(!a04_wlan_event_parse(evt,24,&v));unsigned interval=0;
 assert(!a04_wlan_channel_grant(&ch,&v,&interval) && interval==300);
 evt[9]++;assert(a04_wlan_channel_grant(&ch,&v,&interval)==A04_WLAN_SEQUENCE && !interval);evt[9]--;
 evt[10]=1;assert(a04_wlan_channel_grant(&ch,&v,&interval)==A04_WLAN_STATUS);
 req.ie_length=3;assert(a04_wlan_scan_encode(&req,p,sizeof(p),&n)==A04_WLAN_FORMAT && !n);
 req.ie_length=4;req.channel_count=33;assert(a04_wlan_scan_encode(&req,p,sizeof(p),&n)==A04_WLAN_ARGUMENT);
 memset(&s,0,sizeof(s));s.pending=1;s.sequence=1;v.id=0x0d;v.length=0;
 assert(a04_wlan_scan_done(&s,&v)==A04_WLAN_SEQUENCE && s.failed);
 return 0;
}
''')

    def test_management_descriptor_and_open_auth_assoc_response(self):
        self.run_c(FAKE + r'''
#include "A04BR3WLANProtocol.h"
int main(void){
 struct card c;struct a04_sdio_io io;struct a04_sdio_transport t;struct a04_wlan_host h;
 setup(&c,&io,&t);assert(!a04_wlan_host_open(&h,&t));h.ready=1;assert(!a04_wlan_runtime_begin(&h));
 struct a04_wlan_open_join s;memset(&s,0,sizeof(s));
 const uint8_t client[]={2,1,2,3,4,5},ap[]={2,6,7,8,9,10};
 memcpy(s.client,client,6);memcpy(s.ap,ap,6);
 uint8_t frame[256],reply[256];unsigned n=0;
 assert(!a04_wlan_auth_encode(&s,frame,sizeof(frame),&n) && n==30 && !s.phase);
 assert(frame[0]==0xb0 && !memcmp(frame+4,ap,6) && !memcmp(frame+10,client,6));
 assert(a04_wmt_le16(frame+24)==0 && a04_wmt_le16(frame+26)==1 && !a04_wmt_le16(frame+28));
 assert(!a04_wlan_join_send(&h,&s,7,2,frame,n) && s.phase==1);
 assert(a04_wmt_le16(h.tx)==58 && h.tx[2]==26 && h.tx[3]==0x88 && h.tx[4]==7 && h.tx[5]==0xcc);
 assert(h.tx[7]==8 && h.tx[8]==0x0b && a04_wmt_le16(h.tx+12)==0x800 && !memcmp(h.tx+28,frame,n));
 memcpy(reply,frame,n);memcpy(reply+4,client,6);memcpy(reply+10,ap,6);a04_wmt_put16(reply+26,2);
 assert(!a04_wlan_auth_response(&s,reply,30) && s.phase==2);
 uint8_t rates[]={0x82,0x84,0x8b,0x96,12,18,24,36,48,72,96,108};
 assert(!a04_wlan_assoc_encode(&s,1,10,(const uint8_t *)"test",4,rates,12,frame,sizeof(frame),&n));
 assert(n==50 && !frame[0] && a04_wmt_le16(frame+24)==1 && a04_wmt_le16(frame+26)==10);
 assert(!a04_wlan_ies(frame+28,n-28) && frame[44]==50 && frame[45]==4);
 assert(!a04_wlan_join_send(&h,&s,7,2,frame,n) && s.phase==3);
 memset(reply,0,32);reply[0]=0x10;memcpy(reply+4,client,6);memcpy(reply+10,ap,6);memcpy(reply+16,ap,6);
 a04_wmt_put16(reply+28,0xc123);reply[30]=1;reply[31]=0;
 assert(!a04_wlan_assoc_response(&s,reply,32) && s.phase==4 && s.aid==0x123);
 assert(a04_wlan_join_send(&h,&s,7,2,frame,n)==A04_WLAN_STATE && c.txs==2);
 /* Failure is terminal; incorrect peer/status never advances association. */
 memset(&s,0,sizeof(s));memcpy(s.client,client,6);memcpy(s.ap,ap,6);s.phase=3;
 reply[4]^=1;assert(a04_wlan_assoc_response(&s,reply,32)==A04_WLAN_SEQUENCE && s.phase==3 && !s.aid);
 reply[4]^=1;assert(a04_wlan_assoc_response(&s,reply,32)==A04_WLAN_STATE);
 memset(&s,0,sizeof(s));memcpy(s.client,client,6);memcpy(s.ap,ap,6);s.phase=3;
 reply[26]=1;assert(a04_wlan_assoc_response(&s,reply,32)==A04_WLAN_STATUS && !s.aid);
 return 0;
}
''')

    def test_rx_descriptor_groups_padding_and_corrupt_lengths(self):
        self.run_c(r'''
#include "A04BR3WLANProtocol.h"
int main(void){
 uint8_t p[128];struct a04_wlan_rx_frame frame;
 for(unsigned groups=0;groups<16;groups++)for(unsigned offset=0;offset<2;offset++){
  memset(p,0,sizeof(p));unsigned head=16+((groups&8)?16:0)+((groups&1)?16:0)+((groups&2)?8:0)+((groups&4)?24:0)+2*offset;
  unsigned n=head+30;a04_wmt_put16(p,n);a04_wmt_put16(p+2,0xe001|(groups<<9));
  p[6]=(uint8_t)(24|(offset<<6));p[8]=7;p[5]=6;p[head]=0xb0;
  assert(!a04_wlan_rx_frame_parse(p,n,&frame) && frame.length==30 && frame.data==p+head && frame.wlan==7);
  for(unsigned i=0;i<n;i++)assert(a04_wlan_rx_frame_parse(p,i,&frame));
  p[10]=2;assert(a04_wlan_rx_frame_parse(p,n,&frame)==A04_WLAN_STATUS);p[10]=0;
  p[6]|=0x80;assert(a04_wlan_rx_frame_parse(p,n,&frame)==A04_WLAN_STATUS);
 }
 memset(p,0,sizeof(p));a04_wmt_put16(p,16);a04_wmt_put16(p+2,0xfe01);p[6]=24;
 assert(a04_wlan_rx_frame_parse(p,16,&frame)==A04_WLAN_FORMAT);
 uint8_t ies[]={1,2,0,0,3,0};assert(!a04_wlan_ies(ies,6));assert(a04_wlan_ies(ies,5)==A04_WLAN_FORMAT);
 return 0;
}
''')

if __name__ == '__main__':
    unittest.main()
