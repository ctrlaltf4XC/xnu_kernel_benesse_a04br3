/*
 * Built-in AMFI interface for A04BR3.
 *
 * On Apple platforms AppleMobileFileIntegrity registers this interface and
 * provides entitlements through the closed CoreEntitlements library. A04BR3
 * has neither, and code signing reaches it on every exec (ubc_cs_blob_add).
 *
 * This provider honestly reports that no entitlements exist: code signature
 * blobs get no OSEntitlements object (csb_entitlements stays NULL, which the
 * kernel already handles), every entitlement query fails and every copy
 * returns nothing. Nothing is granted. Code signature and page hash
 * validation stay with the kernel. Trust caches remain unavailable (no
 * Image4), so the TrustCache interface is left at version 0.
 *
 * Interfaces no kernel path uses on A04BR3 (they serve other Apple kexts)
 * panic with a clear message instead of being NULL.
 */
#if defined(ARM64_BOARD_CONFIG_A04BR3)
#include <kern/debug.h>
#include <kern/startup.h>
#include <libkern/amfi/amfi.h>
#include <mach/kern_return.h>
#include <sys/proc.h>
#include <sys/systm.h>

static void
a04br3_amfi_entitlements_invalidate(__unused void *os_entitlements)
{
	/* No OSEntitlements objects are ever created. */
}

static void *
a04br3_amfi_entitlements_as_dict(__unused void *os_entitlements)
{
	return NULL;
}

static CEError_t
a04br3_amfi_entitlements_query(__unused void *os_entitlements,
    __unused uint8_t cdhash[CS_CDHASH_LEN], __unused CEQuery_t query,
    __unused size_t query_length)
{
	panic("A04BR3 AMFI: CoreEntitlements queries are not implemented");
}

static bool
a04br3_amfi_entitlements_get_transmuted(__unused void *os_entitlements,
    __unused const CS_GenericBlob **blob)
{
	return false;
}

static bool
a04br3_amfi_entitlements_get_xml(__unused void *os_entitlements,
    __unused CS_GenericBlob **blob)
{
	return false;
}

static bool
a04br3_amfi_get_legacy_profile_exemptions(__unused const uint8_t **profile,
    __unused size_t *profile_length)
{
	return false;
}

static bool
a04br3_amfi_get_udid(__unused const uint8_t **udid, __unused size_t *udid_length)
{
	return false;
}

static void *
a04br3_amfi_query_context_to_object(__unused CEQueryContext_t ctx)
{
	panic("A04BR3 AMFI: CoreEntitlements query contexts are not implemented");
}

static kern_return_t
a04br3_amfi_adjust_with_monitor(__unused void *os_entitlements,
    __unused const CEQueryContext_t ce_ctx, __unused const void *monitor_sig_obj,
    __unused const char *identity, __unused const uint32_t code_signing_flags)
{
	/* A04BR3 has no code signing monitor. */
	return KERN_NOT_SUPPORTED;
}

static kern_return_t
a04br3_amfi_adjust_without_monitor(__unused void *os_entitlements,
    __unused struct cs_blob *code_signing_blob)
{
	/* Leave the blob without an entitlements context: it has none. */
	return KERN_SUCCESS;
}

static kern_return_t
a04br3_amfi_query_boolean(__unused const void *os_entitlements,
    __unused const char *entitlement_name)
{
	return KERN_DENIED;
}

static kern_return_t
a04br3_amfi_query_boolean_with_proc(__unused const proc_t proc,
    __unused const char *entitlement_name)
{
	return KERN_DENIED;
}

static kern_return_t
a04br3_amfi_query_string(__unused const void *os_entitlements,
    __unused const char *entitlement_name, __unused const char *entitlement_value)
{
	return KERN_DENIED;
}

static kern_return_t
a04br3_amfi_query_string_with_proc(__unused const proc_t proc,
    __unused const char *entitlement_name, __unused const char *entitlement_value)
{
	return KERN_DENIED;
}

static kern_return_t
a04br3_amfi_copy_object(__unused const void *os_entitlements,
    __unused const char *entitlement_name, void **entitlement_object)
{
	if (entitlement_object) {
		*entitlement_object = NULL;
	}
	return KERN_NOT_FOUND;
}

static kern_return_t
a04br3_amfi_copy_object_with_proc(__unused const proc_t proc,
    __unused const char *entitlement_name, void **entitlement_object)
{
	if (entitlement_object) {
		*entitlement_object = NULL;
	}
	return KERN_NOT_FOUND;
}

static const amfi_t a04br3_amfi = {
	.OSEntitlements_invalidate = a04br3_amfi_entitlements_invalidate,
	.OSEntitlements_asdict = a04br3_amfi_entitlements_as_dict,
	.OSEntitlements_query = a04br3_amfi_entitlements_query,
	.OSEntitlements_get_transmuted = a04br3_amfi_entitlements_get_transmuted,
	.OSEntitlements_get_xml = a04br3_amfi_entitlements_get_xml,
	/* CoreEntitlements is only used by the PMAP_CS code signing monitor. */
	.get_legacy_profile_exemptions = a04br3_amfi_get_legacy_profile_exemptions,
	.get_udid = a04br3_amfi_get_udid,
	.query_context_to_object = a04br3_amfi_query_context_to_object,
	/* .TrustCache stays version 0: trust caches are unavailable without Image4. */
	.OSEntitlements = {
		.version = OSENTITLEMENTS_INTERFACE_VERSION,
		.adjustContextWithMonitor = a04br3_amfi_adjust_with_monitor,
		.adjustContextWithoutMonitor = a04br3_amfi_adjust_without_monitor,
		.queryEntitlementBoolean = a04br3_amfi_query_boolean,
		.queryEntitlementBooleanWithProc = a04br3_amfi_query_boolean_with_proc,
		.queryEntitlementString = a04br3_amfi_query_string,
		.queryEntitlementStringWithProc = a04br3_amfi_query_string_with_proc,
		.copyEntitlementAsOSObject = a04br3_amfi_copy_object,
		.copyEntitlementAsOSObjectWithProc = a04br3_amfi_copy_object_with_proc,
	},
};

__startup_func
static void
a04br3_amfi_register(void)
{
	amfi_interface_register(&a04br3_amfi);
	printf("A04BR3 AMFI: built-in interface registered (no entitlements)\n");
}
STARTUP(EARLY_BOOT, STARTUP_RANK_MIDDLE, a04br3_amfi_register);
#endif /* ARM64_BOARD_CONFIG_A04BR3 */
