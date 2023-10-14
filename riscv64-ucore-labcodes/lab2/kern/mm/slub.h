/*
a basic slub structs
by 2110964
*/
#ifndef __KERN_MM_SLUB_H__ 
#define __KERN_MM_SLUB_H__  

#include <assert.h>
#include <atomic.h>
#include <defs.h>
#include <memlayout.h>
#include <mmu.h>
#include <riscv.h>



struct slub_cpu
{
    uintptr_t cpu_free_blocks;//link all of free blocks
    struct Page *slub;
    size_t free_blocks_num;
    list_entry_t slub_link;
};

typedef struct{
    
    list_entry_t partial_list;
}partial_area_t;

struct kmem_cache_node
{
    list_entry_t full_area;
    partial_area_t partial_area;
};


typedef struct{
    size_t object_size;

    size_t block_num;

    struct slub_cpu *cpu;

    struct kmem_cache_node *node;
}kmem_cache;





#endif /* __KERN_MM_SLUB_H__ */