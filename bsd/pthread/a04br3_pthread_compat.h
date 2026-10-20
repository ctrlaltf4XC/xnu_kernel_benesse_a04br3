/*
 * Compile the matching libpthread provider with its kext view of BSD headers.
 * It owns its full ksyn_waitq_element layout; the kernel's opaque counterpart
 * reserves this layout. All communication with private BSD state uses callbacks.
 */
#ifndef A04BR3_PTHREAD_COMPAT_H
#define A04BR3_PTHREAD_COMPAT_H
#undef BSD_KERNEL_PRIVATE
#undef XNU_KERNEL_PRIVATE
#define __PTHREAD_EXPOSE_INTERNALS__ 1
#define NEEDS_SCHED_CALL_T 1

/* The seven-argument kext API forwards to the eight-argument kernel vm_fault. */
#define vm_fault vm_fault_external

/* Avoid collisions with the kernel side of the callback interface. */
#define pthread_kern a04br3_pthread_kern
#define current_uthread a04br3_pthread_current_uthread
#define pthread_start a04br3_pthread_start
#define pthread_stop a04br3_pthread_stop

/* os/log passes &__dso_handle as the containing Mach-O image address. */
extern void *a04br3_pthread_log_image __asm("__mh_execute_header");
#define __dso_handle a04br3_pthread_log_image
#endif
