
#ifndef __KERN_FS_PIPE_DEV_H__
#define __KERN_FS_PIPE_DEV_H__

#include <defs.h>
#include <wait.h>

struct inode;
struct iobuf;

#define PIPE_BUFFER_SIZE 4096

struct pipe {
    size_t p_head;
    size_t p_tail;
    wait_queue_t p_read_queue;
    wait_queue_t p_write_queue;
    char* p_buffer;
};

#define pipe_empty(pip) (pip->p_head == pip->p_tail)
#define pipe_full(pip) (pip->p_head == (pip->p_tail + 1) % PIPE_BUFFER_SIZE)

struct inode* pipe_create_inode();

#endif /* !__KERN_FS_PIPE_DEV_H__ */
