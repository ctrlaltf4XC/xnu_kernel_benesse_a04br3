/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Thin production binding only; real primitives live in PersonalCrypto.
 * No second crypto implementation, SDK dependency or deterministic RNG exists.
 * See A04BR3WLANPSK4Way.sources.json for the audited startup/entropy path.
 */
#ifndef A04BR3_WLAN_PSK_BINDING_H
#define A04BR3_WLAN_PSK_BINDING_H
#include "A04BR3WLANPSK4Way.h"
#include "A04BR3WLANPersonalCrypto.h"
static inline int a04_psk_native_pmk(void *ctx,const uint8_t *pass,unsigned n,const uint8_t *ssid,unsigned sn,uint8_t out[32]){
 (void)ctx;return a04_wlan_personal_pmk(pass,n,ssid,sn,out);
}
static inline int a04_psk_native_ptk(void *ctx,const uint8_t pmk[32],const uint8_t ap[6],const uint8_t sta[6],const uint8_t an[32],const uint8_t sn[32],uint8_t out[64]){
 (void)ctx;return a04_wlan_personal_ptk(pmk,ap,sta,an,sn,out);
}
static inline int a04_psk_native_mic(void *ctx,const uint8_t kck[16],const uint8_t *p,unsigned n,uint8_t out[16]){
 (void)ctx;return a04_wlan_personal_mic(kck,p,n,out);
}
static inline int a04_psk_native_unwrap(void *ctx,const uint8_t kek[16],const uint8_t *p,unsigned n,uint8_t *out,unsigned cap,unsigned *bytes){
 (void)ctx;return a04_wlan_personal_unwrap(kek,p,n,out,cap,bytes);
}
#if defined(KERNEL) && defined(ARM64_BOARD_CONFIG_A04BR3)
#include <sys/types.h>
#include <sys/random.h>
/* IOKit driver/worker entry only, AFTER EARLY_BOOT STARTUP_RANK_FIRST has
 * registered the A04BR3 HMAC-SHA256 DRBG. Bootshim requires64 real TRNG bytes;
 * PE rejects missing/allzero seed and the DRBG panics on generate failure.
 * read_random has no error return. Returning0 here follows its successful
 * return, not a fabricated entropy-ready flag. No fallback to timer/MAC/nonce.
 * read_random refresh/generate use mutexes: call outside SDIO/spin/IRQ locks.
 * Native owner must schedule M1 crypto work outside the locked SDIO event pump.
 */
static inline int a04_psk_native_random(void *ctx,uint8_t out[32]){
 (void)ctx;if(!out)return A04_WLAN_ARGUMENT;
 read_random(out,32);return 0;
}
static inline struct a04_psk_crypto a04_psk_native_crypto(void){
 struct a04_psk_crypto ops={0,a04_psk_native_pmk,a04_psk_native_random,
  a04_psk_native_ptk,a04_psk_native_mic,a04_psk_native_unwrap};return ops;
}
#endif
#endif
