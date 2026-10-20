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
extern int serial_getc(void);
void a04br3_sample_start(void);

static int
sample_proc(proc_t p, __unused void *arg)
{
    char name[32];
    proc_name(proc_pid(p), name, sizeof(name));
    if (strcmp(name, "launchd") && strcmp(name, "backboardd") &&
        strcmp(name, "SpringBoard") && strcmp(name, "logd") &&
        strcmp(name, "notifyd") && strcmp(name, "UserEventAgent") &&
        strcmp(name, "containermanager") && strcmp(name, "containermanagerd") &&
        strcmp(name, "lsd") && strncmp(name, "datamigrat", 10) != 0 && strncmp(name, "migrationplugin", 15) != 0) {
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
    unsigned uart = 0, elapsed = 0, next = 120, rounds = 0, manual = 0, last = 0;
    PE_parse_boot_argn("a04br3.sample_uart", &uart, sizeof(uart));
    if (uart == 1) printf("A04BR3_SAMPLE UART: Ctrl-T requests a snapshot (10s rate limit, 16 requests)\n");
    for (;;) {
        (void)tsleep(&chan, PZERO, "a04sample", hz);
        elapsed++;
        bool scheduled = rounds < 12 && elapsed >= next;
        bool requested = uart == 1 && serial_getc() == 20 && manual < 16 && elapsed - last >= 10;
        if (scheduled || requested) {
            unsigned sequence = scheduled ? rounds++ : 1000 + manual++;
            printf("A04BR3_SAMPLE begin %u elapsed=%u s\n", sequence, elapsed);
            proc_iterate(PROC_ALLPROCLIST, sample_proc, NULL, NULL, NULL);
            printf("A04BR3_SAMPLE end %u\n", sequence);
            if (scheduled) next += 600;
            if (requested) last = elapsed;
        }
        if (rounds == 12 && (uart != 1 || manual == 16)) break;
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
