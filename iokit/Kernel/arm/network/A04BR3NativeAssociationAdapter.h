/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04BR3_NATIVE_ASSOCIATION_ADAPTER_H
#define A04BR3_NATIVE_ASSOCIATION_ADAPTER_H
#include "A04BR3NativeAssociation.h"
#include "A04BR3NativeCurrentNetwork.h"
#include "../sdio/A04BR3WLANBackendAPI.h"
#if !defined(A04_WLAN_JOIN_TARGET_API) || A04_WLAN_JOIN_TARGET_API != 1
#error "Native association requires atomic target admission API1"
#endif
static inline int a04_assoc_provider_info(void *p,struct a04_wlan_scan_info *out)
{return a04br3_wlan_scan_snapshot((IOService *)p,out);}
static inline int a04_assoc_provider_record(void *p,unsigned generation,unsigned index,struct a04_wlan_scan_record *out)
{return a04br3_wlan_scan_copy_record((IOService *)p,generation,index,out);}
static inline int a04_assoc_provider_join(void *p,const struct a04_wlan_join_target *target)
{return a04br3_wlan_join_target((IOService *)p,target);}
static inline int a04_assoc_provider_set(IOService *provider,uint32_t value,const void *in,uint32_t bytes)
{
 if(!provider)return ENXIO;
 struct a04_network_backend backend={};int error=a04br3_wlan_acquire_backend(provider,&backend);
 if(error)return error;
 struct a04_network_state state={};error=backend.state(backend.context,&state);
 if(!error && (!a04_network_control_ready(&state) || !state.radio_enabled))error=ENETDOWN;
 struct a04_wlan_scan_record *work=nullptr;
 if(!error){work=(a04_wlan_scan_record *)IOMallocZero(sizeof(*work));if(!work)error=ENOMEM;}
 if(!error){struct a04_association_backend api={provider,a04_assoc_provider_info,a04_assoc_provider_record,a04_assoc_provider_join};
  error=a04_native_association_set(&api,value,in,bytes,work);}
 if(work)IOFree(work,sizeof(*work));
 a04br3_wlan_release_backend(provider);return error;
}
/* BSD gate may be held by the caller; no SDIO lock is held on entry.
 * acquire_backend adds one provider reference, not a held lock. state() and
 * current_record() separately take/release transportLock; current_record
 * rechecks real association and copies all bytes before dropping that lock.
 * Encode/copy runs lock-free with respect to SDIO, then frees work and drops
 * exactly that temporary provider reference. No refresh/ioctl/DLIL reentry.
 */
static inline int a04_assoc_provider_current(IOService *provider,uint32_t value,void *out,uint32_t *bytes,uint64_t now)
{
 if(!provider)return ENXIO;
 if(value)return EOPNOTSUPP;
 struct a04_network_backend backend={};int error=a04br3_wlan_acquire_backend(provider,&backend);if(error)return error;
 struct a04_network_state state={};error=backend.state(backend.context,&state);
 if(!error && !a04_network_control_ready(&state))error=ENETDOWN;
 if(!error && !state.associated)error=ENOTCONN;
 struct a04_current_network_work *work=nullptr;
 if(!error){work=(a04_current_network_work *)IOMallocZero(sizeof(*work));if(!work)error=ENOMEM;}
 if(!error)error=a04br3_wlan_current_record(provider,&work->raw);
 if(!error)error=a04_native_current_encode(work,now,out,bytes);
 if(work)IOFree(work,sizeof(*work));
 a04br3_wlan_release_backend(provider);return error;
}
#endif
