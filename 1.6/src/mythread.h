#include <stdatomic.h>
#include <sys/types.h>

#define PAGE_SIZE 4096
#define STACK_SIZE PAGE_SIZE

typedef void *(*start_routine_t)(void *);

typedef struct _mythread{
    void            *arg;
    start_routine_t start_routine;
    void            *retval;
    
    atomic_int      finished;
    atomic_int      joined;
    atomic_int      detached;

    void            *stack;            
    size_t          stack_size;
} mythread_t;



int mythread_create(mythread_t** thread, void *(start_routine)(void *), void *arg);
int mythread_join(mythread_t* thread, void **retval);
int mythread_detach(mythread_t* thread);