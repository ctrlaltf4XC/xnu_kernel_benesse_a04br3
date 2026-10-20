/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Mach-side bounded live register snapshots; see BSD iterator counterpart.
 */
#if defined(ARM64_BOARD_CONFIG_A04BR3)
#include <kern/task.h>
#include <kern/thread.h>
#include <arm/thread.h>
#include <vm/vm_shared_region.h>
#include <vm/vm_map.h>
#include <kern/misc_protos.h>
#include <libkern/libkern.h>
void a04br3_sample_task(const char *, int, task_t);
void
a04br3_sample_task(const char *name, int pid, task_t task)
{
    struct { uint64_t pc, lr, sp, fp; } rows[16];
    unsigned count = 0;
    int total;
    uint64_t header, slide = 0;
    vm_shared_region_t sr = vm_shared_region_get(task);
    if (sr != NULL) {
        slide = sr->sr_slide;
        vm_shared_region_deallocate(sr);
    }
    /* Saved user registers are approximate snapshots of live threads.
     * Collect fixed-size registers under the lock; user stacks are read below. */
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
        rows[count].fp = get_saved_state_fp(ss);
        count++;
    }
    task_unlock(task);
    printf("A04BR3_SAMPLE %s[%d] threads=%d mh=0x%llx slide=0x%llx\n",
        name, pid, total, header, slide);
    for (unsigned i = 0; i < count; i++) {
        printf("A04BR3_SAMPLE %s[%d] t%u pc=0x%llx lr=0x%llx sp=0x%llx\n",
            name, pid, i, rows[i].pc, rows[i].lr, rows[i].sp);
        /* Only window-server processes need remote user-stack reads. Never
         * fault user memory while holding the task lock. Saved FP/PC can race
         * running threads: these are approximate live samples, not crashes. */
        if (!strcmp(name, "SpringBoard") || !strcmp(name, "backboardd")) {
            uint64_t fp = rows[i].fp;
            for (unsigned depth = 0; depth < 16 && fp && !(fp & 7); depth++) {
                uint64_t frame[2];
                if (vm_map_read_user(get_task_map(task), fp, frame, sizeof(frame)) != KERN_SUCCESS) break;
                printf("A04BR3_SAMPLE %s[%d] t%u bt%u=0x%llx\n",
                    name, pid, i, depth, frame[1]);
                if (frame[0] <= fp) break;
                fp = frame[0];
            }
        }
    }
}
/* Adapted from ios-sweet 0043 console crash log. Return only the FP;
 * BSD owns fault-recoverable copyin and the bounded user stack walk. */
uint64_t a04br3_crash_registers(const char *, int, int, uint64_t, uint64_t);
uint64_t
a04br3_crash_registers(const char *name, int pid, int sig,
    uint64_t code, uint64_t subcode)
{
    task_t task = current_task();
    arm_saved_state_t *ss = find_user_regs(current_thread());
    uint64_t slide = 0;
    vm_shared_region_t sr = vm_shared_region_get(task);
    if (sr != NULL) {
        slide = sr->sr_slide;
        vm_shared_region_deallocate(sr);
    }
    printf("A04BR3_CRASH %s[%d] sig=%d code=0x%llx sub=0x%llx slide=0x%llx mh=0x%llx\n",
        name, pid, sig, code, subcode, slide, task->mach_header_vm_address);
    if (ss == NULL || !is_saved_state64(ss)) return 0;
    uint64_t fp = get_saved_state_fp(ss);
    printf("A04BR3_CRASH %s[%d] pc=0x%llx lr=0x%llx sp=0x%llx fp=0x%llx far=0x%llx\n",
        name, pid, get_saved_state_pc(ss), get_saved_state_lr(ss),
        get_saved_state_sp(ss), fp, (uint64_t)saved_state64(ss)->far);
    return fp;
}
#endif
