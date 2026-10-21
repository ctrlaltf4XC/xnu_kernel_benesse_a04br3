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
prepare_uart_console(void)
{
    unsigned enabled = 0;
    PE_parse_boot_argn("a04br3.uart_console", &enabled, sizeof(enabled));
    if (enabled != 1) return 0;
    /* Build failure for an absent generated include is intentional: never ship
     * a placeholder or download a program during boot. */
    if (sizeof(a04br3_console_payload) == 0 ||
        sizeof(a04br3_console_payload) > 4U * 1024U * 1024U) return EFBIG;

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

    error = vnode_lookup(A04_CONSOLE_DIR "/" A04_CONSOLE_LEAF,
        VNODE_LOOKUP_NOFOLLOW, &target, ctx);
    if (!error) error = a04_console_regular(target, ctx);
    else if (error == ENOENT) error = 0;
    if (error) goto out;

    /* Do not truncate until the opened inode has passed the no-link/owner
     * checks. A safe leftover temporary file from an interrupted boot can be
     * rewritten; the previous executable remains untouched until rename. */
    error = vnode_open(A04_CONSOLE_DIR "/" A04_CONSOLE_TEMP,
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
    if (!error) error = vn_rdwr(UIO_WRITE, temp, (caddr_t)a04br3_console_payload,
        (int)sizeof(a04br3_console_payload), 0, UIO_SYSSPACE, IO_SYNC,
        vfs_context_ucred(ctx), &resid, vfs_context_proc(ctx));
    if (!error && resid) error = EIO;
    if (error) goto out;

    /* Verify the whole generated payload, using bounded kernel stack space. */
    unsigned char check[512];
    for (size_t offset = 0; offset < sizeof(a04br3_console_payload);) {
        size_t bytes = sizeof(a04br3_console_payload) - offset;
        if (bytes > sizeof(check)) bytes = sizeof(check);
        resid = 0;
        error = vn_rdwr(UIO_READ, temp, (caddr_t)check, (int)bytes, (off_t)offset,
            UIO_SYSSPACE, 0, vfs_context_ucred(ctx), &resid, vfs_context_proc(ctx));
        if (!error && (resid || memcmp(check, a04br3_console_payload + offset, bytes)))
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
    a04_console_component(&from, A04_CONSOLE_TEMP, DELETE, ctx);
    a04_console_component(&to, A04_CONSOLE_LEAF, RENAME, ctx);
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
        a04_console_component(&cn, A04_CONSOLE_TEMP, DELETE, ctx);
        int cleanup = VNOP_REMOVE(parent, temp, &cn, 0, ctx);
        if (cleanup) printf("A04BR3 ROOT: UART wrapper temp cleanup error=%d\n", cleanup);
    }
    if (temp != NULLVP) vnode_put(temp);
    if (target != NULLVP) vnode_put(target);
    if (parent != NULLVP) vnode_put(parent);
    printf("A04BR3 ROOT: UART wrapper installed=%d bytes=%lu error=%d\n",
        installed, (unsigned long)sizeof(a04br3_console_payload), error);
    return error;
}
#endif
