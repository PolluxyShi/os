#include <common.h>
#include <spinlock.h>

#ifndef _OS_H
#define _OS_H

#define MAX_CPU 8

#define LEN(arr) ((sizeof(arr) / sizeof(arr[0])))

#define new(A) (typeof(A)*)pmm->alloc(sizeof(A))

#define STK_SZ ((1<<12)-64)

#define TASK_RUNABLE 0
#define TASK_SLEEP 1
#define TASK_RUNNING 2

struct task {
    // TODO
    volatile uint32_t attr,ncli,intena,cpu;
    spin_lock_t running;
    char* name;
    Context context;
    uint8_t stack[STK_SZ];
    struct{}stack_end;
};

struct spinlock {
    // TODO
    uint32_t reen;
    task_t* owner;
    spin_lock_t locked;
    char *name;
};

#define POOL_LEN 20
struct semaphore {
  // TODO
    char *name;
    volatile int value;
    spin_lock_t lock;
    task_t *pool[POOL_LEN];
    volatile int head,tail;
};

#endif