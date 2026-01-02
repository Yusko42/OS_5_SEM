#ifndef LIST_H
#define LIST_H

#define _GNU_SOURCE
#include <pthread.h>

typedef struct _Node {
    char            value[100];
    struct _Node*   next;
    pthread_rwlock_t sync;
} Node;

typedef struct _Storage {
    Node*           first;
    size_t          size;
} Storage;

Storage* storage_init(int n);
void storage_destroy(Storage* s);

#endif