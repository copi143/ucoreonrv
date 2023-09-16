#### 练习1: 使用GDB验证启动流程

为了熟悉使用qemu和gdb进行调试工作,使用gdb调试QEMU模拟的RISC-V计算机加电开始运行到执行应用程序的第一条指令（即跳转到0x80200000）这个阶段的执行过程，说明RISC-V硬件加电后的几条指令在哪里？完成了哪些功能？要求在报告中简要写出练习过程和回答。

##### 总述
从上电到0x80200000之间的指令太多了，所谓的“几条”指令也不知道是多少条，就只能简单分析前面几条，权当了解一下riscv的指令集

##### 上电到0x80000000
首先查看一下Qemu的源码：

+ target/riscv/cpu_bits.h
```C
/* Default Reset Vector adress */
#define DEFAULT_RSTVEC      0x1000
```

+ target/riscv/cpu.c
```C
static void riscv_any_cpu_init(Object *obj)
{
    CPURISCVState *env = &RISCV_CPU(obj)->env;
    set_misa(env, RVXLEN | RVI | RVM | RVA | RVF | RVD | RVC | RVU);
    set_priv_version(env, PRIV_VERSION_1_11_0);
    set_resetvec(env, DEFAULT_RSTVEC);
}
...
#ifndef CONFIG_USER_ONLY
    env->priv = PRV_M;
    env->mstatus &= ~(MSTATUS_MIE | MSTATUS_MPRV);
    env->mcause = 0;
    env->pc = env->resetvec;
```

+ hw/riscv/virt.c
```C
riscv_find_and_load_firmware(machine, BIOS_FILENAME,
                                 memmap[VIRT_DRAM].base);//memmap[VIRT_MROM].base=0x80000000
...
rom_add_blob_fixed_as("mrom.reset", reset_vec, sizeof(reset_vec),
                          memmap[VIRT_MROM].base, &address_space_memory);
```

>可以看到，Qemu源码里DEFAULT_RSTVEC的值被定义为0x1000，并赋给了PC，然后将复位代码放到了内置ROM的指定位置处，即0x1000，所以上电后QEMU模拟的这款riscv处理器的复位地址是0x1000。而在此之前，bootloader被加载到了0x80000000处，因此复位代码会跳转到这个地址开始执行bootloader。

接着我们用gdb调试，查看CPU上电后待执行的10条riscv汇编指令
```C
(gdb) x/10i $pc
=> 0x1000:	auipc	t0,0x0
   0x1004:	addi	a1,t0,32
   0x1008:	csrr	a0,mhartid
   0x100c:	ld	t0,24(t0)
   0x1010:	jr	t0
   0x1014:	unimp
   0x1016:	unimp
   0x1018:	unimp
   0x101a:	0x8000
   0x101c:	unimp
```

上述代码分析如下：

|          指令       | 分析                                                               
|---------------------|------------------------------------------------------------------|
|auipc   t0,0x0       | 将当前 PC 寄存器的高 20 位加上立即数 0x0，并将结果存储到寄存器 t0 中，用于生成绝对地址              |
|addi    a1,t0,32     | 将寄存器 t0 的值加上立即数 32，并将结果存储到寄存器 a1 中                               |
|csrr    a0,mhartid   | 从特权状态寄存器 (CSR) 中读取 hartid（硬件线程 ID）的值，并将结果存储到寄存器 a0 中，用于获取当前硬件线程的 ID |
|ld      t0,24(t0)    | 从以寄存器 t0 为基址偏移为 24 的内存位置处加载一个双字，并将结果存储到寄存器 t0 中                 |
|jr      t0           | 将控制转移到寄存器 t0 中存储的目标地址处                                           |

单步执行，查看相应寄存器的值
```C
(gdb) si
0x0000000000001004 in ?? ()
(gdb) si
0x0000000000001008 in ?? ()
(gdb) si
0x000000000000100c in ?? ()
(gdb) info r t0
t0             0x0000000000001000	4096
(gdb) x/1xw 0x1018
0x1018:	0x80000000
(gdb) si
0x0000000000001010 in ?? ()
(gdb) info r t0
t0             0x0000000080000000	2147483648
(gdb) si
0x0000000080000000 in ?? ()

```
>我们可以看到，由于 t0 中存放的值为0x1000，因而 24(t0) 的地址即为0x1018，而0x1018处存放的值刚好等于0x80000000，所以 ld	t0,24(t0) 从中读出0x80000000放到 t0 寄存器，并在下一步跳转到这个地址


##### 0x80000000到0x80000046

```C
//读取当前核心(hart)号
0x80000000 csrr a6,mhartid 
//如果当前核心号大于0，则跳转到0x80000108
0x80000004 bgtz a6,0x80000108 
```

>上述代码是让0号核心执行以下操作，而剩下的核心执行其它功能，推测是让其它核心休眠。但0x80000108处的指令也比较复杂，而且该程序其实用不到其它核心(打断点在0x80000108处，但程序并没有停在这里，而是跑到了结尾)


```C
//auipc是将20位立即数有符号拓展为32位，然后加上PC的值，这里
//的立即数是0，故而相当于将当前PC的值(即：0x80000008)装入t0
0x80000008 auipc t0,0x0
//计算得到此时t0的值为0x80000410     (其实是没算，直接看gdb提供的寄存器信息···)
0x8000000c addi t0,t0,1032
//以下两条指令，将t1设置为0x80000000
0x80000010 auipc t1,0x0 
0x80000014 addi t1,t1,-16
//将0x80000000存入0x80000410处
0x80000018 sd t1,0(t0)
//以下三条指令，将0x80000418处的值装入t0
0x8000001c auipc t0,0x0 
0x80000020 addi t0,t0,1020 
0x80000024 ld t0,0(t0)
//以下三条指令，将0x80000420处的值装入t1
0x80000028 auipc t1,0x0 
0x8000002c addi t1,t1,1016 
0x80000030 ld t1,0(t1) 
//以下三条指令，将0x80000410处的值装入t2(其实就是刚刚装入的0x80000000)
0x80000034 auipc t2,0x0 
0x80000038 addi t2,t2,988 
0x8000003c ld t2,0(t2) 

0x80000040 sub t3,t1,t0
0x80000044:  add     t3,t3,t2

//t0如果还等于0x80000000，则跳转到0x8000014e
0x80000046:  beq     t0,t2,0x8000014e
```

>简而言之，其实上述代码就做了一件事儿：看0x80000418处的一个dword是不是0x80000000，如果是，就跳到0x8000014e
