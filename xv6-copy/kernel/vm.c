#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"

/*
 * the kernel's page table
 */
pagetable_t kernel_pagetable;   //typedef uint64 *    pagetable_t

extern char etext[];            // kernel.ld sets this to end of kernel code.

extern char trampoline[];       // trampoline.S

// Make a direct-map page table for the kernel
pagetable_t
kvmmake(void)
{
    pagetable_t kpgtbl;

    kpgtbl = (pagetable_t) kalloc();
    memset(kpgtbl, 0, PGSIZE);

    // uart registers
    kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

    // virtio mmio disk interface
    kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

    // PLIC
    kvmmap(kpgtbl, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

    // map kernel text executable and read-only
    kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

    // map kernel data and the physical RAM we will make use of
    kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

    // map the trampoline for trap entry/exit to
    // the hightest virtual address in the kernel
    kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

    // map kernel stacks
    proc_mapstacks(kpgtbl);

    return kpgtbl;
}

// Initialize the one kernel_pagetable
void
kvminit(void) 
{
    kernel_pagetable = kvmmake();
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
void
kvminithart()
{
    w_satp(MAKE_SATP(kernel_pagetable));
    sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.
// If alloc != 0, create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table pages.
// A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields.
//  39..63 -- must be zero.
//  30..38 -- 9 bits of level-2 index.
//  21..29 -- 9 bits of level-1 index.
//  12..20 -- 9 bits of level-0 index.
//   0..11 -- 12bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
    // 入参pagetable想象成页表的root physical address
    if (va >= MAXVA)
        panic("walk");
    for(int level = 2; level > 0; level--) {
        pte_t *pte = &pagetable[PX(level, va)];
        if(*pte & PTE_V) {
            pagetable = (pagetable_t)PTE2PA(*pte);
        } else {
            if(!alloc || (pagetable = (pagetable_t)kalloc()) == 0)
                return 0;
            memset(pagetable, 0, PGSIZE);
            *pte = (pte_t)PA2PTE(pagetable) | PTE_V;
        }
    }
    return &pagetable[PX(0, va)];
}


// Look up a virtual address, return the physical address, or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
    pte_t   *pte;
    uint64  pa;

    if(va >= MAXVA)
        return 0;

    pte = walk(pagetable, va, 0);
    if(pte == 0)
        return 0;
    if((*pte & PTE_V) == 0)
        return 0;
    if((*pte & PTE_U) == 0)
        return 0;

    pa = PTE2PA(*pte);// *pte就是一个PTE的实体，PTE2PA就是取出第三级page table的物理页面的root addr,即pa所在页面的root addr,并非pa
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void
kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
    if(mappages(kpgtbl, va, sz, pa, perm) != 0)
        panic("kvmmap");
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa.
// va and size might not be page-aligned.
// Returns 0 on success, -1 if walk() couldn't allocate a needed page-table page.
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
    uint64 a, last;
    pte_t *pte;

    if(size == 0)
        panic("mappages: size");

    a = PGROUNDDOWN(va);
    last = PGROUNDDOWN(va + size - 1);
    for(;;) {
        if((pte = walk(pagetable, a, 1)) == 0) // walk完成三级页表与相应PTE的创建
            return -1;
        if(*pte & PTE_V)
            panic("mappages: remap");
        *pte = PA2PTE(pa) | perm | PTE_V; // 第三级page table中PTE的填充，核心内容是pa的root addr
        if(a == last)
            break;
        a += PGSIZE;
        pa += PGSIZE;
    }
    return 0;
}

// Remove npages of mapping starting from va. 
// va must be page-aligned. The mapping must exist.
// Optionally free the physical memory
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
    uint64   a;
    pte_t   *pte;

    if((va % PGSIZE) != 0)
        panic("uvmunmap: not page-aligned");

    for(a = va; a < va + npages * PGSIZE; a = a + PGSIZE) {
        if((pte = walk(pagetable, a, 0)) == 0)
            panic("uvmunmap: walk");
        if((*pte & PTE_V) == 0)
            panic("uvmunmap: not mapped");
        if(PTE_FLAGS(*pte) == PTE_V)
            panic("uvmunmap: not a leaf"); //为什么只有PTE_V为非叶子节点呢？
        if(do_free) {
            uint64 pa = PTE2PA(*pte);
            kfree((void *)pa);
        }
        *pte = 0;
    }

}

// create an empty user page table.
// returns 0 if out of memory.
pagetable_t
uvmcreate()
{
    pagetable_t pagetable;
    pagetable = (pagetable_t)kalloc();
    if(pagetable == 0)
        return 0;
    memset(pagetable, 0, PGSIZE);
    return pagetable; //这里返回的是物理页面的root physical address.
}

// Load the user initcode into address 0 of pagetable.
// for the very first process.
// sz must be less than a page.
void
uvminit(pagetable_t pagetable, uchar *src, uint sz)
{
    char *mem;

    if(sz >= PGSIZE)
        panic("inituvm: more than a page");
    mem = kalloc();
    memset(mem, 0, PGSIZE);
    mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U); // 这里的(uint64)入参强转，联系DPDK1911写个总结
    memmove(mem, src, sz);
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int 
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
    

}

