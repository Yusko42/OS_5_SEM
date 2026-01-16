#define _GNU_SOURCE
#include <sched.h>
#include <stdatomic.h>
#include <malloc.h>
#include <sys/mman.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>

#include "mythread.h"

/* Stack config */

#define PAGE_SIZE 4096
#define REAPER_STACK_SIZE (1 << 20)

typedef struct reap_node {
    mythread_t *thread;
    struct reap_node *next;
} reap_node_t;

typedef struct {
    _Atomic(reap_node_t*) head;
    _Atomic(reap_node_t*) tail;

    atomic_int count;
    atomic_int lock;
} reap_queue_t;


static atomic_int reaper_started = 0;
static void *reaper_stack = NULL;
static size_t reaper_stack_size = 0;
static pid_t  reaper_tid = -1; 

static reap_queue_t reap_queue = {
    .head = NULL, 
    .tail = NULL, 
    .count = 0,
    .lock = 0
};

/* Spinlock operations */

static void spin_lock(atomic_int *lock) {
    int expected = 0;
    while (!atomic_compare_exchange_weak_explicit(lock, &expected, 1, 
        memory_order_acquire, memory_order_relaxed)) {
            expected = 0;
            #ifdef __x86_64__
            asm volatile("pause" ::: "memory");
            #endif
    }
}

static void spin_unlock(atomic_int *lock) {
    atomic_store_explicit(lock, 0, memory_order_release);
}

/* Queue operations */

static void reap_enqueue(mythread_t *t) {
    reap_node_t *node = (reap_node_t*)malloc(sizeof(*node));
    if (!node) { 
        printf("ERROR: cannot alloc memory for node!\n");
        return;
    }
    
    node->thread = t;
    node->next = NULL;
    
    spin_lock(&reap_queue.lock);
    
    reap_node_t *tail = atomic_load_explicit(&reap_queue.tail, memory_order_relaxed);
    if (tail == NULL) {
        atomic_store_explicit(&reap_queue.head, node, memory_order_release);
        atomic_store_explicit(&reap_queue.tail, node, memory_order_release);
    } else {
        reap_node_t *old_tail = atomic_load_explicit(&reap_queue.tail, memory_order_relaxed);
        old_tail->next = node;
        atomic_store_explicit(&reap_queue.tail, node, memory_order_relaxed);
    }
    
    spin_unlock(&reap_queue.lock);
    
    int old_count = atomic_fetch_add_explicit(&reap_queue.count, 1, memory_order_acq_rel);
    if (old_count == 0) {
        futex_wake(&reap_queue.count, 1);
    }
}

static reap_node_t *reap_dequeue(void) {
    spin_lock(&reap_queue.lock);
    
    reap_node_t *head = atomic_load_explicit(&reap_queue.head, memory_order_acquire);
    if (head != NULL) {
        atomic_store_explicit(&reap_queue.head, NULL, memory_order_relaxed);
        atomic_store_explicit(&reap_queue.tail, NULL, memory_order_relaxed);
    }
    
    spin_unlock(&reap_queue.lock);
    return head;
}

/* Reaper thread function */

static int reaper_main(void *arg) {
    (void)arg;
    
    for (;;) {
        // Wait until something is in queue
        int count = atomic_load_explicit(&reap_queue.count, memory_order_acquire);
        while (count == 0) {
            futex_wait(&reap_queue.count, 0);
            count = atomic_load_explicit(&reap_queue.count, memory_order_acquire);
        }
        
        // Get the queue
        reap_node_t *list = reap_dequeue();
        if (!list)  continue;
        
        int processed = 0;
        reap_node_t *current = list;
        
        while (current) {
            reap_node_t *next = current->next;
            mythread_t *t = current->thread;
            
            unsigned int prev = 
                atomic_fetch_or_explicit(&t->state, MT_REAPED, memory_order_acq_rel);
            if (!(prev & MT_REAPED)) {
                if (t->stack)   munmap(t->stack, t->stack_size);
                free(t);
            }
            free(current);
            processed++;
            current = next;
        }
        
        // Decrement the counter by the number of processed
        int old = atomic_fetch_sub_explicit(&reap_queue.count, processed, memory_order_acq_rel);
        
        // If there are any elements remaining after processing, continue w/o waiting
        if (old < processed) {
            printf("ERROR: old < processed\n");
        }
        if (old - processed > 0)
            continue;
    }
    return 0;
}

/* Stack config */

static void __reaper_cleanup_on_error(void *stack, size_t total_size) {
    if (stack && total_size) munmap(stack, total_size);
    atomic_store_explicit(&reaper_started, 0, memory_order_release);
}

int start_reaper() {
    int expected = 0;
    if (!atomic_compare_exchange_strong_explicit(&reaper_started, &expected, 1,
                                    memory_order_acq_rel, memory_order_acquire)) 
    {
        return 0;
    }

    long page = sysconf(_SC_PAGESIZE);
    size_t page_sz = (page > 0) ? (size_t)page : PAGE_SIZE;
    size_t stack_size = (size_t)REAPER_STACK_SIZE;
    size_t total = stack_size + page_sz; // + guard page

    void* map = mmap(NULL, total, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (map == MAP_FAILED) {
        int saved = errno;
        atomic_store_explicit(&reaper_started, 0, memory_order_release);
        errno = saved;
        return -1;
    }

    // guard page
    if (mprotect(map, (size_t)PAGE_SIZE, PROT_NONE) != 0) {
        int saved = errno;
        __reaper_cleanup_on_error(map, total);
        errno = saved;
        return -1;
    }

    void *stack_top = (char *)map + total;

    // выравнивание
    stack_top = (void *)((uintptr_t)stack_top & ~(uintptr_t)0xF);

    // flags
    int flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_SYSVSEM | CLONE_THREAD;

    pid_t pid = clone((int(*)(void *))reaper_main, stack_top, flags, NULL);

    if (pid == -1) {
        int saved = errno;
        atomic_store_explicit(&reaper_started, 0, memory_order_release);
        __reaper_cleanup_on_error(map, total);
        errno = saved;
        return -1;
    }

    reaper_stack = map;
    reaper_stack_size = total;
    reaper_tid = pid;
    return 0;
}