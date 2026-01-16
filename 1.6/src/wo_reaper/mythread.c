#define _GNU_SOURCE
#include <sys/types.h>
#include <errno.h>
#include <malloc.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <unistd.h>
#include <ucontext.h>
#include <stdlib.h>
#include <stdint.h>

#include "string.h"
#include "mythread.h"

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

/* Struct for clearing thread stack */

struct cleanup_args {
    void *old_stack;
    size_t old_stack_size;
    mythread_t *t;
};

static void cleanup_trampoline(uintptr_t arg) {
    struct cleanup_args *a = (struct cleanup_args *)arg;

    if (a->old_stack) {
        munmap(a->old_stack, a->old_stack_size);
    }

    free(a->t);
    free(a);

    syscall(SYS_exit, 0);
}

/* Cleanup for detached thread */

#define TEMP_STACK_SIZE (128 * 1024)

void cleanup_and_exit_from_other_stack(mythread_t *t) {
    struct cleanup_args *args = malloc(sizeof(*args));
    args->old_stack = t->stack;
    args->old_stack_size = t->stack_size;
    args->t = t;

    void *temp_stack = mmap(NULL, TEMP_STACK_SIZE,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS,
                            -1, 0);
    if (temp_stack == MAP_FAILED) {
        _exit(1);
    }

    ucontext_t uc;
    getcontext(&uc);

    uc.uc_stack.ss_sp = temp_stack;
    uc.uc_stack.ss_size = TEMP_STACK_SIZE;
    uc.uc_link = NULL;

    makecontext(&uc, (void (*)())cleanup_trampoline, 1, (uintptr_t)args);

    swapcontext(&(ucontext_t){0}, &uc);
}


/* Thread function wrapper */

static int thread_startup(void *arg){
    mythread_t *t= (mythread_t *)arg;
    void *retval = NULL;

    if (t->start_routine) retval = t->start_routine(t->arg);

    t->retval = retval;
    atomic_store_explicit(&t->finished, 1, memory_order_release);

    futex_wake(&t->finished, 1);

    int expected = 0;
    if (atomic_compare_exchange_strong_explicit(
            &t->joined, &expected, 1,
            memory_order_acq_rel, memory_order_acquire))
    {
        if (atomic_load_explicit(&t->detached, memory_order_acquire)) {
            cleanup_and_exit_from_other_stack(t);
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

int mythread_detach(mythread_t* thread) {
    if (!thread) { 
        errno = EINVAL;
        return -1;
    }

    atomic_store_explicit(&thread->detached, 1, memory_order_release);

    /* If the stream has already ended, we release it ourselves */
    if (atomic_load_explicit(&thread->finished, memory_order_acquire)) {
        int expected = 0;
        if (atomic_compare_exchange_strong_explicit(
                &thread->joined, &expected, 1,
                memory_order_acq_rel, memory_order_acquire))
        {
            destroy_stack(thread->stack, thread->stack_size);
            free(thread);
        }
    }

    return 0;

}