/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Source-built, signed PID1 wrapper installation before load_init_program().
 * Included after prepare_directory() in a04br3_root_prepare.c.
 */
#ifndef A04BR3_CONSOLE_INSTALL_H
#define A04BR3_CONSOLE_INSTALL_H
#include "a04br3_console_payload.inc"

#define A04_CONSOLE_DIR "/usr/appleinternal/sbin"
#define A04_CONSOLE_LEAF "launchd.a04br3"
#define A04_CONSOLE_TEMP ".launchd.a04br3.new"
#define A04_CAPTURE_LEAF "a04-fb-capture"
#define A04_CAPTURE_TEMP ".a04-fb-capture.new"
#define A04_UI_TEST_LEAF "a04-ui-test"
#define A04_UI_TEST_TEMP ".a04-ui-test.new"
#define A04_KEYBOARD_STATE_LEAF "a04br3-keyboard-state"
#define A04_KEYBOARD_STATE_TEMP ".a04br3-keyboard-state.new"
#define A04_ROOT_TWEAKS_LEAF "a04-root-tweaks"
#define A04_ROOT_TWEAKS_TEMP ".a04-root-tweaks.new"

static int
a04_console_regular(vnode_t vp, vfs_context_t ctx)
{
    struct vnode_attr va;
    VATTR_INIT(&va);
    VATTR_WANTED(&va, va_uid);
    VATTR_WANTED(&va, va_nlink);
    int error = vnode_getattr(vp, &va, ctx);
    if (error) return error;
    if (!vnode_isreg(vp) || vnode_mount(vp) != vnode_mount(rootvnode) ||
        !VATTR_IS_SUPPORTED(&va, va_uid) || va.va_uid != 0 ||
        !VATTR_IS_SUPPORTED(&va, va_nlink) || va.va_nlink != 1) return EEXIST;
    return 0;
}

static void
a04_console_component(struct componentname *cn, char *leaf, int operation,
    vfs_context_t ctx)
{
    bzero(cn, sizeof(*cn));
    cn->cn_nameiop = operation;
    cn->cn_flags = ISLASTCN;
    cn->cn_context = ctx;
    cn->cn_nameptr = leaf;
    cn->cn_namelen = (int)strlen(leaf);
}

static int
a04_console_install_program(const unsigned char *payload, size_t payload_bytes,
    char *leaf, char *temporary)
{
    /* No placeholder or boot-time download; each executable is verified and
     * renamed independently. All callers use the fixed names below. */
    if (payload_bytes == 0 || payload_bytes > 4U * 1024U * 1024U) return EFBIG;
    char target_path[128], temp_path[128];
    if (snprintf(target_path, sizeof(target_path), "%s/%s", A04_CONSOLE_DIR, leaf) >= (int)sizeof(target_path) ||
        snprintf(temp_path, sizeof(temp_path), "%s/%s", A04_CONSOLE_DIR, temporary) >= (int)sizeof(temp_path)) return ENAMETOOLONG;

    static const char *directories[] = {"/usr", "/usr/appleinternal", A04_CONSOLE_DIR};
    vfs_context_t ctx = vfs_context_kernel();
    vnode_t parent = NULLVP, target = NULLVP, temp = NULLVP;
    int error = 0, opened = 0, installed = 0;
    for (unsigned i = 0; i < sizeof(directories) / sizeof(directories[0]); ++i) {
        int created = 0;
        error = prepare_directory(directories[i], &created);
        if (error) goto out;
        vnode_t directory = NULLVP;
        error = vnode_lookup(directories[i], VNODE_LOOKUP_NOFOLLOW, &directory, ctx);
        if (error) goto out;
        struct vnode_attr va;
        VATTR_INIT(&va);
        VATTR_WANTED(&va, va_uid);
        VATTR_WANTED(&va, va_mode);
        error = vnode_getattr(directory, &va, ctx);
        if (!error && (!vnode_isdir(directory) ||
            vnode_mount(directory) != vnode_mount(rootvnode) ||
            !VATTR_IS_SUPPORTED(&va, va_uid) || va.va_uid != 0 ||
            !VATTR_IS_SUPPORTED(&va, va_mode) || (va.va_mode & 0022))) error = EACCES;
        if (i + 1 == sizeof(directories) / sizeof(directories[0]) && !error)
            parent = directory;
        else vnode_put(directory);
        if (error) goto out;
    }

    error = vnode_lookup(target_path,
        VNODE_LOOKUP_NOFOLLOW, &target, ctx);
    if (!error) error = a04_console_regular(target, ctx);
    else if (error == ENOENT) error = 0;
    if (error) goto out;

    /* Do not truncate until the opened inode has passed the no-link/owner
     * checks. A safe leftover temporary file from an interrupted boot can be
     * rewritten; the previous executable remains untouched until rename. */
    error = vnode_open(temp_path,
        FWRITE | O_CREAT | O_NOFOLLOW, 0600, 0, &temp, ctx);
    if (error) goto out;
    opened = 1;
    error = a04_console_regular(temp, ctx);
    if (error) goto out;
    struct vnode_attr va;
    VATTR_INIT(&va);
    VATTR_SET(&va, va_uid, 0);
    VATTR_SET(&va, va_gid, 0);
    VATTR_SET(&va, va_mode, 0600);
    VATTR_SET(&va, va_data_size, 0);
    error = vnode_setattr(temp, &va, ctx);
    int resid = 0;
    if (!error) error = vn_rdwr(UIO_WRITE, temp, (caddr_t)payload,
        (int)payload_bytes, 0, UIO_SYSSPACE, IO_SYNC,
        vfs_context_ucred(ctx), &resid, vfs_context_proc(ctx));
    if (!error && resid) error = EIO;
    if (error) goto out;

    /* Verify the whole generated payload, using bounded kernel stack space. */
    unsigned char check[512];
    for (size_t offset = 0; offset < payload_bytes;) {
        size_t bytes = payload_bytes - offset;
        if (bytes > sizeof(check)) bytes = sizeof(check);
        resid = 0;
        error = vn_rdwr(UIO_READ, temp, (caddr_t)check, (int)bytes, (off_t)offset,
            UIO_SYSSPACE, 0, vfs_context_ucred(ctx), &resid, vfs_context_proc(ctx));
        if (!error && (resid || memcmp(check, payload + offset, bytes)))
            error = EIO;
        if (error) goto out;
        offset += bytes;
    }
    VATTR_INIT(&va);
    VATTR_SET(&va, va_mode, 0755);
    error = vnode_setattr(temp, &va, ctx);
    if (!error) error = VNOP_FSYNC(temp, MNT_WAIT, ctx);
    if (error) goto out;
    error = vn_close(temp, FWRITE, ctx);
    opened = 0;
    if (error) goto out;

    struct componentname from, to;
    a04_console_component(&from, temporary, DELETE, ctx);
    a04_console_component(&to, leaf, RENAME, ctx);
    /* Fixed same-directory rename in the pre-launch, root-owned hierarchy.
     * Source and target iocounts stay with this caller. */
    error = VNOP_RENAME(parent, temp, &from, parent, target, &to, ctx);
    if (!error) {
        installed = 1;
        error = VNOP_FSYNC(parent, MNT_WAIT, ctx);
    }
out:
    if (opened) {
        int closed = vn_close(temp, FWRITE, ctx);
        if (!error) error = closed;
    }
    /* Only remove a temporary inode whose ownership/type checks succeed. */
    if (!installed && temp != NULLVP && parent != NULLVP &&
        a04_console_regular(temp, ctx) == 0) {
        struct componentname cn;
        a04_console_component(&cn, temporary, DELETE, ctx);
        int cleanup = VNOP_REMOVE(parent, temp, &cn, 0, ctx);
        if (cleanup) printf("A04BR3 ROOT: source program %s temp cleanup error=%d\n", leaf, cleanup);
    }
    if (temp != NULLVP) vnode_put(temp);
    if (target != NULLVP) vnode_put(target);
    if (parent != NULLVP) vnode_put(parent);
    printf("A04BR3 ROOT: source program %s installed=%d bytes=%lu error=%d\n",
        leaf, installed, (unsigned long)payload_bytes, error);
    return error;
}
static int
prepare_uart_console(void)
{
    unsigned enabled = 0;
    if (!PE_parse_boot_argn("a04br3.console", &enabled, sizeof(enabled)))
        PE_parse_boot_argn("a04br3.uart_console", &enabled, sizeof(enabled));
    if (enabled != 1) return 0;
    int error = a04_console_install_program(a04br3_console_payload,
        sizeof(a04br3_console_payload), A04_CONSOLE_LEAF, A04_CONSOLE_TEMP);
    if (error) return error;
#if defined(A04BR3_CAPTURE_PAYLOAD_PRESENT)
    /* Capture failure is explicit but must not turn an optional screenshot
     * helper into a new reason to panic before native launchd. */
    int capture_error = a04_console_install_program(a04br3_fb_capture_payload,
        sizeof(a04br3_fb_capture_payload), A04_CAPTURE_LEAF, A04_CAPTURE_TEMP);
    if (capture_error) printf("A04BR3 ROOT: optional framebuffer capture installation failed error=%d\n", capture_error);
#endif
#if defined(A04BR3_UI_TEST_PAYLOAD_PRESENT)
    int ui_error = a04_console_install_program(a04br3_ui_test_payload,
        sizeof(a04br3_ui_test_payload), A04_UI_TEST_LEAF, A04_UI_TEST_TEMP);
    if (ui_error) printf("A04BR3 ROOT: optional UI test installation failed error=%d\n", ui_error);
#endif
#if defined(A04BR3_KEYBOARD_STATE_PAYLOAD_PRESENT)
    /* Install only: observation requires an explicit root invocation later. */
    int keyboard_error = a04_console_install_program(a04br3_keyboard_state_payload,
        sizeof(a04br3_keyboard_state_payload), A04_KEYBOARD_STATE_LEAF, A04_KEYBOARD_STATE_TEMP);
    if (keyboard_error) printf("A04BR3 ROOT: optional keyboard state installation failed error=%d\n", keyboard_error);
#endif
#if defined(A04BR3_ROOT_TWEAKS_PAYLOAD_PRESENT)
    /* Install only: the development Setup skip runs on explicit root request. */
    int tweaks_error = a04_console_install_program(a04br3_root_tweaks_payload,
        sizeof(a04br3_root_tweaks_payload), A04_ROOT_TWEAKS_LEAF, A04_ROOT_TWEAKS_TEMP);
    if (tweaks_error) printf("A04BR3 ROOT: optional root tweaks installation failed error=%d\n", tweaks_error);
#endif
    return 0;
}
#endif
