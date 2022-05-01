struct buf;
struct context;
struct file;
struct inode;
struct pipe;
struct proc;
struct spinlock;
struct sleeplock;
struct stat;
struct superblock;



// number of elements in fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))
