/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Bounded bring-up snapshots inspired by ios-sweet patch 0054.
 * No user memory reads or console output while holding task locks.
 */
#if defined(ARM64_BOARD_CONFIG_A04BR3)
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/proc_internal.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <pexpert/pexpert.h>

extern task_t proc_task(proc_t);
extern void a04br3_sample_task(const char *, int, task_t);
extern int hz;
void a04br3_sample_start(void);

static int
sample_proc(proc_t p, __unused void *arg)
{
    char name[32];
    proc_name(proc_pid(p), name, sizeof(name));
    if (strcmp(name, "launchd") && strcmp(name, "backboardd") &&
        strcmp(name, "SpringBoard") && strcmp(name, "logd") &&
        strcmp(name, "notifyd") && strcmp(name, "UserEventAgent")) {
        return PROC_RETURNED;
    }
    task_t task = proc_task(p);
    if (task == TASK_NULL) return PROC_RETURNED;
    a04br3_sample_task(name, proc_pid(p), task);
    return PROC_RETURNED;
}

static void
sample_thread(__unused void *arg, __unused wait_result_t wr)
{
    static int chan;
    /* Long intervals, finite output: this is a diagnostic, not a monitor. */
    (void)tsleep(&chan, PZERO, "a04sample", 120 * hz);
    for (unsigned i = 0; i < 3; i++) {
        printf("A04BR3_SAMPLE begin %u\n", i);
        proc_iterate(PROC_ALLPROCLIST, sample_proc, NULL, NULL, NULL);
        printf("A04BR3_SAMPLE end %u\n", i);
        if (i < 2) (void)tsleep(&chan, PZERO, "a04sample", 120 * hz);
    }
}

void
a04br3_sample_start(void)
{
    unsigned enabled = 0;
    thread_t thread;
    if (!PE_parse_boot_argn("a04br3.full_root", &enabled, sizeof(enabled)) || enabled != 1) return;
    if (kernel_thread_start(sample_thread, NULL, &thread) == KERN_SUCCESS) {
        thread_deallocate(thread);
    }
}
#endif
