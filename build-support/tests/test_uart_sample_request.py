# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Actual queued sampling and tty case bodies; no UART/device operations."""
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    for offset in range(opening + 1, len(source)):
        depth += (source[offset] == '{') - (source[offset] == '}')
        if depth == 0:
            return source[start:offset + 1]
    raise AssertionError('unterminated source function')


class Sampling(unittest.TestCase):
    def test_actual_tiocstat_queue_and_selected_processes(self):
        tty = (ROOT / 'bsd/kern/tty.c').read_text()
        start = tty.index('case TIOCSTAT:                  /* simulate control-T */')
        branch = tty[start:tty.index('break;', start) + len('break;')]
        source = (ROOT / 'bsd/dev/a04br3_sample.c').read_text()
        request = function(source, 'void a04br3_sample_request(void)\n')
        process = function(source, 'static int\nsample_proc(')
        unit = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#define __unused __attribute__((unused))
#define TIOCSTAT 0x20007465UL
#define PROC_RETURNED 0
#define TASK_NULL ((void *)0)
typedef void *task_t;
typedef struct mock_proc { const char *name; int pid; task_t task; } *proc_t;
static unsigned request_enabled, request_pending, root, shown, sampled;
static int last_pid;
static void *tp = (void *)1;
static unsigned kauth_cred_get(void) { return root; }
static int kauth_cred_issuser(unsigned cred) { return cred == 1; }
static void ttyinfo_locked(void *tty) { assert(tty == tp); shown++; }
static const char *proc_best_name(proc_t p) { return p->name; }
static task_t proc_task(proc_t p) { return p->task; }
static int proc_pid(proc_t p) { return p->pid; }
static size_t strlcpy(char *out, const char *in, size_t n) {
    size_t length = strlen(in);
    if (n) { size_t count = length < n-1 ? length : n-1; memcpy(out,in,count); out[count]=0; }
    return length;
}
static const char *strnstr(const char *h, const char *n, size_t bytes) {
    size_t length=strlen(n);
    for(size_t i=0;i+length<=bytes && h[i];i++) if(!strncmp(h+i,n,length)) return h+i;
    return NULL;
}
static void a04br3_sample_task(const char *name, int pid, task_t task) {
    assert(name && task); sampled++; last_pid=pid;
}
REQUEST
PROCESS
static void status(void) { switch((unsigned long)TIOCSTAT) { BRANCH } }
int main(void) {
    root=0; request_enabled=1; status();
    assert(shown==1 && request_pending==0);
    root=1; status();
#if defined(ARM64_BOARD_CONFIG_A04BR3)
    assert(request_pending==1);
#else
    assert(request_pending==0);
#endif
    assert(shown==2);
    request_pending=0; request_enabled=0; status();
    assert(shown==3 && request_pending==0); /* disabled worker remains disabled */
    request_enabled=1; a04br3_sample_request(); a04br3_sample_request();
    assert(request_pending==1); /* queues one bit, never walks tasks under tty lock */
    struct mock_proc p={"launchd.a04br3",3,(void *)1};
    assert(sample_proc(&p,NULL)==PROC_RETURNED && sampled==1 && last_pid==3);
    p.pid=2;sample_proc(&p,NULL);assert(sampled==2 && last_pid==2); /* supervisor */
    p.name="configd";sample_proc(&p,NULL);assert(sampled==3);
    p.name="unrelated";sample_proc(&p,NULL);assert(sampled==3);
    p.name="launchd.a04br3";p.task=TASK_NULL;sample_proc(&p,NULL);assert(sampled==3);
    return 0;
}
'''.replace('REQUEST', request).replace('PROCESS', process).replace('BRANCH', branch)
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory)
            (path / 'case.c').write_text(unit)
            for board in (False, True):
                binary = path / ('board' if board else 'other')
                flags = ['-DARM64_BOARD_CONFIG_A04BR3=1'] if board else []
                subprocess.run(['clang', '-std=c11', '-Wall', '-Wextra', '-Werror',
                                '-Wno-unused-function', '-fsanitize=address,undefined',
                                *flags, str(path / 'case.c'), '-o', str(binary)], check=True)
                subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    unittest.main()
