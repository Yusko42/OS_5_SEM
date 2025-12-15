#ifndef LIST_H
#define LIST_H

#include <pthread.h>

typedef struct _Node {
    char            value[100];
    struct _Node*   next;
    pthread_mutex_t sync;
} Node;

typedef struct _Storage {
    Node*           first;
    size_t          size;
} Storage;

Storage* stroage_init();
void storage_destroy();

#endif