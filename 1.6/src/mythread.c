#define _GNU_SOURCE
#include "string.h"
#include "mythread.h"
#include <sys/types.h>
#include <errno.h>
#include <malloc.h>
#include <sched.h>
#include <sys/mman.h>

// Thread function wrappes
static int thread_startup(void *arg){

}

void *create_stack(off_t size) {
    void *stack;
    /* MAP_STACK helps the kernel, MAP_PRIVATE|MAP_ANONYMOUS for anonymous mapping */
    stack = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (stack == MAP_FAILED) return NULL;
    return stack;
}

void *destroy_stack(void *stack, size_t size) {
    if (!stack || size == 0) return;
    munmap(stack, size);
}

// Ядерный поток - управление структуры в ядре, ядро планирует такой поток.
int mythread_create(mythread_t* thread, void *(start_routine)(void*), void *arg) {
    if (!thread || !start_routine) {
        //Выходим
        return EINVAL; // ERROR INVALID
    }

    // Выделяем память в куче на структуру, представляющую поток
    mythread_struct_t* t =(mythread_struct_t*)calloc(1, sizeof(*t));
    if (!t) 
        return ENOMEM;  // ERROR NO MEMORY
    
    // STACK CREATION
    void *stack = create_stack(STACK_SIZE);
    if (!stack) {
        free(t);
        return ENOMEM;  // ERROR NO MEMORY
    }
    
    // Заполняем структуру
    t->stack = stack;
    t->stack_size = STACK_SIZE;
    t->start_routine = start_routine;
    t->arg = arg;
    t->retval = NULL;
    t->finished = 0;
    t->joined = 0;
    t->detached = 0;
    t->mythread_id = 0;
    
    // FLAGS for clone(...)
    int flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND |
                CLONE_THREAD | CLONE_SYSVSEM |
                CLONE_CHILD_CLEARTID | CLONE_CHILD_SETTID | CLONE_PARENT_SETTID;

    // TASK FOR THREAD CREATION (in process' VMA)

    /* stack grows down on x86-64; clone expects pointer to top-of-stack */
    void *stack_top = (char *)stack + STACK_SIZE;

    int tid = clone(thread_startup, stack_top, flags, (void*)t, &t->mythread_id, NULL);
    if (tid == -1) {
        int saved_errno = errno;
        destroy_stack(stack, STACK_SIZE);
        free(t);
        return saved_errno;
    }

    *thread = t;
    return 0;
}