/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Development-only "open view" of one WPA2-PSK network whose SSID and
 * passphrase were loaded at runtime through the console-gated private UI
 * client (A04BR3HID.cpp a04br3_dev_wifi_credentials). Setup cannot enter text
 * on this port yet; iOS sees the network as open and the driver performs the
 * real WPA2-PSK join with the loaded passphrase. Nothing is persisted/logged.
 */
#ifndef A04BR3_DEV_OPEN_NETWORK_H
#define A04BR3_DEV_OPEN_NETWORK_H
#include <stdint.h>
#include <string.h>
extern "C" int a04br3_dev_wifi_credentials(char ssid[32], unsigned *ssid_len, char psk[63], unsigned *psk_len) __attribute__((weak));
static inline unsigned a04_dev_open_match(const uint8_t *ssid,unsigned length,char psk[63],unsigned *psk_length)
{
    char loaded[32];unsigned loaded_length=0,dummy_length=0;char dummy[63];
    if(!psk){psk=dummy;psk_length=&dummy_length;}
    if(!a04br3_dev_wifi_credentials || !a04br3_dev_wifi_credentials(loaded,&loaded_length,psk,psk_length))return 0;
    return loaded_length==length && !memcmp(loaded,ssid,length);
}
/* Remove RSN (48) and WPA vendor (221, 00:50:F2 type1) IEs in place. */
static inline uint16_t a04_dev_open_strip_ies(uint8_t *ies,uint16_t length)
{
    uint16_t in=0,out=0;
    while(in+2<=length){
        unsigned id=ies[in],n=ies[in+1];
        if(in+2+n>length)break;
        unsigned wpa=id==221 && n>=4 && ies[in+2]==0x00 && ies[in+3]==0x50 && ies[in+4]==0xf2 && ies[in+5]==1;
        if(id!=48 && !wpa){memmove(ies+out,ies+in,2+n);out=(uint16_t)(out+2+n);}
        in=(uint16_t)(in+2+n);
    }
    return out;
}
#endif
