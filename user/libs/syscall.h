#ifndef __USER_LIBS_SYSCALL_H__
#define __USER_LIBS_SYSCALL_H__

#include <defs.h>

int sys_exit(int64_t error_code);
int sys_fork(void);
int sys_wait(int64_t pid, int64_t *store);
int sys_exec(const char *name, int64_t argc, const char **argv);
int sys_yield(void);
int sys_kill(int64_t pid);
int sys_getpid(void);
int sys_putc(int64_t c);
int sys_pgdir(void);
int sys_sleep(int64_t time);
int sys_gettime(void);

struct stat;
struct dirent;

int sys_open(const char *path, uint64_t open_flags);
int sys_close(int64_t fd);
int sys_read(int64_t fd, void *base, size_t len);
int sys_write(int64_t fd, void *base, size_t len);
int sys_seek(int64_t fd, off_t pos, int64_t whence);
int sys_fstat(int64_t fd, struct stat *stat);
int sys_fsync(int64_t fd);
int sys_getcwd(char *buffer, size_t len);
int sys_getdirentry(int64_t fd, struct dirent *dirent);
int sys_dup(int64_t fd1, int64_t fd2);
int sys_pipe(int64_t *fd_store);
void sys_lab6_set_priority(uint64_t priority); //only for lab6
int sys_unlink(const char* path);
int sys_ps(void);

#endif /* !__USER_LIBS_SYSCALL_H__ */

