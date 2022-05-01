// Mutual exclusion lock.
struct spinlock {
    uint locked;    // Is the lock held?

    // For debugging:
    char *name;     // name of lock.
    struct cpu *cpu;// The cpu holding the lock.
};
