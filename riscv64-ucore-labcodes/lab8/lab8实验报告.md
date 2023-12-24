## 练习0：填写已有实验
本实验依赖实验2/3/4/5/6/7。请把你做的实验2/3/4/5/6/7的代码填入本实验中代码中有“LAB2”/“LAB3”/“LAB4”/“LAB5”/“LAB6” /“LAB7”的注释相应部分。并确保编译通过。注意：为了能够正确执行lab8的测试应用程序，可能需对已完成的实验2/3/4/5/6/7的代码进行进一步改进。

本次实验练习0无需修改已完成实验的代码，但由于lab6和lab7没有要求，还是有必要简单说明有关这两个实验需要填写的部分。

### lab6
lab6是关于ucore的系统调度器框架，需要基于此实现一个Stride Scheduling调度算法，简言之，这个算法能够让**每个进程得到的时间资源与它们的优先级成正比关系**，从而更加智能地为每个进程分配合理的CPU资源。

首先在lab5的基础上修改`alloc_proc`函数的初始化：
```C
// kern/process/proc.c
static struct proc_struct * alloc_proc(void) {
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL) {
        ...
        // Lab6 code
        proc->rq = NULL; //初始化运行队列为空
        list_init(&(proc->run_link)); 
        proc->time_slice = 0; //初始化时间片
        proc->lab6_run_pool.left = proc->lab6_run_pool.right = proc->lab6_run_pool.parent = NULL;//初始化指针为空
        proc->lab6_stride = 0;    //设置步长为0
        proc->lab6_priority = 0;  //设置优先级为0
    }
    return proc;
}
```

然后是Stride Scheduling算法的实现部分，主要在kern/schedule/default_sched_stride.c中完成，需要实现的部分如下：

**为BIG_STRIDE赋值**
```C
#define BIG_STRIDE  (1 << 30) /* you should give a value, and is ??? */
```

**初始化运行队列**
```C
static void stride_init(struct run_queue *rq) {
     list_init(&(rq->run_list));
     rq->lab6_run_pool = NULL;
     rq->proc_num = 0;
}
```

**将进程加入就绪队列**
```C
static void
stride_enqueue(struct run_queue *rq, struct proc_struct *proc) {
    //将进程加入就绪队列
     rq->lab6_run_pool = skew_heap_insert(rq->lab6_run_pool, &(proc->lab6_run_pool), proc_stride_comp_f);
     if (proc->time_slice == 0 || proc->time_slice > rq->max_time_slice) {
        proc->time_slice = rq->max_time_slice;
     }
     proc->rq = rq;
     rq->proc_num++;
}
```

**将进程从就绪队列中移除**
```C
static void
stride_dequeue(struct run_queue *rq, struct proc_struct *proc) {
     assert(proc->rq == rq && rq->proc_num > 0);
     rq->lab6_run_pool = skew_heap_remove(rq->lab6_run_pool, &(proc->lab6_run_pool), proc_stride_comp_f);
     rq->proc_num--;
}
```

**选择进程调度**
```C
static struct proc_struct* stride_pick_next(struct run_queue *rq) {
     if (rq->lab6_run_pool == NULL) {
        return NULL;
     }
     struct proc_struct* proc = le2proc(rq->lab6_run_pool, lab6_run_pool);
   
     if (proc->lab6_priority != 0) {//优先级不为0
        //步长设置为优先级的倒数
        proc->lab6_stride += BIG_STRIDE / proc->lab6_priority;
     } 
     else {
        //步长设置为最大值
        proc->lab6_stride += BIG_STRIDE;
     }
     return proc;
}
```

**时间片部分**
```C
static void
stride_proc_tick(struct run_queue *rq, struct proc_struct *proc) {
     if (proc->time_slice > 0) {//到达时间片
        proc->time_slice--;//执行进程的时间片减一
     }
     if (proc->time_slice == 0) {//时间片为0
        //设置此进程成员变量need_resched标识为1,进程需要调度
        proc->need_resched = 1;
     }
}
```

### lab7
lab7是关于ucore中同步互斥机制的实现，需要完成内核级条件变量和基于内核级条件变量的哲学家就餐问题。

首先是基于信号量实现完成条件变量实现，主要填充的是kern/sync/monitor.c中的两个函数`cond_signal`和`cond_wait`，前者用于等待某一个条件，后者提醒某一个条件已经达成。

```C
void cond_signal (condvar_t *cvp) {
   cprintf("cond_signal begin: cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
     if(cvp->count>0) { //当前存在执行cond_wait而睡眠的进程 
        cvp->owner->next_count ++; //睡眠的进程总个数加一  
        up(&(cvp->sem)); //唤醒等待在cv.sem上睡眠的进程 
        down(&(cvp->owner->next)); //自己需要睡眠
        cvp->owner->next_count --; //睡醒后等待此条件的睡眠进程个数减一
      }
   cprintf("cond_signal end: cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
}
```

```C
void cond_wait (condvar_t *cvp) {
    cprintf("cond_wait begin:  cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
      cvp->count++; //需要睡眠的进程个数加一
      if(cvp->owner->next_count > 0) 
         up(&(cvp->owner->next)); //唤醒进程链表中的下一个进程
      else
         up(&(cvp->owner->mutex)); //唤醒睡在monitor.mutex上的进程 
      down(&(cvp->sem));  //将此进程等待  
      cvp->count --;  //睡醒后等待此条件的睡眠进程个数减一
    cprintf("cond_wait end:  cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
}

```

然后是用管程机制解决哲学家就餐问题，要补充的是kern/sync/check_sync.c中的两个函数`phi_take_forks_condvar`和`phi_put_forks_condvar`。

哲学家先尝试获取刀叉，如果刀叉没有获取到，则等待刀叉。
```C
void phi_take_forks_condvar(int i) {
    down(&(mtp->mutex));  //通过P操作进入临界区
    state_condvar[i]=HUNGRY; //记录哲学家i是否饥饿
    phi_test_condvar(i);   //试图拿到叉子 
    if (state_condvar[i] != EATING) {
        cprintf("phi_take_forks_condvar: %d didn't get fork and will wait\n",i);
        cond_wait(&mtp->cv[i]); //得不到叉子就睡眠
    }
    if(mtp->next_count>0)  //如果存在睡眠的进程则那么将之唤醒
        up(&(mtp->next));
    else
        up(&(mtp->mutex));
}
```

当哲学家放下刀叉时，如果左右两边的哲学家都满足条件可以进餐，则设置对应的条件变量。
```C
void phi_put_forks_condvar(int i) {
    down(&(mtp->mutex)); ;//通过P操作进入临界区
    state_condvar[i]=THINKING; //记录进餐结束的状态
    phi_test_condvar(LEFT); //看一下左边哲学家现在是否能进餐
    phi_test_condvar(RIGHT); //看一下右边哲学家现在是否能进餐
    if(mtp->next_count>0) //如果有哲学家睡眠就予以唤醒
        up(&(mtp->next));
    else
        up(&(mtp->mutex)); //离开临界区
}
```

## 练习1: 完成读文件操作的实现（需要编码）
首先了解打开文件的处理流程，然后参考本实验后续的文件读写操作的过程分析，填写在 kern/fs/sfs/sfs_inode.c中 的sfs_io_nolock()函数，实现读文件中数据的代码。

## 练习2: 完成基于文件系统的执行程序机制的实现（需要编码）
改写proc.c中的load_icode函数和其他相关函数，实现基于文件系统的执行程序机制。执行：make qemu。如果能看看到sh用户程序的执行界面，则基本成功了。如果在sh用户界面上可以执行”ls”,”hello”等其他放置在sfs文件系统中的其他执行程序，则可以认为本实验基本成功。

## 扩展练习 Challenge1：完成基于“UNIX的PIPE机制”的设计方案
如果要在ucore里加入UNIX的管道（Pipe）机制，至少需要定义哪些数据结构和接口？（接口给出语义即可，不必具体实现。数据结构的设计应当给出一个（或多个）具体的C语言struct定义。在网络上查找相关的Linux资料和实现，请在实验报告中给出设计实现”UNIX的PIPE机制“的概要设方案，你的设计应当体现出对可能出现的同步互斥问题的处理。）

## 扩展练习 Challenge2：完成基于“UNIX的软连接和硬连接机制”的设计方案
如果要在ucore里加入UNIX的软连接和硬连接机制，至少需要定义哪些数据结构和接口？（接口给出语义即可，不必具体实现。数据结构的设计应当给出一个（或多个）具体的C语言struct定义。在网络上查找相关的Linux资料和实现，请在实验报告中给出设计实现”UNIX的软连接和硬连接机制“的概要设方案，你的设计应当体现出对可能出现的同步互斥问题的处理。）