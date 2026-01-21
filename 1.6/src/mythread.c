#define _GNU_SOURCE
#include <sys/types.h>
#include <errno.h>
#include <malloc.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <unistd.h>

#include "string.h"

#include "queue.h"

/* Futex syscalls wrappers */

static int futex_wait(atomic_int *addr, int expected) {
    return syscall(SYS_futex, (int *)addr, FUTEX_WAIT, expected, NULL, NULL, 0);
}

static int futex_wake(atomic_int *addr, int n) {
    return syscall(SYS_futex, (int *)addr, FUTEX_WAKE, n, NULL, NULL, 0);
}

/* Stack functions */

void *create_stack(size_t usable_size, size_t *out_total_size) {
    long page = sysconf(_SC_PAGESIZE);
    if (page <= 0) page = 4096;
    size_t page_sz = (size_t)page;

    size_t total = usable_size + page_sz; // guard + usable 
    void *map = mmap(NULL, total, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (map == MAP_FAILED) return NULL;

    // guard page at the low address 
    if (mprotect(map, page_sz, PROT_NONE) != 0) {
        munmap(map, total);
        return NULL;
    }

    if (out_total_size) *out_total_size = total;
    return map; // NOTE: returned pointer points to region base (guard page at base)
}

void destroy_stack(void *stack, size_t size) {
    if (!stack || size == 0) return;
    munmap(stack, size);
}

/* Thread function wrapper */

static int thread_startup(void *arg){
    mythread_t *t = (mythread_t *)arg;
    void *retval = NULL;

    if (t->start_routine) retval = t->start_routine(t->arg);

    t->retval = retval;

    atomic_store_explicit(&t->finished, 1, memory_order_release);
    futex_wake(&t->finished, 1);

    //If the thread was already detached before setting FINISHED, we enqueue it.
    //Use the MT_QUEUED bit to avoid double enqueuing (exit+detach).ы
    unsigned int s = atomic_load_explicit(&t->state, memory_order_acquire);
    if (s & MT_DETACHED) {
        unsigned int prev = atomic_fetch_or_explicit(&t->state, MT_QUEUED, memory_order_acq_rel);
        if (!(prev & MT_QUEUED)) {
            (void) start_reaper();
            reap_enqueue(t);
        }
    }

    syscall(SYS_exit, 0);
    return 0; // unreachable 
}

/* Kernel thread -- management structure in the kernel that schedules such a thread. */

int mythread_create(mythread_t** thread, void *(start_routine)(void*), void *arg) {
    if (!thread || !start_routine) {
        errno = EINVAL; // ERROR INVALID
        return -1;
    }
    mythread_t* t = calloc(1, sizeof(*t));
    if (!t) {
        errno = ENOMEM;  // ERROR NO MEMORY
        return -1;
    }

    const size_t USABLE_STACK = 256 * 1024;
    size_t total_size = 0;
    void *stack = create_stack(USABLE_STACK, &total_size);
    if (!stack) {
        free(t);
        errno = ENOMEM;  // ERROR NO MEMORY
        return -1;
    }
    
    t->arg = arg;
    t->start_routine = start_routine;
    t->retval = NULL;
    atomic_store(&t->finished, 0);
    atomic_store(&t->state, 0);
    t->stack = stack;
    t->stack_size = total_size;

    
    // FLAGS for clone(...)
    int flags =
        CLONE_VM | CLONE_FS | CLONE_FILES | 
        CLONE_SIGHAND | CLONE_THREAD;

    // TASK FOR THREAD CREATION (in process' VMA)
    // stack grows down on x86-64; clone expects pointer to top-of-stack
    void *stack_top = (char *)stack + total_size;
    stack_top = (void *)((uintptr_t)stack_top & ~(uintptr_t)0xF);

    int c_tid = clone(thread_startup, stack_top, flags, t);
    if (c_tid == -1) {
        int saved_errno = errno;
        destroy_stack(t->stack, t->stack_size);
        free(t);
        errno = saved_errno;
        return -1;
    }

    *thread = t;
    return 0;
}

int mythread_join(mythread_t* thread, void **retval) {
    if (!thread) {
        errno = EINVAL;
        return -1;
    }

    unsigned int s = atomic_load_explicit(&thread->state, memory_order_acquire);

    for (;;) {
        if (s & MT_DETACHED) {
            errno = EINVAL;
            return -1;
        }

        if (s & MT_JOINED) {
            errno = EINVAL;
            return -1;
        }

        unsigned int want = s | MT_JOINED;
        if (atomic_compare_exchange_strong_explicit(&thread->state, &s, want,
                                                    memory_order_acq_rel,
                                                    memory_order_acquire)) {
            break;
        }
    }


    while (!(atomic_load_explicit(&thread->finished, memory_order_acquire))) {
        futex_wait(&thread->finished, 0);
    }

    if (retval) *retval = thread->retval;

    unsigned int prev = atomic_fetch_or_explicit(&thread->state, MT_REAPED, memory_order_acq_rel);
    if (!(prev & MT_REAPED)) {
        destroy_stack(thread->stack, thread->stack_size);
        free(thread);
    }

    return 0;
}


int mythread_detach(mythread_t* thread) {
    if (!thread) { 
        errno = EINVAL;
        return -1;
    }
    
    unsigned int s = atomic_load_explicit(&thread->state, memory_order_acquire);

    for (;;) {
        if (s & MT_DETACHED) {
            return 0;
        }

        if (s & MT_JOINED) {
            errno = EINVAL; 
            return -1;
        }

        unsigned int want = s | MT_DETACHED;

        if (atomic_compare_exchange_weak_explicit(&thread->state, &s, want,
                                                  memory_order_acq_rel,
                                                  memory_order_acquire)) {
            break;
        }
    }

    s = atomic_load_explicit(&thread->finished, memory_order_acquire);
    if (s) {
        unsigned int prev = atomic_fetch_or_explicit(&thread->state, MT_QUEUED, memory_order_acq_rel);
        if (!(prev & MT_QUEUED)) {
            start_reaper();
            reap_enqueue(thread);
        }
    }
    return 0;
}