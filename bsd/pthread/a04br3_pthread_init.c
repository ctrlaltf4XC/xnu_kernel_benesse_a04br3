/* Built-in provider: Apple libpthread-514.100.2, paired with xnu-8796.141.3. */
#if defined(ARM64_BOARD_CONFIG_A04BR3)
#include "a04br3_pthread_compat.h"
#include "a04br3_libpthread/kern/kern_init.c"

/* Keep the provider's full layout within the kernel's reserved storage. */
_Static_assert(sizeof(struct ksyn_waitq_element) == 56,
    "update A04BR3 ksyn storage in sys/pthread_internal.h");

/* Restore the startup interface after compiling the provider's kext code. */
#define XNU_KERNEL_PRIVATE 1
#include <kern/startup.h>
#include <pexpert/pexpert.h>

__startup_func
static void
a04br3_pthread_register(void)
{
    kern_return_t kr = a04br3_pthread_start(NULL, NULL);
    if (kr != KERN_SUCCESS || a04br3_pthread_kern == NULL) {
        panic("A04BR3: pthread provider registration failed: %d", kr);
    }
    kprintf("A04BR3 pthread: libpthread-514.100.2 registered\n");
}
STARTUP(EARLY_BOOT, STARTUP_RANK_SECOND, a04br3_pthread_register);
#endif /* ARM64_BOARD_CONFIG_A04BR3 */
