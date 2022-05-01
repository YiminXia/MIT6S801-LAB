#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void wakeup1(struct proc *chan);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// initialize the proc table at boot time.
void
procinit(void)
{
    struct proc *p;

    initlock(&pid_lock, "nextpid");
    for(p = proc; p < &proc[NPROC]; p++) {
        initlock(&p->lock, "proc");

        // allocate a page for the process's kernel stack.
        // Map it high in memory, followed by an invalid
        // guard page.
        char *pa = kalloc();
        if(pa == 0)
            panic("kalloc");
        uint64 va = KSTACK((int) (p - proc));
        kvmmap(va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
        p->kstack = va;
    }
    kvminithart();
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU
int 
cpuid()
{
    int id = r_tp();
    return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu *
mycpu(void) {
    int id = cpuid();
    struct cpu *c = &cpus[id];
    return c;
}

// Return the current struct proc *, or zero if none.
struct proc *
myproc(void) {
    push_off();
    struct cpu *c = mycpu();
    struct proc *p = c->proc;
    pop_off();
    return p;
}

int
allocpid() {
    int pid;

    acquire(&pid_lock);
    pid = nextpid;
    nextpid = nextpid + 1;
    release(&pid_lock);

    return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0
static struct proc*
allocproc(void)
{
    struct proc *p;

    for(p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if(p->state == UNUSED) {
            goto found;
        } else {
            release(&p->lock);
        }
    }
    return 0;
    
found:
    p->pid = allocpid();

    // Allocate a trapfram page
    if((p->trapframe = (struct trapframe *)kalloc()) == 0) {
        release(&p->lock);
        return 0;
    }

    // An empty user page table.
    p->pagetable = proc_pagetable(p);
    if(p->pagetable == 0) {
        freeproc(p);
        release(&p->lock);
        return 0;
    }

    // set up new context to start executing at forkret,
    // which returns to user space.
    memset(&p->context, 0, sizeof(p->context));
    p->context.ra = (uint64)forkret;
    p->context.sp = p->kstack + PGSIZE;

    return p;
}

