#define _GNU_SOURCE
#include "string.h"
#include "mythread.h"
#include <sys/types.h>
#include <errno.h>
#include <malloc.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <unistd.h>

/* Fitex syscalls wrappers */

static int futex_wait(atomic_int *addr, int expected) {
    return syscall(SYS_futex, (int *)addr, FUTEX_WAIT, expected, NULL, NULL, 0);
}

static int futex_wake(atomic_int *addr, int n) {
    return syscall(SYS_futex, (int *)addr, FUTEX_WAKE, n, NULL, NULL, 0);
}

/* Stack functions */

void *create_stack(off_t size) {
    void *stack;
    stack = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stack == MAP_FAILED) return NULL;
    return stack;
}

void destroy_stack(void *stack, size_t size) {
    if (!stack || size == 0) return;
    munmap(stack, size);
}

/* Thread function wrapper */

static int thread_startup(void *arg){
    mythread_t *t= (mythread_t *)arg;
    void *retval = NULL;

    if (t->start_routine) retval = t->start_routine(t->arg);

    t->retval = retval;
    atomic_store_explicit(&t->finished, 1, memory_order_release);

    futex_wake(&t->finished, 1);

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

    void *stack = create_stack(STACK_SIZE);
    if (!stack) {
        free(t);
        errno = ENOMEM;  // ERROR NO MEMORY
        return -1;
    }
    
    t->arg = arg;
    t->start_routine = start_routine;
    t->retval = NULL;
    atomic_store(&t->finished, 0);
    atomic_store(&t->joined, 0);
    atomic_store(&t->detached, 0);
    t->stack = stack;
    t->stack_size = STACK_SIZE;
    
    // FLAGS for clone(...)
    int flags =
        CLONE_VM | CLONE_FS | CLONE_FILES | 
        CLONE_SIGHAND | CLONE_THREAD;

    // TASK FOR THREAD CREATION (in process' VMA)
    // stack grows down on x86-64; clone expects pointer to top-of-stack
    void *stack_top = (char *)stack + STACK_SIZE;

    int c_tid = clone(thread_startup, stack_top, flags, t);
    if (c_tid == -1) {
        int saved_errno = errno;
        destroy_stack(t->stack, STACK_SIZE);
        free(t);
        errno = saved_errno;
        return -1;
    }

    *thread = t;
    return 0;
}

int mythread_join(mythread_t* thread, void **retval) {
    if (!thread || atomic_load_explicit(&thread->detached, memory_order_acquire)) { 
        errno = EINVAL;
        return -1;
    }
    for (;;) {
        if (atomic_load_explicit(&thread->finished, memory_order_acquire))
            break;
        futex_wait(&thread->finished, 0);
    }
    if (retval) *retval = thread->retval;

    int prev = atomic_exchange(&thread->joined, 1);
    if (prev == 0) {
        destroy_stack(thread->stack, thread->stack_size);
        free(thread);
    }

    return 0;
}

static int reaper_start(void *arg) {
    mythread_t *t = arg;
    for (;;) {
        if (atomic_load_explicit(&t->finished, memory_order_acquire)) break;
        futex_wait(&t->finished, 0);
    }
    int prev = atomic_exchange_explicit(&t->joined, 1, memory_order_acq_rel);
    if (prev == 0) {
        destroy_stack(t->stack, t->stack_size);
        free(t);
    }
    syscall(SYS_exit, 0);
    return 0;
}

int mythread_detach(mythread_t* thread) {
    if (!thread) { 
        errno = EINVAL;
        return -1;
    }

    atomic_store(&thread->detached, 1);

    /* If the stream has already ended, we release it ourselves */
    if (atomic_load_explicit(&thread->finished, memory_order_acquire) 
        && !atomic_exchange_explicit(&thread->joined, 1, memory_order_acq_rel)) {
        destroy_stack(thread->stack, STACK_SIZE);
        free(thread);
    }

    /* Otherwise, we create a reaper thread that will wait for finished and free up resources. */
    void *rstack = create_stack(STACK_SIZE);
    if (!rstack) { 
        errno = ENOMEM; return -1; 
    }
    void *rstack_top = (char*)rstack + STACK_SIZE;
    int flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD;
    int res = clone(reaper_start, rstack_top, flags, thread);
    if (res == -1) {
        destroy_stack(rstack, STACK_SIZE);
        errno = errno;
        return -1;
    }
    return 0;

}
