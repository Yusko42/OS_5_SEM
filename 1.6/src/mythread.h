#include <sys/types.h>

#define PAGE_SIZE 4096
#define STACK_SIZE PAGE_SIZE

typedef struct mythread_struct_t *mythread_t;
typedef void *(*start_routine_t)(void *);

typedef struct _mythread{
    int             mythread_id;

    void            *arg;
    start_routine_t start_routine;
    void            *retval;
    
    volatile int    finished;
    volatile int    joined;
    volatile int    detached;

    void*           stack;            
    size_t          stack_size;
} mythread_struct_t;

void *create_stack(off_t size);
int mythread_create(mythread_t* thread, void *(start_routine)(void *), void *arg);
int mythread_join();
int mythread_detach();