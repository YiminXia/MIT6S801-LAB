// physical memory layout

// qemu -machine virt is set up like this,
// based on qemu's hw/riscv/virt.c:

// 00001000 -- boot ROM, provided by qemu boot只读内存
// 02000000 -- CLINT
// 0C000000 -- PLIC
// 10000000 -- uart0
// 10001000 -- virtio disk
// 80000000 -- boot ROM jumps here in machine mode //machine mode的boot ROM跳到这里
//             -kernel loads the kernel here // 内核在这里80000000加载内核源码
// unused RAM after 80000000.

// the kernel uses physical memory thus:
// 80000000 -- entry.S, then kernel text, kernel data
// end      -- start of kernel page allocation area
// PHYSTOP  -- end RAM used by the kernel

//----------------------------------------------------------------
// core local interruptor (CLINT), which contains the timer. 核心本地中断器？
#define CLINT                   0x02000000L
#define CLINT_MTIMECMP(hartid)  (CLINT + 0x4000 + 8*(hartid))
#define CLINT_MTIME             (CLINT + 0xBFF8)

//qemu puts platform-level interrupt controller (PLIC) here. 平台级中断控制器？
#define PLIC                    0x0C000000L
#define PLIC_PRIORITY           (PLIC + 0x0)
#define PLIC_PENDIND            (PLIC + 0x1000)
#define PLIC_MENABLE(hart)      (PLIC + 0x2000 + (hart)*0x100)
#define PLIC_SENABLE(hart)      (PLIC + 0x2080 + (hart)*0x100)
#define PLIC_MPRIORITY(hart)    (PLIC + 0x200000 + (hart)*0x2000)
#define PLIC_SPRIORITY(hart)    (PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_MCLAIM(hart)       (PLIC + 0x200004 + (hart)*0x2000)
#define PLIC_SCLAIM(hart)       (PLIC + 0x201004 + (hart)*0x2000)

// qemu puts UART registers here in physical memory         UART寄存器
#define UART0                   0x10000000L
#define UART0_IRQ               10

// virtio mmio interface                                    virtio mmio接口
#define VIRTIO0                 0x10001000
#define VIRTIO0_IRQ             1

// the kernel expects there to be RAM
// for use by the kernel and user pages
// from physical address 0x80000000 to PHYSTOP
#define KERNBASE                0x80000000L
#define PHYSTOP                 (KERNBASE + 128*1024*1024)

// map the trampoline page to the highest address,
// in both user and kernel space.
#define TRAMPOLINE              (MAXVA - PGSIZE)

// map kernel stacks beneath the trampoline,
// each surrounded by invalid guard pages.
#define KSTACK(p)               (TRAMPOLINE - ((p)+1)* 2*PGSIZE)

// User memory layout.
//  TRAMPOLINE (the same page as in the kernel)
//  TRAPFRAME (p->trapframe, used by the trampoline)
//  ...
//  expandable heap
//  fixed-size stack
//  original data and bss
//  text
// Address zero first:
#define TRAPFRAME               (TRAMPOLINE - PGSIZE)