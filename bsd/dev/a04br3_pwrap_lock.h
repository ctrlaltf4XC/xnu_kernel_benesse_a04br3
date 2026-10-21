/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * One board WACS2 transaction lock, shared by display, touch and USB.
 * Lock covers idle->CMD->result->VLDCLR->idle, NOT a regulator delay/sleep.
 * No recursive/nested acquisition. USB bounded probes use try_lock and return
 * EBUSY on contention; existing touch/display transactions use lock/unlock.
 */
#ifndef A04BR3_PWRAP_LOCK_H
#define A04BR3_PWRAP_LOCK_H
#ifdef __cplusplus
extern "C" {
#endif
void a04br3_pwrap_lock(void);
int a04br3_pwrap_try_lock(void); /*1 acquired,0 busy*/
void a04br3_pwrap_unlock(void);
#ifdef __cplusplus
}
#endif
#endif
