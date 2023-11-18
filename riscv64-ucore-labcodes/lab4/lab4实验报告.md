<h1><center>lab4实验报告</center></h1>



###  练习1：分配并初始化一个进程控制块（需要编码）

alloc_proc函数（位于kern/process/proc.c中）负责分配并返回一个新的struct proc_struct结构，用于存储新建立的内核线程的管理信息。ucore需要对这个结构进行最基本的初始化，你需要完成这个初始化过程。

> 【提示】在alloc_proc函数的实现中，需要初始化的proc_struct结构中的成员变量至少包括：state/pid/runs/kstack/need_resched/parent/mm/context/tf/cr3/flags/name。

请在实验报告中简要说明你的设计实现过程。请回答如下问题：

- 请说明proc_struct中`struct context context`和`struct trapframe *tf`成员变量含义和在本实验中的作用是啥？（提示通过看代码和编程调试可以判断出来）


这里的初始化思路很简单，就是一个清零操作，具体来说，就是将所有的成员全部设置为 0，对应于指针则为 `NULL`，部分则需要特殊处理。我们的指导书也是很贴心的给出了三个特殊的处理，分别是初始化状态为 `PROC_UNINIT`，`pid` 为 `-1`，页表基址设置为启动页表的地址。
这里复制了指导书对于第三个的说明，由于该内核线程在内核中运行，故采用为uCore内核已经建立的页表，即设置为在uCore内核页表的起始地址boot_cr3。

```c
        proc->state = PROC_UNINIT; 
        proc->pid = -1;
        proc->runs = 0;
        proc->kstack = 0;
        proc->need_resched = 0;
        proc->parent = NULL;
        proc->mm = NULL;
        memset(&(proc->context), 0, sizeof(struct context));
        proc->tf = NULL;
        proc->cr3 = boot_cr3;
        proc->flags = 0;
        memset(proc->name, 0, PROC_NAME_LEN);
```

proc_struct中`struct context context`表示进程执行的上下文，具体来说，结构体`struct context`中包含了ra，sp，s0~s11共14个寄存器，这些寄存器的值用于在进程切换中还原之前进程的运行状态。而之所以不需保存所有寄存器，是利用了编译器对于函数的处理。我们知道寄存器可以分为调用者保存（caller-saved）寄存器和被调用者保存（callee-saved）寄存器。因为线程切换是在一个函数`void switch_to(struct context *from, struct context *to)`（定义在riscv64-ucore-labcodes\lab4\kern\process\switch.S）当中实现的，所以编译器会自动帮助我们生成保存和恢复调用者保存寄存器的代码，在实际的进程切换过程中我们只需要保存被调用者保存寄存器即可。

`struct trapframe *tf`表示进程的中断帧，当进程从用户空间跳进内核空间的时候，进程的执行状态被保存在了中断帧中。但本次实验仅实现了内核线程的管理，因此要想理解`struct trapframe *tf`在本实验中的作用，先来看一下我们是如何创建并切换到第1个内核线程 initproc：

在创建 initproc 时，`kernel_thread`函数采用了局部变量tf来放置保存内核线程的临时中断帧，并把中断帧tf的指针传递给`do_fork`函数，而`do_fork`函数会调用`copy_thread`函数:

```c
static void
copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf) {
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE - sizeof(struct trapframe));
    *(proc->tf) = *tf;

    // Set a0 to 0 so a child process knows it's just forked
    proc->tf->gpr.a0 = 0;
    proc->tf->gpr.sp = (esp == 0) ? (uintptr_t)proc->tf : esp;

    proc->context.ra = (uintptr_t)forkret;
    proc->context.sp = (uintptr_t)(proc->tf);
}
```
`copy_thread`函数在新创建的进程内核栈上专门给进程的中断帧分配了一块空间，然后将进程的中断帧中的a0寄存器（返回值）设置为0，说明这个进程是一个子进程。之后将上下文中的ra（返回地址）设置为了forkret函数的入口，所以在上下文切换后会返回到forkret函数。`copy_thread`函数还把trapframe放在上下文的栈顶，而forkret函数把trapframe传给了forkrets：

```c
static void
forkret(void) {
    forkrets(current->tf);
}
```
forkrets再把传进来的参数，也就是进程的中断帧放在了sp：
```
    .globl forkrets
forkrets:
    # set stack to this new process's trapframe
    move sp, a0
    j __trapret
```
这样在__trapret中就可以直接从中断帧里面恢复所有的寄存器：
```
__trapret:
    RESTORE_ALL
    # go back from supervisor call
    sret
```

我们在初始化的时候还将中断帧epc寄存器指向`kernel_thread_entry`，在`kernel_thread_entry`函数中：

```
.text
.globl kernel_thread_entry
kernel_thread_entry:        # void kernel_thread(void)
    move a0, s1
    jalr s0

    jal do_exit
```
其中s0寄存器里放的是新进程要执行的函数，s1寄存器里放的是传给函数的参数。我们把参数放在了a0寄存器，并跳转到s0执行我们指定的函数。这样，一个进程的初始化就完成了。

因此，`struct context context`在本实验的作用就是保存进程的上下文（主要是被调用者保存寄存器），便于在进程切换中还原之前进程的运行状态；`struct trapframe *tf`在本实验的作用就是保存第1个内核线程 initproc 的中断帧，设置其运行后去执行我们指定的函数，并在上下文切换时恢复之前进程的所有寄存器。

### 练习2：为新创建的内核线程分配资源（需要编码）

创建一个内核线程需要分配和设置好很多资源。kernel_thread函数通过调用**do_fork**函数完成具体内核线程的创建工作。do_kernel函数会调用alloc_proc函数来分配并初始化一个进程控制块，但alloc_proc只是找到了一小块内存用以记录进程的必要信息，并没有实际分配这些资源。ucore一般通过do_fork实际创建新的内核线程。do_fork的作用是，创建当前内核线程的一个副本，它们的执行上下文、代码、数据都一样，但是存储位置不同。因此，我们**实际需要”fork”的东西就是stack和trapframe**。在这个过程中，需要给新内核线程分配资源，并且复制原进程的状态。你需要完成在kern/process/proc.c中的do_fork函数中的处理过程。它的大致执行步骤包括：

- 调用alloc_proc，首先获得一块用户信息块。
- 为进程分配一个内核栈。
- 复制原进程的内存管理信息到新进程（但内核线程不必做此事）
- 复制原进程上下文到新进程
- 将新进程添加到进程列表
- 唤醒新进程
- 返回新进程号

请在实验报告中简要说明你的设计实现过程。请回答如下问题：

- 请说明ucore是否做到给每个新fork的线程一个唯一的id？请说明你的分析和理由。


这里的实现过程其实上面已经写的很明确了，我把具体想说的都放在了注释里面，具体如下：
```C
/* do_fork -     parent process for a new child process
 * @clone_flags: used to guide how to clone the child process
 * @stack:       the parent's user stack pointer. if stack==0, It means to fork a kernel thread.
 * @tf:          the trapframe info, which will be copied to child process's proc->tf
 */
int
do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) {
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;
    if (nr_process >= MAX_PROCESS) {
        goto fork_out;
    }
    ret = -E_NO_MEM;
    //LAB4:EXERCISE2 YOUR CODE
    /*
     * Some Useful MACROs, Functions and DEFINEs, you can use them in below implementation.
     * MACROs or Functions:
     *   alloc_proc:   create a proc struct and init fields (lab4:exercise1)
     *   setup_kstack: alloc pages with size KSTACKPAGE as process kernel stack
     *   copy_mm:      process "proc" duplicate OR share process "current"'s mm according clone_flags
     *                 if clone_flags & CLONE_VM, then "share" ; else "duplicate"
     *   copy_thread:  setup the trapframe on the  process's kernel stack top and
     *                 setup the kernel entry point and stack of process
     *   hash_proc:    add proc into proc hash_list
     *   get_pid:      alloc a unique pid for process
     *   wakeup_proc:  set proc->state = PROC_RUNNABLE
     * VARIABLES:
     *   proc_list:    the process set's list
     *   nr_process:   the number of process set
     */

    //    1. call alloc_proc to allocate a proc_struct
    //    2. call setup_kstack to allocate a kernel stack for child process
    //    3. call copy_mm to dup OR share mm according clone_flag
    //    4. call copy_thread to setup tf & context in proc_struct
    //    5. insert proc_struct into hash_list && proc_list
    //    6. call wakeup_proc to make the new child process RUNNABLE
    //    7. set ret vaule using child proc's pid
    


//调用alloc_proc，分配进程控制块，名字叫proc
    if ((proc = alloc_proc()) == NULL) {
        goto fork_out;
    }
// 设置子进程的父进程为当前进程
    proc->parent = current;
//为进程分配一个内核栈,setup_kstack函数，分配一个大小KSTACKPAGE
// setup_kstack - alloc pages with size KSTACKPAGE as process kernel stack
/*
static int
setup_kstack(struct proc_struct *proc) {
    struct Page *page = alloc_pages(KSTACKPAGE);
    if (page != NULL) {
        proc->kstack = (uintptr_t)page2kva(page);
        return 0;
    }
    return -E_NO_MEM;
}*/
    if (setup_kstack(proc) != 0) {
        goto bad_fork_cleanup_proc;
    }
//依据参数clone_flags进行内存拷贝，本次实验似乎没用到
    if (copy_mm(clone_flags, proc) != 0) {
        goto bad_fork_cleanup_kstack;
    }
// 复制线程的状态，包括中断帧tf和上下文context等
//这里在新创建的进程内核栈上专门给进程的中断帧分配一块空间。
//构造新进程的中断帧的过程在kernel_thread函数里面
/*
// copy_thread - setup the trapframe on the  process's kernel stack top and
//             - setup the kernel entry point and stack of process
static void
copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf) {
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE - sizeof(struct trapframe));
    *(proc->tf) = *tf;

    // Set a0 to 0 so a child process knows it's just forked
    proc->tf->gpr.a0 = 0;
    proc->tf->gpr.sp = (esp == 0) ? (uintptr_t)proc->tf : esp;

    proc->context.ra = (uintptr_t)forkret;
    proc->context.sp = (uintptr_t)(proc->tf);
}
*/
    copy_thread(proc, stack, tf);

//关闭中断，分配 pid，将进程块链入哈希表和进程链表，最后恢复中断，一气呵成
//这里只介绍hash_proc函数，因为开关中断在challenge，get_rid在本题思考题会说到。
//其实这里只是多加了一个基于pid寻找在哈希表对应位置的过程
/*
// hash_proc - add proc into proc hash_list
static void
hash_proc(struct proc_struct *proc) {
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}
*/
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        proc->pid = get_pid();
        hash_proc(proc);
        list_add(&proc_list,&proc->list_link);
    }
    local_intr_restore(intr_flag);
//唤醒新进程，顾名思义就是把proc的state设为PROC_RUNNABLE态
/*
void
wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE);
    proc->state = PROC_RUNNABLE;
}
*/
    wakeup_proc(proc);
//新进程号赋给ret，用于函数返回
    ret = proc->pid;
    

fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}
```

关于思考题我们来看一下函数`get_id`函数。

```c
// get_pid - alloc a unique pid for process
static int
get_pid(void) {
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;
    //next_safe和last_pid两个变量，这里需要注意的是，它们是static全局变量，每一次调用这个函数的时候，这两个变量的数值之间的取值均是合法的 pid（也就是说没有被使用过）
    static int next_safe = MAX_PID, last_pid = MAX_PID;
    //++last_pid>=MAX_PID,说明pid分到了最后一个，需要从1重新再来
    if (++ last_pid >= MAX_PID) {
        last_pid = 1;
        goto inside;
    }
    if (last_pid >= next_safe) {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list; //le等于线程的链表头
        while ((le = list_next(le)) != list) {//这里相当于循环链表的操作
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid) {
                //如果proc的pid与last_pid相等，即这个pid被其他进程占用，则将last_pid加1，重新检查
                //如果又发生了last_pid+1 >= next_safe，next_safe就要变为最后一个，重新循环
                //当然这里如果在前两者基础上出现了last_pid>=MAX_PID,相当于直接从整个区间重新再来
                if (++ last_pid >= next_safe) {
                    if (last_pid >= MAX_PID) {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            }
            //如果last_pid<proc->pid<next_safe，确保最后能够找到这么一个满足条件的区间[last_pid,min(next_safe, proc->pid)) 尚未被占用，获得合法的pid范围，取区间第一个作为返回值
            else if (proc->pid > last_pid && next_safe > proc->pid) {
                next_safe = proc->pid;
            }
        }
    }
    return last_pid;
}
```

首先我们在头文件首先定义了
```C
#define MAX_PID                     (MAX_PROCESS * 2)
```
所以这里的断言不会出现部分PROCESS无ID可分的情况。
接着他的循环其实就是在构建了一个区间，通过确认没有进程的 id 在这个区间内部，来分配一个合适的 pid。
这里我们必须要注意的是next_safe和last_pid两个变量是静态的局部变量，他们其实在每一次函数调用前，是上一次保留的结果，也就意味着这两个变量的数值之间的取值均是合法的 pid。
然后这里分以下情况说明是唯一的：
- 在函数get_pid中，如果静态成员last_pid+1小于next_safe，则当前分配的last_pid+1一定是安全的，即唯一的PID，直接返回。具体理由就是上面的他其实是上一次调用的结果。
- 但如果last_pid+1大于等于next_safe，或者last_pid+1的值超过MAX_PID，则当前的last_pid就不一定是安全的PID，此时就需要遍历proc_list，去寻找新的合适的区间。if (proc->pid == last_pid)分支用来检查当前的 last_pid 是否已经被其他进程占用，如果已经被占用就last_pid加一在新的区间里重新检查，这里检查的基础就是新区间last_pid不能超过next_safe和MAX_PID，前者会让next_safe回到大区间的末端重新遍历链表，后者会在前者的基础上让last_pid回到大区间的开始，相当于回到了第一次初始化从MAX_PID个区间重新遍历链表。如果last_pid\<proc->pid<next_safe，我们就让next_safe（他这头是开区间）变成proc->pid，（这是因为last_pid到next_safe的区间存在了proc->pid这个已经被使用的值，所以我们要把区间的末端缩小到proc->pid前面，把它摒弃在外面），当我们彻底循环完一次链表后（这里就意味着完全遍历一次链表没有发生意外，否则就要回到repeat更改区间值重新遍历），得到的就是一个非常可靠的区间，同时也能为下一次使用。这个区间就是[last_pid,next_safe) 。
最后返回区间的第一个last_pid就是我们需要的。之所以返回第一个，是因为我们下一次不如意外的话可以直接加一使用第二个。

（这里不太好讲，讲的有点抽象，简单来说就是寻找一个区间，在能完全遍历完链表且不发生意外的情况下，不断缩小这个区间，得到就是安全区间）








### 练习3：编写proc_run 函数（需要编码）

proc_run用于将指定的进程切换到CPU上运行。它的大致执行步骤包括：

- 检查要切换的进程是否与当前正在运行的进程相同，如果相同则不需要切换。
- 禁用中断。你可以使用`/kern/sync/sync.h`中定义好的宏`local_intr_save(x)`和`local_intr_restore(x)`来实现关、开中断。
- 切换当前进程为要运行的进程。
- 切换页表，以便使用新进程的地址空间。`/libs/riscv.h`中提供了`lcr3(unsigned int cr3)`函数，可实现修改CR3寄存器值的功能。
- 实现上下文切换。`/kern/process`中已经预先编写好了`switch.S`，其中定义了`switch_to()`函数。可实现两个进程的context切换。
- 允许中断。

请回答如下问题：

- 在本实验的执行过程中，创建且运行了几个内核线程？




### 扩展练习 Challenge：

说明语句`local_intr_save(intr_flag);....local_intr_restore(intr_flag);`是如何实现开关中断的？




