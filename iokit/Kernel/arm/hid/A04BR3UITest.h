/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Private, explicitly opted-in board UI test ABI. No held/raw input API.
 * Selector0: exactly three uint64 scalars {operation,x,y}; no output.
 * Home requires x=y=0; tap uses the physical 800x1280 panel coordinates.
 */
#ifndef A04BR3_UI_TEST_H
#define A04BR3_UI_TEST_H
#include <stdint.h>
#define A04_UI_CLIENT_TYPE 0xa04bu
#define A04_UI_SELECTOR 0u
#define A04_UI_HOME 1u
#define A04_UI_TAP 2u
static inline int a04_ui_request_valid(uint32_t selector,const uint64_t *s,uint32_t count)
{
    if(selector!=A04_UI_SELECTOR || !s || count!=3)return 0;
    return (s[0]==A04_UI_HOME && !s[1] && !s[2]) ||
        (s[0]==A04_UI_TAP && s[1]<800 && s[2]<1280);
}
#endif
