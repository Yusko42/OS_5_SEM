#include <stdatomic.h>
#include <sys/types.h>

#define PAGE_SIZE 4096
#define STACK_SIZE 16*PAGE_SIZE

typedef void *(*start_routine_t)(void *);

typedef struct _mythread{
    void            *arg;
    start_routine_t start_routine;
    void            *retval;
    
    atomic_int      finished;
    _Atomic unsigned int        state;
    
    void            *stack;            
    size_t          stack_size;
} mythread_t;

enum {
    MT_DETACHED = 1u << 1,
    MT_JOINED   = 1u << 2,
    MT_REAPED   = 1u << 3,
    MT_QUEUED   = 1u << 4
};

int mythread_create(mythread_t** thread, void *(start_routine)(void *), void *arg);
int mythread_join(mythread_t* thread, void **retval);
int mythread_detach(mythread_t* thread);

static int futex_wait(atomic_int *addr, int expected);
static int futex_wake(atomic_int *addr, int n);