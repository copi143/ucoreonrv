#### 练习1：理解内核启动中的程序入口操作

阅读 kern/init/entry.S内容代码，结合操作系统内核启动流程，说明指令 la sp, bootstacktop 完成了什么操作，目的是什么？ tail kern_init 完成了什么操作，目的是什么？

##### 简单阅读
```C
#include <mmu.h>
#include <memlayout.h>

    .section .text,"ax",%progbits
    .globl kern_entry
kern_entry:
    la sp, bootstacktop

    tail kern_init

.section .data
    # .align 2^12
    .align PGSHIFT
    .global bootstack
bootstack:
    .space KSTACKSIZE
    .global bootstacktop
bootstacktop:
```

上述是entry.S的代码，入口点kern_entry处的地址为0x80200000，对应的汇编指令如下：
```
0x80200000 <kern_entry> auipc sp,0x4 
0x80200004 <kern_entry+4> mv sp,sp 
0x80200008 <kern_entry+8> j 0x8020000c <kern_init>
```
la sp, bootstacktop对应auipc sp, 0x4，将sp变为0x80204000，
tail kern_init对应j 0x802000c，跳转到kern_init部分代码

>riscv说明书中有对tail的说明：
>tail symbol pc = &symbol; clobber x[6]
>尾调用(Tail call). 伪指令(Pseudoinstuction), RV32I and RV64I.
>设置 pc 为 symbol，同时覆写 x[6]。实际扩展为 auipc x6, offsetHi 和 jalr x0,
>offsetLo(x6)。

8001bd80
##### 进阶疑问
看似这两个指令都很好理解，那么，为什么bootstacktop所在位置是0x80204000？首先，我们得看包含的两个头文件mmu.h以及memlayout.h，它们的内容如下：

mmu.h
```C
#ifndef __KERN_MM_MMU_H__
#define __KERN_MM_MMU_H__

#define PGSIZE          4096                    // bytes mapped by a page
#define PGSHIFT         12                      // log2(PGSIZE)

#endif /* !__KERN_MM_MMU_H__ */
```
memlayout.h
```C
#ifndef __KERN_MM_MEMLAYOUT_H__
#define __KERN_MM_MEMLAYOUT_H__

#define KSTACKPAGE          2                           // # of pages in kernel stack
#define KSTACKSIZE          (KSTACKPAGE * PGSIZE)       // sizeof kernel stack

#endif /* !__KERN_MM_MEMLAYOUT_H__ */
```
从这两个头文件可知，PGSHIFT为12，KSTACKSIZE为8KB(对应十六进制2000)
回到entry.S，`.align PGSHIFT`意味着对齐设置为12，即对齐4KB($2^{12}$)的边缘，`.space KSTACKSIZE`则意味着开辟8KB的空间。但往最大可能说，就算对齐需要填充整整4KB空间，加上8KB的空间，也到达0x3000而不是0x4000，所以这个问题还没有解决。

接下来需要注意的是`.section`助记符，entry.S中有两个`.section`，一个是代码段(.text)，一个是数据段(.data)。通过`.section .text`提示汇编器和链接器，先在这一部分的位置填装满指令。因此，从0x80200000开始往后的大段内容都是指令，我们也可以看到，kern_init的位置正是0x8020000c，也就正好在跳转指令之后而已。再次观察程序，看到代码终结的位置大致在0x802010f8处，此时对齐4KB正好到0x80202000，再开辟8KB空间恰好是0x80204000。
如下：
![指令终结处](lab1img1.png)

#### 练习2：完善中断处理 （需要编程）

请编程完善trap.c中的中断处理函数trap，在对时钟中断进行处理的部分填写kern/trap/trap.c函数中处理时钟中断的部分，使操作系统每遇到100次时钟中断后，调用print_ticks子程序，向屏幕上打印一行文字”100 ticks”，在打印完10行后调用sbi.h中的shut_down()函数关机。
要求完成问题1提出的相关函数实现，提交改进后的源代码包（可以编译执行），并在实验报告中简要说明实现过程和定时器中断中断处理的流程。实现要求的部分代码后，运行整个系统，大约每1秒会输出一次”100 ticks”，输出10行。

其实lab1的文档已经描述得很详细了，具体代码也已经上传到gitee，这里总结一下思路：
总体分为三个阶段：
1. 每10ms触发一次时钟中断；
2. 每100次时钟中断，调用一次print_ticks()
3. 调用10次print_ticks后，调用sbi_shutdown()关机
后面两项十分简单，在处理程序中调用两个变量计数就行(ticks和num)，关键在于第一项，如何每10ms触发一次时钟中断？
`sbi_set_timer()`接口，可以传入一个时刻，让它在那个时刻触发一次时钟中断，但是我们需要的是“每10ms”——即时间间隔，而该接口只接受“时刻”。
解决这个问题需要`rdtime`伪指令，根据32位和64位的区别，将其封装在函数`get_cycles`中，通过其获得从运行开始到目前执行的时间。
因此，只需要sbi_set_timer(get_cycles+10ms)就能为下一个10ms设置中断了。将该段代码放在处理函数中，就能实现每次中断都为下一个10ms设置中断。
现在，只需要让“石头滚下”——设置第一个中断。在kern_init()中调用clock_init()，清空ticks并设置第一个时钟中断，这样就完成了任务。


#### 扩展练习 Challenge1：描述与理解中断流程

回答：描述ucore中处理中断异常的流程（从异常的产生开始），其中mov a0，sp的目的是什么？SAVE_ALL中寄寄存器保存在栈中的位置是什么确定的？对于任何中断，__alltraps 中都需要保存所有寄存器吗？请说明理由。

#### 扩增练习 Challenge2：理解上下文切换机制

回答：在trapentry.S中汇编代码 csrw sscratch, sp；csrrw s0, sscratch, x0实现了什么操作，目的是什么？save all里面保存了stval scause这些csr，而在restore all里面却不还原它们？那这样store的意义何在呢？

#### 扩展练习Challenge3：完善异常中断

编程完善在触发一条非法指令异常 mret和，在 kern/trap/trap.c的异常处理函数中捕获，并对其进行处理，简单输出异常类型和异常指令触发地址，即“Illegal instruction caught at 0x(地址)”，“ebreak caught at 0x（地址）”与“Exception type:Illegal instruction"，“Exception type: breakpoint”。（