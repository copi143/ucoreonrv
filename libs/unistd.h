#ifndef __LIBS_UNISTD_H__
#define __LIBS_UNISTD_H__

#define T_SYSCALL           0x80

/* syscall number */
#define SYS_exit            1
#define SYS_fork            2
#define SYS_wait            3
#define SYS_exec            4
#define SYS_clone           5
#define SYS_yield           6
#define SYS_sleep           7
#define SYS_kill            8
#define SYS_ps              9
#define SYS_gettime         10
#define SYS_getpid          11
#define SYS_mmap            12
#define SYS_munmap          13
#define SYS_shmem           14
#define SYS_putc            15
#define SYS_pgdir           16
#define SYS_unlink          17
#define SYS_open            18
#define SYS_close           19
#define SYS_read            20
#define SYS_write           21
#define SYS_seek            22
#define SYS_fstat           23
#define SYS_fsync           24
#define SYS_getcwd          25
#define SYS_getdirentry     26
#define SYS_dup             27
#define SYS_pipe            28

/* OLNY FOR LAB6 */
#define SYS_lab6_set_priority 255

/* SYS_fork flags */
#define CLONE_VM            0x00000100  // set if VM shared between processes
#define CLONE_THREAD        0x00000200  // thread group
#define CLONE_FS            0x00000800  // set if shared between processes

/* VFS flags */
// flags for open: choose one of these
#define O_RDONLY            0           // open for reading only
#define O_WRONLY            1           // open for writing only
#define O_RDWR              2           // open for reading and writing
// then or in any of these:
#define O_CREAT             0x00000004  // create file if it does not exist
#define O_EXCL              0x00000008  // error if O_CREAT and the file exists
#define O_TRUNC             0x00000010  // truncate file upon open
#define O_APPEND            0x00000020  // append on each write
// additonal related definition
#define O_ACCMODE           3           // mask for O_RDONLY / O_WRONLY / O_RDWR

#define NO_FD               -0x9527     // invalid fd

/* lseek codes */
#define LSEEK_SET           0           // seek relative to beginning of file
#define LSEEK_CUR           1           // seek relative to current position in file
#define LSEEK_END           2           // seek relative to end of file

#define FS_MAX_DNAME_LEN    31
#define FS_MAX_FNAME_LEN    255
#define FS_MAX_FPATH_LEN    4095

#define EXEC_MAX_ARG_NUM    32
#define EXEC_MAX_ARG_LEN    4095

#endif /* !__LIBS_UNISTD_H__ */

