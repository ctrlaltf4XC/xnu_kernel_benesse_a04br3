/*
 * Compile Apple's HFS kext sources inside A04BR3 the way the kext is built:
 * with the kext (KPI) view of the kernel headers, and with only HFS's own
 * kext-config.h deciding features. The kernel's -DCONFIG_* options are not
 * visible to a kext build, so the ones HFS tests are cleared first. A04BR3
 * has no AppleKeyStore, so content protection stays off.
 */
#ifndef A04BR3_HFS_COMPAT_H
#define A04BR3_HFS_COMPAT_H
#undef BSD_KERNEL_PRIVATE
#undef XNU_KERNEL_PRIVATE
#undef CONFIG_PROTECT
#undef CONFIG_SEARCHFS
#undef CONFIG_HFS_DIRLINK
#undef CONFIG_SECLUDED_RENAME
#undef CONFIG_MACF
#undef CONFIG_HFS_RBTREE
#undef QUOTA
#undef NAMEDSTREAMS
#undef FIFO
#undef HFS_COMPRESSION
#include "core/kext-config.h"

/* Built in, not a loadable kext (see a04br3_hfs_iokit.c). */
#define OSKextGetCurrentLoadTag a04br3_hfs_current_load_tag
#endif
