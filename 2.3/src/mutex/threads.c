#include <stdatomic.h>
#include "list.h"

// Atomic counters
static atomic_long iterations_asc = 0;
static atomic_long iterations_desc = 0;
static atomic_long iterations_equal = 0;

static atomic_long pairs_asc = 0;
static atomic_long pairs_desc = 0;
static atomic_long pairs_equal = 0;

static atomic_long swap_asc = 0;
static atomic_long swap_desc = 0;
static atomic_long swap_not_equal = 0;

/* Main function for reading threads */

static void search_pairs(Storage* s, int (*cmp)(int, int), atomic_long* global_counter, atomic_long* pairs_counter) {
    // One iteration of list traversal per cycle
    while(1) {
        Node* first = s->first;
        Node* a;
        Node* b;

        pthread_mutex_lock(&first->sync);
        a = first->next;

        // One node in the storage
        if (!a) {
            pthread_mutex_unlock(&first->sync);
            atomic_fetch_add(pairs_counter, 1);
            continue;
        }

        // If we do have 'a', the go to the next node 
        pthread_mutex_lock(&a->sync);
        pthread_mutex_unlock(&first->sync);

        while(1) {
            b = a->next;
            if (!b)
                break;
            pthread_mutex_lock(&b->sync);

            int len_a = (int)strlen(a->value);
            int len_b = (int)strlen(b->value);
            if (cmp(len_a, len_b))
                atomic_fetch_add(pairs_counter, 1);
            
            pthread_mutex_unlock(&a->sync);
            a = b; // next step
        }
        pthread_mutex_unlock(&b->sync);
        atomic_fetch_add(pairs_counter, 1);
    }
}

static int cmr_asc(int len_1, int len_2) { return len_1 < len_2; }
static int cmr_desc(int len_1, int len_2) { return len_1 > len_2; }
static int cmr_equal(int len_1, int len_2) { return len_1 == len_2; }
static int cmp_not_equal(int len_1, int len_2) { // for swapper thread
    if (len_1 != len_2) return rand() % 2 == 0; 
}

/* Reading treads (find pairs) */

void* pair_searcher_asc(void* arg){
    Storage* s = (Storage*)arg;
    search_pairs(s, cmp_asc(), &iterations_asc, &pairs_asc);
    return NULL;
}

void* pair_searcher_desc(void* arg){
    Storage* s = (Storage*)arg;
    search_pairs(s, cmp_desc(), &iterations_desc, &pairs_desc);
    return NULL;
}

void* pair_searcher_equal(void* arg){
    Storage* s = (Storage*)arg;
    search_pairs(s, cmp_equal(), &iterations_equal, &pairs_equal);
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
static void swap_pairs(Storage* s, int (*cmp)(int, int), atomic_long* swap_counter) {
    if (s->size < 2)    return NULL;

    // случайный индекс
    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)pthread_self();
    int max_index = s->size - 1;
    int left_index = rand_s(&seed) % max_index;

    Node* prev = s->first;
    pthread_mutex_lock(&prev->sync);
    if (!prev)          return NULL;
    
    Node* cur = prev->next;
    if (!cur)              return NULL;

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
        return NULL;
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
    Storage* s = (Storage*)arg;
    while(1) {
        swap_pairs(s, cmp_asc(), swap_asc);
    }
    return NULL;
}

void* swapper_desc(void* arg){
    Storage* s = (Storage*)arg;
    while(1) {
        swap_pairs(s, cmp_desc(), swap_desc);
    }
    return NULL;
}

void* swapper_not_eql(void* arg){
    Storage* s = (Storage*)arg;
    while(1) {
        swap_pairs(s, cmp_not_equal(), swap_not_equal);
    }
    return NULL;
}