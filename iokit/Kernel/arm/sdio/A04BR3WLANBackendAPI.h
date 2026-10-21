/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * SDIO-owned kernel interface for the native bridge owner. No private ioctl
 * numbers or Apple payload layouts are guessed. Acquire ONLY succeeds after
 * net_init and real MAC/control discovery; release after BSD detach finishes.
 */
#ifndef A04BR3_WLAN_BACKEND_API_H
#define A04BR3_WLAN_BACKEND_API_H
#include "A04BR3WLANData.h"
#include "../network/A04BR3NetworkBackend.h"
#ifdef __cplusplus
class IOService;
typedef IOService *a04_wlan_service;
extern "C" {
#else
typedef void *a04_wlan_service;
#endif
struct a04_wlan_native_client {
 a04_wlan_service owner;void *context;
 /* Driver retains owner per callback. Notification/packet callbacks run
  * outside the SDIO lock, may call typed APIs, and must copy packet bytes.
  * ready is delivered only after net_init + dynamic original calibration +
  * real MAC + successful command setup. It is NOT an association callback. */
 void (*ready)(void *,a04_wlan_service);
 void (*changed)(void *);
 void (*packet)(void *,unsigned,const void *,unsigned);
 /* These run in the bridge's gate; do not reenter refresh()/ioctl().
  * Implement only retained/proven native payload ABIs, unknown -> errno45. */
 int (*get)(void *,uint32_t,uint32_t,void *,uint32_t *);
 int (*set)(void *,uint32_t,uint32_t,const void *,uint32_t);
 /* Crypto-only verifier; runs under SDIO lock and MUST NOT reenter driver.
  * Actual native supplicant owns KCK/GTK crypto, no success stub is permitted. */
 a04_wlan_mic_verify verify;
};
int a04br3_wlan_acquire_backend(a04_wlan_service,struct a04_network_backend *);
void a04br3_wlan_release_backend(a04_wlan_service);
int a04br3_wlan_bind_native(a04_wlan_service,const struct a04_wlan_native_client *);
int a04br3_wlan_unbind_native(a04_wlan_service);
int a04br3_wlan_scan(a04_wlan_service);
int a04br3_wlan_scan_count(a04_wlan_service,unsigned *count,unsigned *complete);
int a04br3_wlan_scan_copy(a04_wlan_service,unsigned index,struct a04_wlan_ap *);
/* Explicit native association request for an ACTUALLY observed result. */
int a04br3_wlan_join(a04_wlan_service,unsigned index,unsigned secured);
int a04br3_wlan_eapol(a04_wlan_service,const void *ethernet,unsigned bytes);
int a04br3_wlan_install_key(a04_wlan_service,unsigned pairwise,unsigned keyid,const uint8_t key[16],const uint8_t rsc[8]);
/* Only real authenticated native supplicant completion, never a key ioctl. */
int a04br3_wlan_authorize(a04_wlan_service,int authenticated_result);
#ifdef __cplusplus
}
#endif
#endif
