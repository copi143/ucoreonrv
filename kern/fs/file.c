#include <defs.h>
#include <string.h>
#include <vfs.h>
#include <proc.h>
#include <file.h>
#include <unistd.h>
#include <iobuf.h>
#include <inode.h>
#include <stat.h>
#include <dirent.h>
#include <error.h>
#include <assert.h>
#include <log.h>
#include <pipe.h>

#define testfd(fd)                          ((fd) >= 0 && (fd) < FILES_STRUCT_NENTRY)

// get_fd_array - get current process's open files table
static struct file *
get_fd_array(void) {
    struct files_struct *filesp = current->filesp;
    assert(filesp != NULL && files_count(filesp) > 0);
    return filesp->fd_array;
}

// fd_array_init - initialize the open files table
void
fd_array_init(struct file *fd_array) {
    int fd;
    struct file *file = fd_array;
    for (fd = 0; fd < FILES_STRUCT_NENTRY; fd ++, file ++) {
        file->open_count = 0;
        file->status = FD_NONE, file->fd = fd;
    }
}

// fs_array_alloc - allocate a free file item (with FD_NONE status) in open files table
static int
fd_array_alloc(int fd, struct file **file_store) {
//    panic("debug");
    struct file *file = get_fd_array(); // 拿到当前的进程的file列表，问题就是这个列表意味着什么？注释里说的是“opened file”
    if (fd == NO_FD) { //无效的fd，从0开始找，找到第一个FD_NONE状态的file
        for (fd = 0; fd < FILES_STRUCT_NENTRY; fd ++, file ++) { 
            if (file->status == FD_NONE) {
                goto found;
            }
        }
        return -E_MAX_OPEN;
    }
    else {// 有效的fd，直接锁定
        if (testfd(fd)) {
            file += fd;
            if (file->status == FD_NONE) {
                goto found;
            }
            return -E_BUSY;
        }
        return -E_INVAL;
    }
found:
    assert(fopen_count(file) == 0);
    file->status = FD_INIT, file->node = NULL;// 将文件状态标识为init
    *file_store = file;
    return 0;
}

// fd_array_free - free a file item in open files table
static void
fd_array_free(struct file *file) {
    assert(file->status == FD_INIT || file->status == FD_CLOSED);
    assert(fopen_count(file) == 0);
    if (file->status == FD_CLOSED) {
        vfs_close(file->node);
    }
    file->status = FD_NONE;
}

static void
fd_array_acquire(struct file *file) {
    assert(file->status == FD_OPENED);
    fopen_count_inc(file);
}

// fd_array_release - file's open_count--; if file's open_count-- == 0 , then call fd_array_free to free this file item
static void
fd_array_release(struct file *file) {
    assert(file->status == FD_OPENED || file->status == FD_CLOSED);
    assert(fopen_count(file) > 0);
    if (fopen_count_dec(file) == 0) {
        fd_array_free(file);
    }
}

// fd_array_open - file's open_count++, set status to FD_OPENED
void
fd_array_open(struct file *file) {
    assert(file->status == FD_INIT && file->node != NULL);
    file->status = FD_OPENED; //将fd置为opened
    fopen_count_inc(file);
}

// fd_array_close - file's open_count--; if file's open_count-- == 0 , then call fd_array_free to free this file item
void
fd_array_close(struct file *file) {
    assert(file->status == FD_OPENED);
    assert(fopen_count(file) > 0);
    file->status = FD_CLOSED;
    if (fopen_count_dec(file) == 0) {
        fd_array_free(file);
    }
}

//fs_array_dup - duplicate file 'from'  to file 'to'
void
fd_array_dup(struct file *to, struct file *from) {
    //cprintf("[fd_array_dup]from fd=%d, to fd=%d\n",from->fd, to->fd);
    assert(to->status == FD_INIT && from->status == FD_OPENED);
    to->pos = from->pos;
    to->readable = from->readable;
    to->writable = from->writable;
    struct inode *node = from->node;
    vop_ref_inc(node), vop_open_inc(node);
    to->node = node;
    fd_array_open(to);
}

// fd2file - use fd as index of fd_array, return the array item (file)
static inline int
fd2file(int fd, struct file **file_store) {
    if (testfd(fd)) {
        struct file *file = get_fd_array() + fd;
        if (file->status == FD_OPENED && file->fd == fd) {
            *file_store = file;
            return 0;
        }
    }
    return -E_INVAL;
}

// file_testfd - test file is readble or writable?
bool
file_testfd(int fd, bool readable, bool writable) {
    int ret;
    struct file *file;
    if ((ret = fd2file(fd, &file)) != 0) {
        return 0;
    }
    if (readable && !file->readable) {
        return 0;
    }
    if (writable && !file->writable) {
        return 0;
    }
    return 1;
}

// open file
int
file_open(char *path, uint32_t open_flags) {
    infof("Attempting to open file: %s with flags: 0x%x", path, open_flags);
    bool readable = 0, writable = 0;
    switch (open_flags & O_ACCMODE) { //解析 open_flags
    case O_RDONLY: readable = 1; break;
    case O_WRONLY: writable = 1; break;
    case O_RDWR:
        readable = writable = 1;
        break;
    default:
        errorf("Invalid access mode for file: %s", path);
        return -E_INVAL;
    }
    int ret;
    struct file *file;
    if ((ret = fd_array_alloc(NO_FD, &file)) != 0) { // 从头到尾找到第一个未初始化的fd，并将其初始化init
        errorf("Failed to allocate file descriptor for file: %s", path);
        return ret;
    }
    struct inode *node;
    if ((ret = vfs_open(path, open_flags, &node)) != 0) {
        fd_array_free(file);
        errorf("Failed to open file: %s, error code: %d", path, ret);
        return ret;
    }
    file->pos = 0;
    if (open_flags & O_APPEND) {
        struct stat __stat, *stat = &__stat;
        if ((ret = vop_fstat(node, stat)) != 0) {
            vfs_close(node);
            fd_array_free(file);
            errorf("Failed to get file stats for file: %s in append mode", path);
            return ret;
        }
        file->pos = stat->st_size;
    }
    file->node = node;
    file->readable = readable;
    file->writable = writable;
    fd_array_open(file);
    // infof("File opened successfully: %s, FD: %d", path, file->fd);
    if (file->fd != 0 && file->fd != 1) {
        infof("File opened successfully: %s, FD: %d", path, file->fd);
    
    }
    return file->fd;
}

// close file
int
file_close(int fd) {
    // infof("Attempting to close file descriptor: %d", fd);
    if (fd != 0) {
        infof("Attempting to close file descriptor: %d", fd);
    }
    
    int ret;
    struct file *file;
    if ((ret = fd2file(fd, &file)) != 0) {
        errorf("Invalid file descriptor: %d", fd);
        return ret;
    }

    fd_array_close(file);
    // infof("File descriptor %d closed successfully", fd);
    if (fd != 0) {
        infof("File descriptor %d closed successfully", fd);
    }
    return 0;
}

// read file
int
file_read(int fd, void *base, size_t len, size_t *copied_store) {
    // infof("Reading from file descriptor: %d", fd);
    if (fd != 0) {
       infof("Reading from file descriptor: %d", fd); 
    }

    int ret;
    struct file *file;
    *copied_store = 0;
    if ((ret = fd2file(fd, &file)) != 0) {
        errorf("Invalid file descriptor for read: %d", fd);
        return ret;
    }
    if (!file->readable) {
        errorf("File descriptor %d is not readable", fd);
        return -E_INVAL;
    }

    fd_array_acquire(file);
    struct iobuf __iob, *iob = iobuf_init(&__iob, base, len, file->pos);
    ret = vop_read(file->node, iob);

    size_t copied = iobuf_used(iob);
    if (file->status == FD_OPENED) {
        file->pos += copied;
    }
    *copied_store = copied;
    fd_array_release(file);

    // if (ret == 0) {
    //     infof("Read from file descriptor %d", fd);
    // } else {
    //     errorf("Error reading from file descriptor %d, error code: %d", fd, ret);
    // }
    if (fd != 0) {
        if (ret == 0) {
            infof("Read from file descriptor %d", fd);
        } else {
            errorf("Error reading from file descriptor %d, error code: %d", fd, ret);
        }
    }
    
    return ret;
}


// write file
int
file_write(int fd, void *base, size_t len, size_t *copied_store) {
    // infof("Writing to file descriptor: %d", fd);
    if (fd != 1) {
        infof("Writing to file descriptor: %d", fd);
    }

    int ret;
    struct file *file;
    *copied_store = 0;
    if ((ret = fd2file(fd, &file)) != 0) {
        errorf("Invalid file descriptor for write: %d", fd);
        return ret;
    }
    if (!file->writable) {
        errorf("File descriptor %d is not writable", fd);
        return -E_INVAL;
    }

    fd_array_acquire(file);
    struct iobuf __iob, *iob = iobuf_init(&__iob, base, len, file->pos);
    ret = vop_write(file->node, iob);

    size_t copied = iobuf_used(iob);
    if (file->status == FD_OPENED) {
        file->pos += copied;
    }
    *copied_store = copied;
    fd_array_release(file);

    // if (ret == 0) {
    //     infof("Write into file descriptor %d", fd);
    // } else {
    //     errorf("Error writing to file descriptor %d, error code: %d", fd, ret);
    // }
    if (fd != 1) {
        if (ret == 0) {
            infof("Write into file descriptor %d", fd);
        } else {
            errorf("Error writing to file descriptor %d, error code: %d", fd, ret);
        }
    }

    return ret;
}


// seek file
int
file_seek(int fd, off_t pos, int whence) {
    // infof("Seeking in file descriptor %d, position: %lld, whence: %d", fd, (long long)pos, whence);
    if (fd != 0) {
        infof("Seeking in file descriptor %d, position: %lld, whence: %d", fd, (long long)pos, whence);
    }

    struct stat __stat, *stat = &__stat;
    int ret;
    struct file *file;
    if ((ret = fd2file(fd, &file)) != 0) {
        errorf("Invalid file descriptor for seek: %d", fd);
        return ret;
    }

    fd_array_acquire(file);
    switch (whence) {
        case LSEEK_SET: break;
        case LSEEK_CUR: pos += file->pos; break;
        case LSEEK_END:
            if ((ret = vop_fstat(file->node, stat)) == 0) {
                pos += stat->st_size;
            }
            break;
        default: ret = -E_INVAL;
    }

    if (ret == 0) {
        if ((ret = vop_tryseek(file->node, pos)) == 0) {
            file->pos = pos;
        }
    }
    fd_array_release(file);

    // if (ret == 0) {
    //     infof("Seek successful in file descriptor %d, new position: %lld", fd, (long long)file->pos);
    // } else {
    //     errorf("Error seeking in file descriptor %d, error code: %d", fd, ret);
    // }
    if (fd != 0) {
        if (ret == 0) {
            infof("Seek successful in file descriptor %d, new position: %lld", fd, (long long)file->pos);
        } else {
            errorf("Error seeking in file descriptor %d, error code: %d", fd, ret);
        }
    }

    return ret;
}

// stat file
int
file_fstat(int fd, struct stat *stat) {
    int ret;
    struct file *file;
    if ((ret = fd2file(fd, &file)) != 0) {
        return ret;
    }
    fd_array_acquire(file);
    ret = vop_fstat(file->node, stat);
    fd_array_release(file);
    return ret;
}

// sync file
int
file_fsync(int fd) {
    int ret;
    struct file *file;
    if ((ret = fd2file(fd, &file)) != 0) {
        return ret;
    }
    fd_array_acquire(file);
    ret = vop_fsync(file->node);
    fd_array_release(file);
    return ret;
}

// get file entry in DIR
int
file_getdirentry(int fd, struct dirent *direntp) {
    int ret;
    struct file *file;
    if ((ret = fd2file(fd, &file)) != 0) {
        return ret;
    }
    fd_array_acquire(file);

    struct iobuf __iob, *iob = iobuf_init(&__iob, direntp->name, sizeof(direntp->name), direntp->offset);
    if ((ret = vop_getdirentry(file->node, iob)) == 0) {
        direntp->offset += iobuf_used(iob);
    }
    fd_array_release(file);
    return ret;
}

// duplicate file
int
file_dup(int fd1, int fd2) {
    int ret;
    struct file *file1, *file2;
    if ((ret = fd2file(fd1, &file1)) != 0) {
        return ret;
    }
    if ((ret = fd_array_alloc(fd2, &file2)) != 0) {
        return ret;
    }
    fd_array_dup(file2, file1);
    return file2->fd;
}

// create pipe
int file_pipe(int* fd_store)
{
    int ret = 0;
    // allocate fd
    struct file *file_in, *file_out;
    if ((ret = fd_array_alloc(NO_FD, &file_in)) != 0) {
        return ret;
    }
    if ((ret = fd_array_alloc(NO_FD, &file_out)) != 0) {
        fd_array_free(file_in);
        return ret;
    }
    struct inode* node;
    if ((node = pipe_create_inode()) == NULL) {
        fd_array_free(file_in);
        fd_array_free(file_out);
        return -E_NO_MEM;
    }
    vop_ref_inc(node);
    vop_open_inc(node);
    vop_open_inc(node);
    // set file descriptor
    file_in->pos = 0;
    file_in->node = node;
    file_in->readable = 1;
    file_in->writable = 0;
    fd_array_open(file_in);
    file_out->pos = 0;
    file_out->node = node;
    file_out->readable = 0;
    file_out->writable = 1;
    fd_array_open(file_out);
    // set file descriptor number
    fd_store[0] = file_in->fd;
    fd_store[1] = file_out->fd;
    return ret;
}