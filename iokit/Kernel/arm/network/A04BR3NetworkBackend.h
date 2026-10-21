/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Observed backend state only. No fabricated MAC, firmware, scan or link data.
 * Caller retains context until BSD detach has completed, and serializes backend
 * operations. Callbacks must not reenter this interface while its gate is held.
 */
#ifndef A04BR3_NETWORK_BACKEND_H
#define A04BR3_NETWORK_BACKEND_H
#include <stdint.h>
#include <stddef.h>
struct a04_network_state {
    /* firmware_running/control_ready describe WLAN firmware and real command
     * transport, not WMT patch delivery. Neither requires association. */
    unsigned present,mac_valid,firmware_running,control_ready,datapath_ready,associated;
    uint8_t mac[6];
};
struct a04_network_backend {
    void *context;
    int (*state)(void *,struct a04_network_state *);
    /* Native GET payload is opaque, bounded and initially copied from user.
     * Return errno, and actual initialized output length <= supplied capacity.
     * Unsupported operations MUST return EOPNOTSUPP; never synthesize results. */
    int (*native_get)(void *,uint32_t type,uint32_t value,void *,uint32_t *bytes);
    /* Enables/disables radio control before scan/association. Success means
     * that command succeeded, never that association or IP connectivity exists.
     * Callbacks may update state; bridge resamples after this operation. */
    int (*set_admin)(void *,unsigned enabled);
    /* A successful transmit consumes mbuf. On error leave it to the bridge.
     * Backend must queue promptly; no long firmware exchange in output(). */
    int (*transmit)(void *,void *mbuf);
};
static inline int a04_network_mac_valid(const struct a04_network_state *s)
{
    unsigned any=0;for(unsigned i=0;i<6;i++)any|=s->mac[i];
    return s->present && s->mac_valid && any && !(s->mac[0]&1);
}
static inline int a04_network_control_ready(const struct a04_network_state *s)
{return a04_network_mac_valid(s) && s->firmware_running && s->control_ready;}
static inline int a04_network_data_ready(const struct a04_network_state *s)
{return a04_network_control_ready(s) && s->datapath_ready && s->associated;}
#endif
