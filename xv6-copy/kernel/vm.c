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
pagetable_t kernel_pagetable;






// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int 
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
    

}

