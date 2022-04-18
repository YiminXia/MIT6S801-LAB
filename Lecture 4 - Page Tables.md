# Lecture 4 - Page Tables

[toc]

## 1 main object of this course

* address space
* RISC-V Hardware about page tables
* the source code about page tables in xv-6 system

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

这个图画的很好，github上面的课程翻译关于下面这个图的，要好好看下并抄写，核心思想是每个进程都有自己的mapping（va->pa的映射关系），这个mapping是给MMU或者CPU这些具有地址翻译功能的硬件用的，每个进程的这个mapping也是存在内存里面的，其root address存在每个process的satp寄存器里面，这里很好的印证了之前的思想，process是CPU的抽象。

![image-20220419001304078](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220419001304078.png)