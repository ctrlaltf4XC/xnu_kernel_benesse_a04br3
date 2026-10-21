/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Opt-in development restore scaffolding for a complete donor HFS root.
 * Create missing container parent directories before native launchd starts.
 * Existing entries/ownership/modes are never replaced or loosened.
 */
#if defined(ARM64_BOARD_CONFIG_A04BR3)
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/vnode_internal.h>
#include <sys/mount.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <pexpert/pexpert.h>
#include <kern/kalloc.h>
#include "a04br3_fairplay_merge.h"
#include "a04br3_mg_cache.h"

void a04br3_root_prepare(void);
static int
prepare_directory(const char *path, int *created)
{
    vfs_context_t ctx = vfs_context_kernel();
    vnode_t vp = NULLVP, parent = NULLVP;
    int error = vnode_lookup(path, VNODE_LOOKUP_NOFOLLOW, &vp, ctx);
    if (!error) {
        error = vnode_isdir(vp) && vnode_mount(vp) == vnode_mount(rootvnode) ? 0 : ENOTDIR;
        vnode_put(vp);
        return error;
    }
    if (error != ENOENT) return error;
    char directory[128];
    if (strlcpy(directory, path, sizeof(directory)) >= sizeof(directory)) return ENAMETOOLONG;
    char *leaf = strrchr(directory, '/');
    if (!leaf || leaf == directory || !leaf[1]) return EINVAL;
    *leaf++ = 0;
    error = vnode_lookup(directory, VNODE_LOOKUP_NOFOLLOW, &parent, ctx);
    if (error) return error;
    if (!vnode_isdir(parent) || vnode_mount(parent) != vnode_mount(rootvnode)) {
        vnode_put(parent);return ENOTDIR;
    }
    struct componentname cn;
    bzero(&cn, sizeof(cn));
    cn.cn_nameiop = CREATE;
    cn.cn_flags = ISLASTCN;
    cn.cn_context = ctx;
    cn.cn_pnbuf = leaf;
    cn.cn_pnlen = (int)strlen(leaf) + 1;
    cn.cn_nameptr = leaf;
    cn.cn_namelen = (int)strlen(leaf);
    struct vnode_attr va;
    VATTR_INIT(&va);
    VATTR_SET(&va, va_uid, 0);
    VATTR_SET(&va, va_gid, 0);
    VATTR_SET(&va, va_mode, 0755);
    VATTR_SET(&va, va_type, VDIR);
    va.va_vaflags = VA_EXCLUSIVE;
    error = VNOP_MKDIR(parent, &vp, &cn, &va, ctx);
    if (!error) *created = 1;
    if (vp != NULLVP) vnode_put(vp);
    vnode_put(parent);
    return error;
}

/* Native launchd override, not a FairPlay implementation or signature bypass.
 * Preserve existing overrides through a backed-up binary-plist transaction.
 * The small XML fallback is removable only on exact match. Both persist
 * across boot-arg changes. */
static const char fp_override[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<!-- a04br3 development FairPlay service isolation -->\n"
    "<plist version=\"1.0\"><dict>"
    "<key>com.apple.fairplayd.H2</key><true/>"
    "</dict></plist>\n";
static int
remove_fairplay_override(vnode_t vp, vfs_context_t ctx)
{
    vnode_t parent = NULLVP;
    int error = vnode_lookup("/private/var/db/com.apple.xpc.launchd", VNODE_LOOKUP_NOFOLLOW, &parent, ctx);
    if (error) return error;
    if (!vnode_isdir(parent) || vnode_mount(parent) != vnode_mount(rootvnode)) {
        vnode_put(parent); return ENOTDIR;
    }
    struct componentname cn;
    bzero(&cn, sizeof(cn));
    cn.cn_nameiop = DELETE;
    cn.cn_flags = ISLASTCN;
    cn.cn_context = ctx;
    cn.cn_nameptr = "disabled.plist";
    cn.cn_namelen = 14;
    error = VNOP_REMOVE(parent, vp, &cn, 0, ctx);
    vnode_put(parent);
    return error;
}
static int
prepare_fairplay(unsigned restore)
{
    const char *directory = "/private/var/db/com.apple.xpc.launchd";
    const char *path = "/private/var/db/com.apple.xpc.launchd/disabled.plist";
    vfs_context_t ctx = vfs_context_kernel();
    vnode_t vp = NULLVP;
    int created = 0, error = prepare_directory(directory, &created);
    if (error) return error;
    error = vnode_lookup(path, VNODE_LOOKUP_NOFOLLOW, &vp, ctx);
    if (!error) {
        struct vnode_attr va;
        VATTR_INIT(&va);
        VATTR_WANTED(&va, va_uid);
        VATTR_WANTED(&va, va_nlink);
        VATTR_WANTED(&va, va_data_size);
        error = vnode_getattr(vp, &va, ctx);
        if (!error && (!vnode_isreg(vp) || vnode_mount(vp) != vnode_mount(rootvnode) ||
            !VATTR_IS_SUPPORTED(&va, va_uid) || va.va_uid != 0 ||
            !VATTR_IS_SUPPORTED(&va, va_nlink) || va.va_nlink != 1 ||
            !VATTR_IS_SUPPORTED(&va, va_data_size) || va.va_data_size != sizeof(fp_override)-1))
            error = EEXIST;
        char data[sizeof(fp_override)];
        int resid = 0;
        if (!error) error = vn_rdwr(UIO_READ, vp, data, sizeof(fp_override)-1, 0,
            UIO_SYSSPACE, 0, vfs_context_ucred(ctx), &resid, vfs_context_proc(ctx));
        if (!error && (resid || memcmp(data, fp_override, sizeof(fp_override)-1))) error = EEXIST;
        if (!error && restore) error = remove_fairplay_override(vp, ctx);
        if (error == EEXIST) error = merge_existing_fairplay(vp, restore, ctx);
        vnode_put(vp);
        return error;
    }
    if (error != ENOENT || restore) return error == ENOENT ? 0 : error;
    error = vnode_open(path, FWRITE | O_CREAT | O_EXCL | O_NOFOLLOW, 0644, 0, &vp, ctx);
    if (error) return error;
    int resid = 0;
    error = vn_rdwr(UIO_WRITE, vp, (caddr_t)fp_override, sizeof(fp_override)-1, 0,
        UIO_SYSSPACE, IO_SYNC, vfs_context_ucred(ctx), &resid, vfs_context_proc(ctx));
    if (!error && resid) error = EIO;
    int close_error = vn_close(vp, FWRITE, ctx);
    if (error || close_error) {
        int cleanup = remove_fairplay_override(vp, ctx);
        if (cleanup) printf("A04BR3 ROOT: incomplete FairPlay override cleanup error=%d\n", cleanup);
    }
    vnode_put(vp);
    return error ? error : close_error;
}

static int prepare_audio(unsigned restore)
{
    const char *active = "/Library/Audio/Plug-Ins/HAL/VirtualAudio.plugin";
    const char *disabled = "/Library/Audio/Plug-Ins/HAL.disabled/VirtualAudio.plugin";
    vfs_context_t ctx = vfs_context_kernel();
    int created = 0;
    int error = prepare_directory(restore ? "/Library/Audio/Plug-Ins/HAL" :
        "/Library/Audio/Plug-Ins/HAL.disabled", &created);
    if(error)return error;
    vnode_t vp = NULLVP;
    error = vnode_lookup(restore ? disabled : active, VNODE_LOOKUP_NOFOLLOW, &vp, ctx);
    if(error == ENOENT) {
        error = vnode_lookup(restore ? active : disabled, VNODE_LOOKUP_NOFOLLOW, &vp, ctx);
        if(error)return error;
        error = vnode_isdir(vp) && vnode_mount(vp)==vnode_mount(rootvnode) ? 0 : ENOTDIR;
        vnode_put(vp);return error;
    }
    if(error)return error;
    error = vnode_isdir(vp) && vnode_mount(vp)==vnode_mount(rootvnode) ? 0 : ENOTDIR;
    vnode_put(vp);if(error)return error;
    extern int a04br3_audio_bundle_move(unsigned restore);
    return a04br3_audio_bundle_move(restore);
}

void
a04br3_root_prepare(void)
{
    unsigned enabled = 0, full = 0, writable = 0, emmc = 0;
    PE_parse_boot_argn("a04br3.prepare_root", &enabled, sizeof(enabled));
    PE_parse_boot_argn("a04br3.full_root", &full, sizeof(full));
    PE_parse_boot_argn("a04br3.root_rw", &writable, sizeof(writable));
    PE_parse_boot_argn("a04br3.emmc_rw", &emmc, sizeof(emmc));
    if (enabled != 1 || full != 1 || writable != 1 || emmc != 1) return;
    if (strcmp(vfs_statfs(vnode_mount(rootvnode))->f_fstypename, "hfs"))
        panic("A04BR3 root prepare requires development HFS root");
    static const char *paths[] = {
        "/private/var/containers",
        "/private/var/containers/Bundle",
        "/private/var/containers/Bundle/Application",
        "/private/var/containers/Data",
        "/private/var/containers/Data/Application",
        "/private/var/containers/Data/System",
        "/private/var/containers/Shared",
        "/private/var/containers/Shared/SystemGroup",
        "/private/var/containers/Temp",
    };
    for (unsigned i = 0; i < sizeof(paths)/sizeof(paths[0]); i++) {
        int created = 0;
        int error = prepare_directory(paths[i], &created);
        printf("A04BR3 ROOT: %s %s error=%d\n", paths[i], created ? "created" : "existing", error);
        if (error) panic("A04BR3 root prepare failed: %s error=%d", paths[i], error);
    }
    /* Known product transition only; preserve the stale cache before MG starts. */
    int mg_error = prepare_mobilegestalt();
    printf("A04BR3 ROOT: MobileGestalt identity cache preparation error=%d\n", mg_error);
    unsigned fairplay = 0;
    PE_parse_boot_argn("a04br3.fp", &fairplay, sizeof(fairplay));
    if (fairplay == 1 || fairplay == 2) {
        int error = prepare_fairplay(fairplay == 2);
        printf("A04BR3 ROOT: FairPlay service %s error=%d (native unavailable-service experiment)\n",
            fairplay == 2 ? "restored" : "disabled", error);
    }
    unsigned audio = 0;
    PE_parse_boot_argn("a04br3.no_va", &audio, sizeof(audio));
    if(audio==1 || audio==2) {
        int error=prepare_audio(audio==2);
        printf("A04BR3 ROOT: VirtualAudio %s error=%d\n",audio==2?"restored":"disabled",error);
        if(error)panic("A04BR3 VirtualAudio disposition failed %d",error);
    }

}
#endif
