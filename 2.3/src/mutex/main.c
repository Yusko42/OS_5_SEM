#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <unistd.h>
#include <string.h>

#include "list.h"


static atomic_long iterations_asc = 0;
static atomic_long iterations_desc = 0;
static atomic_long iterations_equal = 0;

static atomic_long pairs_asc = 0;
static atomic_long pairs_desc = 0;
static atomic_long pairs_equal = 0;

static atomic_long swap_asc = 0;
static atomic_long swap_desc = 0;
static atomic_long swap_not_equal = 0;


atomic_int is_running = 1;

/* Main function for reading threads */

static void search_pairs(Storage* s, int (*cmp)(int, int), atomic_long* global_counter, atomic_long* pairs_counter) {
    // One iteration of list traversal per cycle
    while(atomic_load(&is_running)) {
        Node* first = s->first;
        if (!first) return; // или continue в бесконечном цикле
        Node* a;
        Node* b;

        pthread_mutex_lock(&first->sync);
        a = first->next;

        // One node in the storage
        if (!a) {
            pthread_mutex_unlock(&first->sync);
            atomic_fetch_add(global_counter, 1);
            continue;
        }

        // If we do have 'a', the go to the next node 
        pthread_mutex_lock(&a->sync);
        pthread_mutex_unlock(&first->sync);

        while(a) {
            b = a->next;
            if (!b) {
                pthread_mutex_unlock(&a->sync);
                break;
            }
            pthread_mutex_lock(&b->sync);

            int len_a = (int)strlen(a->value);
            int len_b = (int)strlen(b->value);
            if (cmp(len_a, len_b))
                atomic_fetch_add(pairs_counter, 1);
            
            pthread_mutex_unlock(&a->sync);
            a = b; // next step
        }
        //pthread_mutex_unlock(&a->sync);
        atomic_fetch_add(global_counter, 1);
    }
}

static int cmp_asc(int len_1, int len_2) { return len_1 > len_2; }
static int cmp_desc(int len_1, int len_2) { return len_1 < len_2; }
static int cmp_equal(int len_1, int len_2) { return len_1 == len_2; }
static int cmp_not_equal(int len_1, int len_2) { // for swapper thread
    //unsigned int seed = (unsigned int)time(NULL);
    return len_1 != len_2; // return (rand_r(&seed) % 2 == 0); 
                           //return 0;
}

/* Reading treads (find pairs) */

void* pair_searcher_asc(void* arg){
    Storage* s = (Storage*)arg;
    if (s->size < 2)
        return NULL;
    search_pairs(s, cmp_asc, &iterations_asc, &pairs_asc);
    return NULL;
}

void* pair_searcher_desc(void* arg){
    Storage* s = (Storage*)arg;
    if (s->size < 2)
        return NULL;
    search_pairs(s, cmp_desc, &iterations_desc, &pairs_desc);
    return NULL;
}

void* pair_searcher_equal(void* arg){
    Storage* s = (Storage*)arg;
    if (s->size < 2)
        return NULL;
    search_pairs(s, cmp_equal, &iterations_equal, &pairs_equal);
    return NULL;
}

// PREV - L - R - NEXT => PREV - R - L - NEXT
void swap_nodes(Node* prev, Node* node_left, Node* node_right) {
    prev->next = node_right;
    node_left->next = node_right->next;
    node_right->next = node_left;
}

/* Main function for swapping threads */
// Высчитываем индекс, шагаем туда, меняем? 
static void swap_pairs(Storage* s, int (*cmp)(int, int), atomic_long* swap_counter, unsigned int* seed) {
    // случайный индекс
    int max_index = s->size - 1;
    int left_index = rand_r(seed) % max_index;

    Node* prev = s->first;
    if (!prev)          return;

    pthread_mutex_lock(&prev->sync);
    Node* cur = prev->next;
    if (!cur) {
        pthread_mutex_unlock(&prev->sync);
        return;
    }

    pthread_mutex_lock(&cur->sync);
    // движемся по списку к левой ноде (cur = lft, next = right)
    for (int i = 0; i < left_index; i++){
        Node* next = cur->next;
        if (!next)          break;
        pthread_mutex_lock(&next->sync);
        pthread_mutex_unlock(&prev->sync);

        prev = cur;
        cur = next;
    }

    Node* next = cur->next;
    if (!next) {
        pthread_mutex_unlock(&cur->sync);
        pthread_mutex_unlock(&prev->sync);
        return;
    }

    // Дошли: применяем алгоритм
    pthread_mutex_lock(&next->sync);
    int len_left = strlen(cur->value);
    int len_right = strlen(next->value);

    if (cmp(len_left, len_right)){
        swap_nodes(prev, cur, next);
        atomic_fetch_add(swap_counter, 1);
    }
    pthread_mutex_unlock(&next->sync);
    pthread_mutex_unlock(&cur->sync);
    pthread_mutex_unlock(&prev->sync);
}

/* Swap threads */

void* swapper_asc(void* arg){
    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)pthread_self();
    Storage* s = (Storage*)arg;
    if (s->size < 2)    return NULL;
    while(atomic_load(&is_running)) {
        swap_pairs(s, cmp_asc, &swap_asc, &seed);
    }
    return NULL;
}

void* swapper_desc(void* arg){
    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)pthread_self();
    Storage* s = (Storage*)arg;
    if (s->size < 2)    return NULL;
    while(atomic_load(&is_running)) {
        swap_pairs(s, cmp_desc, &swap_desc, &seed);
    }
    return NULL;
}

void* swapper_not_eql(void* arg){
    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)pthread_self();
    Storage* s = (Storage*)arg;
    if (s->size < 2)    return NULL;
    while(atomic_load(&is_running)) {
        swap_pairs(s, cmp_not_equal, &swap_not_equal, &seed);
    }
    return NULL;
}

void stop() {
    atomic_store(&is_running, 0);
}


int main(int argc, char **argv) {
    if (argc < 2) {
        printf("no arg - list size\n");
        return -1;
    }
    srand(time(NULL));

    int storage_size = atoi(argv[1]);
    Storage* s = storage_init(storage_size);

    pthread_t searcher_asc, searcher_desc, searcher_eql;
    pthread_t pswapper_asc, pswapper_desc, pswapper_not_eql;

    pthread_create(&searcher_asc, NULL, pair_searcher_asc, s);
    pthread_create(&searcher_desc, NULL, pair_searcher_desc, s);
    pthread_create(&searcher_eql, NULL, pair_searcher_equal, s);

    pthread_create(&pswapper_asc, NULL, swapper_asc, s);
    pthread_create(&pswapper_desc, NULL, swapper_desc, s);
    pthread_create(&pswapper_not_eql, NULL, swapper_not_eql, s);

    sleep(10);
    stop(); //is_running == 0

    pthread_join(searcher_asc, NULL);
    pthread_join(searcher_desc, NULL);
    pthread_join(searcher_eql, NULL);

    pthread_join(pswapper_asc, NULL);
    pthread_join(pswapper_desc, NULL);
    pthread_join(pswapper_not_eql, NULL);

    printf("Mutex size: %d, 10 s\n", storage_size);
    printf("iterations_asc:  %lu, pairs_asc:  %lu, swap_asc: %lu\n", iterations_asc, pairs_asc, swap_asc);
    printf("iterations_desc: %lu, pairs_desc: %lu, swap_desc: %lu\n", iterations_desc, pairs_desc, swap_desc);
    printf("iterations_eql:  %lu, pairs_eql:  %lu, swap_not_equal: %lu\n", iterations_equal, pairs_equal, swap_not_equal);

    return 0;
}