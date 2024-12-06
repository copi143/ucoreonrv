#include <assert.h>
#include <clock.h>
#include <pmm.h>
#include <proc.h>
#include <stdio.h>
#include <syscall.h>
#include <sysfile.h>
#include <trap.h>
#include <unistd.h>
#include <log.h>

static int
sys_exit(uint64_t arg[])
{
    int error_code = (int)arg[0];
    return do_exit(error_code);
}

static int
sys_fork(uint64_t arg[])
{
    struct trapframe* tf = current->tf;
    uintptr_t stack = tf->gpr.sp;
    return do_fork(0, stack, tf);
}

static int
sys_wait(uint64_t arg[])
{
    int pid = (int)arg[0];
    int* store = (int*)arg[1];
    return do_wait(pid, store);
}
static int
sys_exec(uint64_t arg[])
{
    const char* name = (const char*)(arg[0]);
    int argc = (int)arg[1];
    const char** argv = (const char**)arg[2];
    // cprintf("kernel sys_exec: name: %s, argc: %d\n", name, argc);
    return do_execve(name, argc, argv);
}

static int
sys_yield(uint64_t arg[])
{
    return do_yield();
}

static int
sys_kill(uint64_t arg[])
{
    int pid = (int)arg[0];
    return do_kill(pid);
}

static int
sys_ps(uint64_t arg[])
{
    return do_ps();
}

static int
sys_getpid(uint64_t arg[])
{
    return current->pid;
}

static int
sys_putc(uint64_t arg[])
{
    int c = (int)arg[0];
    cputchar(c);
    return 0;
}

static int
sys_unlink(uint64_t arg[])
{
    const char* path = (const char*)arg[0];
    return sysfile_unlink(path);
}

static int
sys_pgdir(uint64_t arg[])
{
    // print_pgdir();
    return 0;
}
static int sys_gettime(uint64_t arg[])
{
    return (int)ticks * 10;
}
static int sys_lab6_set_priority(uint64_t arg[])
{
    uint64_t priority = (uint64_t)arg[0];
    lab6_set_priority(priority);
    return 0;
}
static int
sys_sleep(uint64_t arg[])
{
    unsigned int time = (unsigned int)arg[0];
    return do_sleep(time);
}
static int
sys_open(uint64_t arg[])
{
    const char* path = (const char*)arg[0];
    uint32_t open_flags = (uint32_t)arg[1];
    return sysfile_open(path, open_flags);
}

static int
sys_close(uint64_t arg[])
{
    int fd = (int)arg[0];
    return sysfile_close(fd);
}

static int
sys_read(uint64_t arg[])
{
    int fd = (int)arg[0];
    void* base = (void*)arg[1];
    size_t len = (size_t)arg[2];
    return sysfile_read(fd, base, len);
}

static int
sys_write(uint64_t arg[])
{
    int fd = (int)arg[0];
    void* base = (void*)arg[1];
    size_t len = (size_t)arg[2];
    return sysfile_write(fd, base, len);
}

static int
sys_seek(uint64_t arg[])
{
    int fd = (int)arg[0];
    off_t pos = (off_t)arg[1];
    int whence = (int)arg[2];
    return sysfile_seek(fd, pos, whence);
}

static int
sys_fstat(uint64_t arg[])
{
    int fd = (int)arg[0];
    struct stat* stat = (struct stat*)arg[1];
    return sysfile_fstat(fd, stat);
}

static int
sys_fsync(uint64_t arg[])
{
    int fd = (int)arg[0];
    return sysfile_fsync(fd);
}

static int
sys_getcwd(uint64_t arg[])
{
    char* buf = (char*)arg[0];
    size_t len = (size_t)arg[1];
    return sysfile_getcwd(buf, len);
}

static int
sys_getdirentry(uint64_t arg[])
{
    int fd = (int)arg[0];
    struct dirent* direntp = (struct dirent*)arg[1];
    return sysfile_getdirentry(fd, direntp);
}

static int
sys_dup(uint64_t arg[])
{
    int fd1 = (int)arg[0];
    int fd2 = (int)arg[1];
    return sysfile_dup(fd1, fd2);
}


static int (*syscalls[])(uint64_t arg[]) = {
    [SYS_exit] sys_exit,
    [SYS_fork] sys_fork,
    [SYS_wait] sys_wait,
    [SYS_exec] sys_exec,
    [SYS_yield] sys_yield,
    [SYS_kill] sys_kill,
    [SYS_ps] sys_ps,
    [SYS_getpid] sys_getpid,
    [SYS_putc] sys_putc,
    [SYS_unlink] sys_unlink,
    [SYS_pgdir] sys_pgdir,
    [SYS_gettime] sys_gettime,
    [SYS_lab6_set_priority] sys_lab6_set_priority,
    [SYS_sleep] sys_sleep,
    [SYS_open] sys_open,
    [SYS_close] sys_close,
    [SYS_read] sys_read,
    [SYS_write] sys_write,
    [SYS_seek] sys_seek,
    [SYS_fstat] sys_fstat,
    [SYS_fsync] sys_fsync,
    [SYS_getcwd] sys_getcwd,
    [SYS_getdirentry] sys_getdirentry,
    [SYS_dup] sys_dup,
};

static char* syscalls_name[] = {
    "",
    "exit", "fork", "wait", "exec", "clone", "yield", "sleep", "kill", "ps",
    "gettime", "getpid", "mmap", "munmap", "shmem", "putc", "pgdir",
    "unlink", "open", "close", "read", "write", "seek", "fstat", "fsync",
    "getcwd", "getdirentry", "dup",
};

#define NUM_SYSCALLS ((sizeof(syscalls)) / (sizeof(syscalls[0])))

void syscall(void)
{
    struct trapframe* tf = current->tf;
    uint64_t arg[5];
    int num = tf->gpr.a0;
    if (num >= 0 && num < NUM_SYSCALLS) {
        if (syscalls[num] != NULL) {
            arg[0] = tf->gpr.a1;
            arg[1] = tf->gpr.a2;
            arg[2] = tf->gpr.a3;
            arg[3] = tf->gpr.a4;
            arg[4] = tf->gpr.a5;
            tf->gpr.a0 = syscalls[num](arg);
            if (num != SYS_read && num != SYS_write && num != SYS_putc) {
                tracef("syscall %d: %s, pid = %d, name = %s.",
                    num, syscalls_name[num], current->pid, current->name);
            }
            return;
        }
    }
    print_trapframe(tf);
    panic("undefined syscall %d, pid = %d, name = %s.\n",
        num, current->pid, current->name);
}
