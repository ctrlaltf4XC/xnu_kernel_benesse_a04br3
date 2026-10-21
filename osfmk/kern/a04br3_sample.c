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
#include <string.h>
#include <kern/kalloc.h>
#include <mach/vm_region.h>
/* dyld_image_info is three native-width user pointers/words on arm64.
 * This is an approximate bounded snapshot, not a task stop or trusted map. */
static void a04br3_sample_images(const char *name,int pid,task_t task)
{
    struct { uint32_t version,count;uint64_t array; } info;
    vm_map_t map=get_task_map(task);
    uint64_t at=task_get_all_image_info_addr(task);
    if(!at || vm_map_read_user(map,at,&info,sizeof(info))!=KERN_SUCCESS ||
        !info.version || !info.array || info.count>4096)return;
    unsigned limit=info.count<1024?info.count:1024, printed=0;
    for(unsigned offset=0;offset<limit && printed<32;offset+=16){
        struct {uint64_t base,path,modified;} images[16];
        unsigned n=limit-offset<16?limit-offset:16;
        if(vm_map_read_user(map,info.array+(uint64_t)offset*sizeof(images[0]),
            images,n*sizeof(images[0]))!=KERN_SUCCESS)break;
        for(unsigned i=0;i<n && printed<32;i++){
            if(images[i].base<0x100000000ULL || images[i].base>=0x180000000ULL || !images[i].path)continue;
            char path[192];bzero(path,sizeof(path));
            unsigned bytes=PAGE_SIZE-(images[i].path&PAGE_MASK);
            if(bytes>sizeof(path)-1)bytes=sizeof(path)-1;
            if(vm_map_read_user(map,images[i].path,path,bytes)!=KERN_SUCCESS)continue;
            printf("A04BR3_IMAGE %s[%d] base=0x%llx path=%s\n",name,pid,images[i].base,path);
            printed++;
        }
    }
}
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
    if(strnstr(name,"migrationplugin",32))a04br3_sample_images(name,pid,task);
    printf("A04BR3_SAMPLE %s[%d] threads=%d mh=0x%llx slide=0x%llx\n",
        name, pid, total, header, slide);
    for (unsigned i = 0; i < count; i++) {
        printf("A04BR3_SAMPLE %s[%d] t%u pc=0x%llx lr=0x%llx sp=0x%llx\n",
            name, pid, i, rows[i].pc, rows[i].lr, rows[i].sp);
        /* Only window-server processes need remote user-stack reads. Never
         * fault user memory while holding the task lock. Saved FP/PC can race
         * running threads: these are approximate live samples, not crashes. */
        if (!strcmp(name, "SpringBoard") || !strcmp(name, "backboardd") ||
            !strcmp(name, "containermanager") || !strcmp(name, "containermanagerd") ||
            !strcmp(name, "lsd") || !strcmp(name,"installd") ||
            !strcmp(name,"installcoordinationd") || !strcmp(name,"appstored") ||
            !strcmp(name,"locationd") || strnstr(name, "datamigrat", 32) || strnstr(name, "migrationplugin", 32)) {
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
/* ios-sweet 0051 adaptation: bounded first-abort heap text for the actual
 * exception reason, which libc++abi does not put in os_reason. */
static void
a04br3_crash_strings(const char *name)
{
    static unsigned scans[6];
    unsigned bucket = !strcmp(name, "backboardd") ? 0 : !strcmp(name,"lsd") ? 1 :
        !strcmp(name,"installd") ? 2 : !strcmp(name,"SpringBoard") ? 4 : !strcmp(name,"Setup") ? 5 : 3;
    if (scans[bucket]++) return;
    char *buf = kalloc_data(65536, Z_WAITOK);
    if (!buf) return;
    vm_map_t map = current_map();
    vm_map_offset_t addr = 0;
    uint64_t scanned = 0;
    unsigned lines = 0;
    while (lines < 256 && scanned < (64ULL << 20)) {
        vm_map_size_t size = 0;
        natural_t depth = 0;
        vm_region_submap_info_data_64_t info;
        mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_V2_COUNT_64;
        if (vm_map_region_recurse_64(map, &addr, &size, &depth,
            (vm_region_submap_info_64_t)&info, &count) != KERN_SUCCESS) break;
        vm_map_offset_t base = addr;
        if (!size || addr + size <= addr) break;
        addr += size;
        if (info.is_submap ||
            (info.protection & (VM_PROT_READ | VM_PROT_WRITE)) != (VM_PROT_READ | VM_PROT_WRITE) ||
            info.user_tag < 1 || info.user_tag > 11) continue;
        for (vm_map_offset_t off = 0; off < size && lines < 256 && scanned < (64ULL << 20); off += 65536) {
            size_t len = size - off < 65536 ? (size_t)(size - off) : 65536;
            scanned += len; /* Bound attempts too, including unreadable pages. */
            if (vm_map_read_user(map, base + off, buf, len) != KERN_SUCCESS) continue;
            size_t start = 0;
            unsigned spaces = 0;
            for (size_t i = 0; i <= len && lines < 256; i++) {
                if (i < len && (unsigned char)buf[i] >= 0x20 && (unsigned char)buf[i] < 0x7f) {
                    spaces += buf[i] == ' '; continue;
                }
                if (i - start >= 24 && spaces >= 2) {
                    char text[192];
                    size_t n = i - start < sizeof(text) - 1 ? i - start : sizeof(text) - 1;
                    memcpy(text, buf + start, n);text[n] = 0;
                    printf("A04BR3_REASON %s\n", text);lines++;
                }
                start = i + 1;spaces = 0;
            }
        }
    }
    printf("A04BR3_REASON done %llu KiB %u lines\n", scanned >> 10, lines);
    kfree_data(buf, 65536);
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
    printf("A04BR3_CRASH %s[%d] x0=0x%llx x1=0x%llx x2=0x%llx x3=0x%llx\n",
        name, pid, saved_state64(ss)->x[0], saved_state64(ss)->x[1],
        saved_state64(ss)->x[2], saved_state64(ss)->x[3]);
    if (sig == 6 && (!strcmp(name, "backboardd") || !strcmp(name, "lsd") ||
        !strcmp(name,"installd") || !strcmp(name,"installcoordinationd") ||
        !strcmp(name,"SpringBoard") || !strcmp(name,"Setup"))) a04br3_crash_strings(name);
    return fp;
}
#endif
