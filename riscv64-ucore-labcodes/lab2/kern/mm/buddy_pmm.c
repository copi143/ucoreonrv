#include <pmm.h>
#include <list.h>
#include <string.h>
#include <default_pmm.h>
#include <stdio.h>

#define LEFT_LEAF(index) ((index) * 2 + 1)
#define RIGHT_LEAF(index) ((index) * 2 + 2)
#define PARENT(index) ( ((index) + 1) / 2 - 1)
#define IS_POWER_OF_2(x) (!((x)&((x)-1)))
unsigned calculate_node_size(unsigned n) {
    unsigned i = 0;
    while (n > 1) {
        n >>= 1; 
        i++;
    }
    return 1 << i; 
}
free_area_t free_area;
#define max(a, b) ((a) > (b) ? (a) : (b))
#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)
unsigned *root;
int size;
struct Page *_base;

static void
buddy_system_init(void)
{
    list_init(&free_list);
    nr_free = 0;
}

static void
buddy_system_init_memmap(struct Page *base, size_t n)
{
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p++)
    {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
        SetPageProperty(p);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    _base = base;
    size =calculate_node_size(n);
    unsigned node_size = 2*size;
    root = (unsigned *)(base + size);
    for (int i = 0; i < 2 * size - 1; ++i)
    {
        if (IS_POWER_OF_2(i + 1))
            node_size /= 2;
        root[i] = node_size;
    }
}

static struct Page *
buddy_system_alloc_pages(size_t n)
{
    assert(n > 0);
    if (n > nr_free)
    {
        return NULL;
    }
    struct Page *page = NULL;
    unsigned index = 0;
    unsigned node_size;
    unsigned offset = 0;
    if (n <= 0)
        n = 1;
    else if (!IS_POWER_OF_2(n))
    {
        n = calculate_node_size(n);
    }
    if (root[index] < n)
        offset = -1;
    for (node_size = size; node_size != n; node_size /= 2)
    {
        if (root[LEFT_LEAF(index) ] >= n)
            index =LEFT_LEAF(index) ;
        else
            index =RIGHT_LEAF(index) ;
    }
    root[index] = 0;
    offset = (index + 1) * node_size - size;
    while (index > 0)
    {
       index=PARENT(index);
        root[index]=max(root[LEFT_LEAF(index)],root[RIGHT_LEAF(index)]);
    }
    page = _base+ offset;
    page->property = n;
    unsigned size_ = calculate_node_size(n);
    nr_free -= size_;
    for (struct Page *p = page; p != page + size_; p++)
        ClearPageProperty(p);
    return page;
}

static void
buddy_system_free_pages(struct Page *base, size_t n)
{
    assert(n > 0);
    n=calculate_node_size(n);
    struct Page *p = base;
    for (; p != base + n; p++)
    {
        assert(!PageReserved(p) && !PageProperty(p));
        set_page_ref(p, 0);
    }
    nr_free += n;
    unsigned offset = base - _base;
    unsigned node_size = 1;
    unsigned index = size + offset - 1;
    while(root[index]){
        node_size *= 2;
        if (index == 0)
            return;
        index=PARENT(index);
    }
    root[index] = node_size;
    while (index)
    {
        index=PARENT(index);
        node_size *= 2;
        if (root[LEFT_LEAF(index)] + root[RIGHT_LEAF(index)] == node_size)
            root[index] = node_size;
        else
            root[index]=max(root[LEFT_LEAF(index)],root[RIGHT_LEAF(index)]);
    }
    for (struct Page *p = base; p != base + n; p++)
        ClearPageProperty(p);
}

static size_t
buddy_system_nr_free_pages(void)
{
    return nr_free;
}
//下面检测来自伙伴分配器链接开端那张图+csdn
static void
buddy_check(void)
{
/*
    cprintf("root check%s\n","!");
    
    assert((p0 = alloc_page()) != NULL);
    assert((A = alloc_page()) != NULL);
    assert((B = alloc_page()) != NULL);

    cprintf("before free p0,A,B root[0] %u\n", root[0]);

    assert(p0 != A && p0 != B && A != B);
    assert(page_ref(p0) == 0 && page_ref(A) == 0 && page_ref(B) == 0);

    free_page(p0);
    free_page(A);
    free_page(B);
    cprintf("after free p0,A,B root[0] %u\n", root[0]);
*/
    struct Page *p0, *A, *B, *C, *D;
    p0 = A = B = C = D = NULL;
    A = alloc_pages(512);
    B = alloc_pages(512);
    cprintf("A分配512，B分配512\n");
    cprintf("此时A %p\n",A);
    cprintf("此时B %p\n",B);
    free_pages(A, 256);
    cprintf("A释放256\n");
    free_pages(B, 512);
    cprintf("B释放512\n");
    free_pages(A + 256, 256);
    cprintf("A释放256\n");
    p0 = alloc_pages(8192);
    cprintf("p0分配8192\n");
    assert(p0 == A);
    free_pages(p0, 1024);
    cprintf("p0释放1024\n");
    // 以下是根据链接中的样例测试编写的
    A = alloc_pages(128);
    B = alloc_pages(64);
    cprintf("A分配128，B分配64\n");
    cprintf("此时A %p\n",A);
    cprintf("此时B %p\n",B);
    // 检查是否相邻
    assert(A + 128 == B);
    cprintf("检查AB相邻\n");
    C = alloc_pages(128);
    cprintf("C分配128\n");
    cprintf("此时C %p\n",C);
    // 检查C有没有和A重叠
    assert(A + 256 == C);
    cprintf("检查AB重叠\n");
    // 释放A
    free_pages(A, 128);
    cprintf("A释放128\n");
    cprintf("D分配64\n");
    D = alloc_pages(64);
    cprintf("此时D %p\n", D);
    // 检查D是否能够使用A刚刚释放的内存
    assert(D + 128 == B);
    cprintf("检查D使用刚才的A内存\n");
    free_pages(C, 128);
    cprintf("C释放128\n");
    C = alloc_pages(64);
    cprintf("C分配64\n");
    // 检查C是否在B、D之间
    assert(C == D + 64 && C == B - 64);
    cprintf("检查C在BD中间\n");
    free_pages(B, 64);
    free_pages(D, 64);
    free_pages(C, 64);
    // 全部释放
    free_pages(p0, 8192);
    cprintf("全部释放\n");
    cprintf("检查完成，没有错误\n");
}
const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_system_init,
    .init_memmap = buddy_system_init_memmap,
    .alloc_pages = buddy_system_alloc_pages,
    .free_pages = buddy_system_free_pages,
    .nr_free_pages = buddy_system_nr_free_pages,
    .check = buddy_check,
};
