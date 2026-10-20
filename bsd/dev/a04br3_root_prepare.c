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
#include <pexpert/pexpert.h>

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
    unsigned audio = 0;
    PE_parse_boot_argn("a04br3.no_va", &audio, sizeof(audio));
    if(audio==1 || audio==2) {
        int error=prepare_audio(audio==2);
        printf("A04BR3 ROOT: VirtualAudio %s error=%d\n",audio==2?"restored":"disabled",error);
        if(error)panic("A04BR3 VirtualAudio disposition failed %d",error);
    }

}
#endif
