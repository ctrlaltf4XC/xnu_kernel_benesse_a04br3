/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Mach-side bounded live register snapshots; see BSD iterator counterpart.
 */
#if defined(ARM64_BOARD_CONFIG_A04BR3)
#include <kern/task.h>
#include <kern/thread.h>
#include <arm/thread.h>
#include <vm/vm_shared_region.h>
#include <kern/printf.h>
void a04br3_sample_task(const char *, int, task_t);
void
a04br3_sample_task(const char *name, int pid, task_t task)
{
    struct { uint64_t pc, lr, sp; } rows[16];
    unsigned count = 0;
    int total;
    uint64_t header, slide = 0;
    vm_shared_region_t sr = vm_shared_region_get(task);
    if (sr != NULL) {
        slide = sr->sr_slide;
        vm_shared_region_deallocate(sr);
    }
    /* Saved user registers are approximate snapshots of live threads.
     * Only collect fixed-size register values; never walk a user stack here. */
    task_lock(task);
    total = task->thread_count;
    header = task->mach_header_vm_address;
    thread_t thread;
    queue_iterate(&task->threads, thread, thread_t, task_threads) {
        if (count == 16) break;
        arm_saved_state_t *ss = find_user_regs(thread);
        if (ss == NULL || !is_saved_state64(ss)) continue;
        rows[count].pc = get_saved_state_pc(ss);
        rows[count].lr = get_saved_state_lr(ss);
        rows[count].sp = get_saved_state_sp(ss);
        count++;
    }
    task_unlock(task);
    printf("A04BR3_SAMPLE %s[%d] threads=%d mh=0x%llx slide=0x%llx\n",
        name, pid, total, header, slide);
    for (unsigned i = 0; i < count; i++) {
        printf("A04BR3_SAMPLE %s[%d] t%u pc=0x%llx lr=0x%llx sp=0x%llx\n",
            name, pid, i, rows[i].pc, rows[i].lr, rows[i].sp);
    }
}
#endif
