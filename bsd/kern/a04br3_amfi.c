/*
 * Built-in AMFI interface for A04BR3.
 *
 * On Apple platforms AppleMobileFileIntegrity registers this interface and
 * provides entitlements through the closed CoreEntitlements library. A04BR3
 * has neither, and code signing reaches it on every exec (ubc_cs_blob_add).
 *
 * Entitlements: when a code signature is added, its XML entitlements blob
 * (already checked against the code directory) is parsed into a
 * A04BR3Entitlements object (iokit/Kernel/arm/A04BR3Entitlements.cpp), set as
 * the blob's csb_entitlements, and every query is answered from it. DER-only
 * signatures get none. Code signature and page hash validation stay with the
 * kernel. Trust caches remain unavailable (no Image4), so the TrustCache
 * interface is left at version 0.
 *
 * Platform binaries: AMFI marks binaries whose cdhash is in a trust cache
 * as platform binaries from its vnode_check_signature MAC hook. Without
 * trust caches, a04br3.platform_root=1 explicitly marks signed binaries on
 * the read-only root filesystem instead (the iOS system binaries are ad-hoc signed and are
 * platform binaries only through the static trust cache).
 *
 * Interfaces no kernel path uses on A04BR3 (they serve other Apple kexts)
 * panic with a clear message instead of being NULL.
 */
#if defined(ARM64_BOARD_CONFIG_A04BR3)
#include <kern/debug.h>
#include <kern/startup.h>
#include <libkern/amfi/amfi.h>
#include <mach/kern_return.h>
#include <sys/codesign.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/ubc_internal.h>
#include <sys/vnode.h>
#include <security/mac_policy.h>

/* iokit/Kernel/arm/A04BR3Entitlements.cpp */
void *a04br3_entitlements_create(const CS_GenericBlob *blob, size_t blob_len);
void *a04br3_entitlements_copy_dict(const void *ent);
bool a04br3_entitlements_copy_xml(const void *ent, CS_GenericBlob **out);
bool a04br3_entitlements_bool(const void *ent, const char *name);
bool a04br3_entitlements_string(const void *ent, const char *name, const char *value);
void *a04br3_entitlements_copy_object(const void *ent, const char *name);
unsigned int a04br3_entitlements_count(const void *ent);
extern void osobject_release(void *object);

/* boot-arg a04br3_amfi_debug=1: log the entitlements attached to each signature. */
static TUNABLE(uint32_t, a04br3_amfi_debug, "a04br3_amfi_debug", 0);
/* Explicit development trust substitute; never enabled by default. */
static TUNABLE(uint32_t, a04br3_platform_root, "a04br3.platform_root", 0);
static TUNABLE(uint32_t, a04br3_root_rw, "a04br3.root_rw", 0);
static TUNABLE(uint32_t, a04br3_emmc_rw, "a04br3.emmc_rw", 0);

/* The entitlements object of a process's main binary, or NULL. */
static void *
a04br3_proc_entitlements(proc_t proc)
{
	struct cs_blob *blob = csproc_get_blob(proc);
	return blob != NULL ? csblob_os_entitlements_get(blob) : NULL;
}

static void
a04br3_amfi_entitlements_invalidate(__unused void *os_entitlements)
{
	/* Queries already require a valid signature (csblob_get_entitlements). */
}

static void *
a04br3_amfi_entitlements_as_dict(void *os_entitlements)
{
	return a04br3_entitlements_copy_dict(os_entitlements);
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
a04br3_amfi_entitlements_get_xml(void *os_entitlements, CS_GenericBlob **blob)
{
	return a04br3_entitlements_copy_xml(os_entitlements, blob);
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

/*
 * Attach the parsed XML entitlements to a code signature being added. This
 * must happen in the vnode_check_signature MAC hook, like AMFI does: later,
 * converting a 4K-page signature to multilevel hashes drops the XML blob.
 * Nothing is granted when the blob is missing, unreadable or does not match
 * its code directory hash.
 */
static void
a04br3_amfi_attach_entitlements(struct cs_blob *code_signing_blob)
{
	void *xml = NULL;
	size_t xml_len = 0;

	if (csblob_get_entitlements(code_signing_blob, &xml, &xml_len) != 0) {
		printf("A04BR3 AMFI: %s: entitlements do not match the code directory, granting none\n",
		    csblob_get_identity(code_signing_blob));
		return;
	}
	if (xml == NULL) {
		return;                 /* no XML entitlements */
	}
	void *ent = a04br3_entitlements_create(xml, xml_len);
	if (ent == NULL) {
		printf("A04BR3 AMFI: %s: unreadable entitlements (%zu bytes), granting none\n",
		    csblob_get_identity(code_signing_blob), xml_len);
		return;
	}
	if (a04br3_amfi_debug) {
		/* The test keys only exist in tools/mkinit.py test signatures. */
		printf("A04BR3 AMFI: %s: %u entitlements (test: bool %d string %d array %d)\n",
		    csblob_get_identity(code_signing_blob), a04br3_entitlements_count(ent),
		    a04br3_entitlements_bool(ent, "com.apple.private.a04br3.test-bool"),
		    a04br3_entitlements_string(ent, "com.apple.private.a04br3.test-string", "hello"),
		    a04br3_entitlements_string(ent, "com.apple.private.a04br3.test-array", "two"));
	}
	csblob_os_entitlements_set(code_signing_blob, ent);    /* takes a reference */
	osobject_release(ent);
}

static kern_return_t
a04br3_amfi_adjust_without_monitor(__unused void *os_entitlements,
    __unused struct cs_blob *code_signing_blob)
{
	/* Entitlements were attached in a04br3_amfi_vnode_check_signature. */
	return KERN_SUCCESS;
}

static kern_return_t
a04br3_amfi_query_boolean(const void *os_entitlements, const char *entitlement_name)
{
	return a04br3_entitlements_bool(os_entitlements, entitlement_name) ? KERN_SUCCESS : KERN_DENIED;
}

static kern_return_t
a04br3_amfi_query_boolean_with_proc(const proc_t proc, const char *entitlement_name)
{
	return a04br3_amfi_query_boolean(a04br3_proc_entitlements(proc), entitlement_name);
}

static kern_return_t
a04br3_amfi_query_string(const void *os_entitlements,
    const char *entitlement_name, const char *entitlement_value)
{
	return a04br3_entitlements_string(os_entitlements, entitlement_name, entitlement_value) ?
	       KERN_SUCCESS : KERN_DENIED;
}

static kern_return_t
a04br3_amfi_query_string_with_proc(const proc_t proc,
    const char *entitlement_name, const char *entitlement_value)
{
	return a04br3_amfi_query_string(a04br3_proc_entitlements(proc), entitlement_name,
	           entitlement_value);
}

static kern_return_t
a04br3_amfi_copy_object(const void *os_entitlements,
    const char *entitlement_name, void **entitlement_object)
{
	void *obj = a04br3_entitlements_copy_object(os_entitlements, entitlement_name);
	if (entitlement_object) {
		*entitlement_object = obj;
	} else if (obj) {
		osobject_release(obj);
	}
	return obj ? KERN_SUCCESS : KERN_NOT_FOUND;
}

static kern_return_t
a04br3_amfi_copy_object_with_proc(const proc_t proc,
    const char *entitlement_name, void **entitlement_object)
{
	return a04br3_amfi_copy_object(a04br3_proc_entitlements(proc), entitlement_name,
	           entitlement_object);
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
	printf("A04BR3 AMFI: built-in interface registered (XML entitlements)\n");
}
STARTUP(EARLY_BOOT, STARTUP_RANK_MIDDLE, a04br3_amfi_register);

/*
 * Every signed binary on the root filesystem is a platform binary. New
 * signatures (ubc_cs_blob_add passes its writable copy; revalidation passes
 * the read-only blob itself) get their entitlements here.
 */
static int
a04br3_amfi_vnode_check_signature(struct vnode *vp, __unused struct label *label,
    __unused cpu_type_t cpu_type, struct cs_blob *cs_blob,
    unsigned int *cs_flags, __unused unsigned int *signer_type,
    __unused int flags, __unused unsigned int platform,
    __unused char **fatal_failure_desc, __unused size_t *fatal_failure_desc_len)
{
	mount_t mp = vnode_mount(vp);
	/* XNU has validated this signature before invoking the policy. */
	*cs_flags |= CS_SIGNED;
	if (a04br3_platform_root == 1 && mp != NULL &&
	    (vfs_flags(mp) & MNT_ROOTFS) && ((vfs_flags(mp) & MNT_RDONLY) ||
        (a04br3_root_rw==1 && a04br3_emmc_rw==1))) {
		*cs_flags |= CS_PLATFORM_BINARY;
	}
	if (cs_blob != cs_blob->csb_ro_addr && cs_blob->csb_entitlements == NULL) {
		a04br3_amfi_attach_entitlements(cs_blob);
	}
	return 0;
}

/* Development-only stand-in adapted from ios-sweet patch 0050.
 * No Apple Sandbox kext is present. Avoid ENOPOLICY for native checks;
 * operations requiring output data fail rather than fabricate a token/path.
 */
static uint64_t a04br3_sandbox_seen;
static int
a04br3_sandbox_syscall(struct proc *p, int call, __unused user_addr_t arg)
{
    if (call >= 0 && call < 64 && !(a04br3_sandbox_seen & (1ULL << call))) {
        a04br3_sandbox_seen |= 1ULL << call;
        char name[32];
        proc_name(proc_pid(p), name, sizeof(name));
        printf("A04BR3 Sandbox: %s op %d\n", name, call);
    }
    switch (call) {
    case 4: case 5: case 6: case 0x43: return ENOENT;
    default: return 0;
    }
}
static const struct mac_policy_ops a04br3_sandbox_mac_ops = {
    .mpo_policy_syscall = a04br3_sandbox_syscall,
};
static struct mac_policy_conf a04br3_sandbox_mac_conf = {
    .mpc_name = "Sandbox",
    .mpc_fullname = "A04BR3 development permissive sandbox stand-in",
    .mpc_ops = &a04br3_sandbox_mac_ops,
};
static mac_policy_handle_t a04br3_sandbox_mac_handle;

static const struct mac_policy_ops a04br3_amfi_mac_ops = {
	.mpo_vnode_check_signature = a04br3_amfi_vnode_check_signature,
};

static struct mac_policy_conf a04br3_amfi_mac_conf = {
	.mpc_name = "A04BR3 AMFI",
	.mpc_fullname = "A04BR3 built-in Mobile File Integrity",
	.mpc_ops = &a04br3_amfi_mac_ops,
};

static mac_policy_handle_t a04br3_amfi_mac_handle;

/* Called from bsd_init once MAC policies can register, before any exec. */
void a04br3_amfi_policy_register(void);

void
a04br3_amfi_policy_register(void)
{
	int error = mac_policy_register(&a04br3_amfi_mac_conf, &a04br3_amfi_mac_handle, NULL);
	printf("A04BR3 AMFI: platform binary policy %s (%d)\n",
	    error ? "NOT registered" : "registered", error);
    if (a04br3_platform_root == 1 && a04br3_root_rw == 1 && a04br3_emmc_rw == 1) {
        error = mac_policy_register(&a04br3_sandbox_mac_conf, &a04br3_sandbox_mac_handle, NULL);
        printf("A04BR3 Sandbox: development permissive policy %s (%d)\n",
            error ? "NOT registered" : "registered", error);
    }
}
#endif /* ARM64_BOARD_CONFIG_A04BR3 */
