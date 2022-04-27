# Lecture 4 - Page Tables

[toc]

## 1 main object of this course

* **address space**
* **RISC-V Hardware about page tables**
* **the source code about page tables in xv-6 system**

## 2 address space

* address space的作用是什么？isolation(e.g.cat进程可能scribbling over shell进程的memory image)，但是如果每个进程，包括用户态进程与kernel（这里kernel就是个大进程）都拥有自己的独立的address space就可以避免这个问题，各自独立的address space的地址区间从[0, N]；N的大小与实际物理地址的最大值M没有必然联系。

* 联系Lab2里面的sysinfo作业，让计算空闲页表的大小（单位bytes）；会发现xv6内核负责这个的结构体是这个玩意儿

  ```c
  struct {
    struct spinlock lock;
    struct run *freelist;
  } kmem;
  ```

* how basically multiplex all these address spaces across a single physical memory?-Page Tables

这个图画的很好，github上面的课程翻译关于下面这个图的，要好好看下并抄写，核心思想是每个进程都有自己的`mapping（va--->pa的映射关系）`，这个`mapping`是给`MMU`或者`CPU`这些具有地址翻译功能的硬件用的，每个进程的这个`mapping`也是存在内存里面的，其`root address`存在每个`process`的`satp`寄存器里面，这里很好的印证了之前的思想，`process`是`CPU`的抽象。

![image-20220419190658145](.assets/image-20220419190658145.png)

## 3 RISC-V Hardware about page tables

### VA到PA的换算公式：

虚拟地址里面的`index`负责映射到`PPN(physical page number)`也就是映射到具体的`physical page`，`offset`则表示在`physical page`页中的具体偏移。

![image-20220419194311899](.assets/image-20220419194311899.png)

### `index`是`27bit`的原因：

`index`的作用是找到`PPN(physical page number)`，三级页表每一级`9bit`，详见

### `offset`是`12bit`的原因：

一个物理页的大小是`4096byte`正好是`2^12`，所以`offset`作为页内偏移必须`12bit`，这里可以知道物理页是一段在物理上连续的`4096byte`大小的内存，如果不连续那么`offset`就没有意义了。

### `PPN`是`44bit`的原因，同时也是物理地址`56bit`的原因：

Frans教授：这是由硬件设计人员决定的。所以RISC-V的设计人员认为`56bit`的物理内存地址是个不错的选择。可以假定，他们是通过技术发展的趋势得到这里的数字。比如说，设计是为了满足5年的需求，可以预测物理内存在5年内不可能超过`2^56`这么大。或许，他们预测是的一个小得多的数字，但是为了防止预测错误，他们选择了像`2^56`这么大的数字。这里说的通吗？很多同学都问了这个问题。

**硬件设计人员为啥不直接选择`64bit`作为物理地址，非要选择`56bit`：**

Frans教授：选择`56bit`而不是`64bit`是因为在主板上只需要`56`根线。

### 物理页表必须`4K`对齐的原因（未解决）:

这里加一个三级页面转换图，地址转换完全由硬件完成，比如MMU或者CPU，但是XV6需要自己去实现这个遍历的流程，为什么？

### 转译后备缓存器(Translation Lookaside Buffer TLB)是什么:

对于OS来说，switch page table 等价于 flush TLB(RISC-V命令是sfence_VMA)

## 4 Page Tables in xv6

### 遍历页表翻译VA是HW的任务，我们干嘛要写呢?

Frans教授：这是个好问题，有好2个原因使得我们必须了解遍历过程并编写遍历功能：

* xv6在初始化page tables功能时候，它需要对3级page table进行编程所以它首先需要能模拟3级page tables。

* 另一个原因就是在sysinfo实验中遇到的，在xv6系统中，kernel拥有自己的`Kernel Page Tables`，每一个用户态进程同样拥有自己的`user Page Tables`，在实验中`struct sysinfo`存在于用户态进程`sysinfotest`的`user address space`。

  所以`kernel`必须翻译这个`struct sysinfo`结构体的`VA->PA`，然后才能读写她，这一过程相关函数`copyin,copyout`。实际过程是`kernel`使用用户态程序的`user Page Tables`将她得`VA->PA`，以便供`kernel`自己读写。

### Page Tables provides level of indirection

`Page Tables`提供了某种间接性，指的是`VA->PA`的映射，而这个映射完全由`kernel`控制，这样就导致了`kernel`可以做很多骚操作：

* 比如`kernel`在执行一条命令时候，发现一个`PTE`是无效的，`HW`会返回给`kernel`一个`page fault`，此时`kernel`可以更新这个`PTE`然后重新执行该命令。

