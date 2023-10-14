#include <pmm.h>
#include <list.h>
#include <string.h>
#include <slub.h>

/*96,192,2^3~2^11*/
#define MAX_ORDER_OF_AN_OBJECT 11

#define my_cpu_slub (p_cache->cpu->slub)
#define my_cpu_freelist (p_cache->cpu->cpu_free_blocks)
#define my_node_fulllist (p_cache->node->full_area)
#define my_node_partialist (p_cache->node->partial_area)
#define le2slubcpu(le, member)                 \
    to_struct((le), struct slub_cpu, member)

/*
some globel value
*/
kmem_cache kmem_slub_cache[MAX_ORDER_OF_AN_OBJECT];

/*this is just a simple slub, if we need a more effient slub,we should allow to create new cache
kmem_cache kmem_create_cache(char* name,size_t object_size)
*/

static unsigned int getorder(size_t n)
{
    unsigned int order = 1;
    while (order < n) {
        order <<= 1;
    }
    return order;
}

/*init all of lists and the size of objects*/
static void slub_init(){
    for (size_t i = 0; i < MAX_ORDER_OF_AN_OBJECT; i++){
        //init the object size
        if (i==0){
            kmem_slub_cache[i].object_size = 96;
        }
        else if(i==1){
            kmem_slub_cache[i].object_size = 192;
        }
        else{
            kmem_slub_cache[i].object_size = 1<<i;
        }
        //init lists
        //list_init(&kmem_slub_cache[i].cpu->cpu_free_blocks);
        kmem_slub_cache[i].cpu->cpu_free_blocks=NULL;
        kmem_slub_cache[i].block_n = PGSIZE/kmem_slub_cache[i].object_size;
        kmem_slub_cache[i].cpu->free_blocks_num = 0;
        list_init(&kmem_slub_cache[i].node->full_area);
        list_init(&kmem_slub_cache[i].node->partial_area);
    }
}

/*should I define another type*/
static uintptr_t kmem_alloc(size_t n){
    unsigned int order = getorder(n);
    assert(order>11);//it'a simple slub, we don't treat the applyments which larger than 1 page

    /*find a suitable cache in needed*/
    kmem_cache* p_cache;
    if(n>64 && n<96){
        p_cache = kmem_slub_cache[0];
    }else if (n>128 && n <192)
    {
        p_cache = kmem_slub_cache[1];
    }else
    {
        p_cache = kmem_slub_cache[order];
    }
    
    uintptr_t result;
    size_t offset = p_cache->object_size/8;//move through the object and find the next object

    //ensure my cpu has a slub
    if(my_cpu_freelist==NULL){
        while(!list_empty(&my_node_partialist)){
            list_entry_t* le = &my_node_partialist;
            if((le = list_next(le)) != &my_node_partialist){
                my_cpu_slub = le2page(le,page_link);
                if(p_cache->block_n==p_cache->cpu->free_blocks_num + 1){
                    pmm_manager->free_pages(my_cpu_slub,1);//reback the page to pmm
                }
            }
        }
        //if have time, we should extract this as a function
        //remind that, we don't solve the request for more than 1 page
        my_cpu_slub = pmm_manager->alloc_pages(1);
        //we should set the blocks into free_list
        my_cpu_freelist = page2pa(my_cpu_slub);

        uintptr_t my_pointer = my_cpu_freelist;
        //we also need to set posion data, but we don't finish it now, since we just need a simple slub
        //so we just link those blocks
        for(size_t i = 0;i < p_cache->block_n;i++){
            if(i!=PGSIZE/p_cache->object_size-1)
                *(my_pointer+i*offset) = my_pointer + (i+1)*offset;
            else *(my_pointer+i*offset) = NULL;
        }
        p_cache->cpu->free_blocks_num=p_cache->cpu->blocks_num;//all of the blocks are free
    }

    //go to free_list to find a needed free block
    result = my_cpu_freelist;
    p_cache->cpu->free_blocks_num--;
    //we should set the red-zone, but again, this just a simple slub
    if(*(result)== NULL){
        my_cpu_freelist = NULL;
        list_add(&my_node_fulllist,&(my_cpu_slub->page_link));//when the free list is empty, means slub is full
    }
    //it should be a va, right?
    return result;
}

static void
kmem_free(uintptr_t va,size_t n){
    order = getorder(n);
    assert(order>11);

    /*find a suitable cache in needed*/
    kmem_cache* p_cache;
    if(n>64 && n<96){
        p_cache = kmem_slub_cache[0];
    }else if (n>128 && n <192)
    {
        p_cache = kmem_slub_cache[1];
    }else
    {
        p_cache = kmem_slub_cache[order];
    }

    //find the target is on which list
    pa = PADDR(va);
    struct Page* page = pa2page(pa);
    *pa = NULL;
    //find the list, to find the target you need
    int flag = 0;
    //if free from cpu
    if(page == my_cpu_slub){
        flag = 2;
    }
    list_entry_t* le = page->page_link; 
    while (flag!=2 &&
            (list_next(le) != &my_node_fulllist)
            ||((list_next(le)) != &my_node_partialist)) {
        le = list_next(le);
    }
    //if free from full
    if(list_next(le) == &my_node_fulllist){
        flag = 1;
    }
    //if free from partial
    else flag = 0;

    if(flag==2){
        if(p_cache->block_n==p_cache->cpu->free_blocks_num + 1){
            pmm_manager->free_pages(page,1);//reback the page to pmm
        }
    }else if (flag==1)
    {
        //give it to partial, then solve
        list_del(&(page->page_link));
        list_add(&(my_node_partialist),&(p_cache->cpu->slub_link));

        p_cache->cpu->cpu_free_blocks=
    }
    
}
