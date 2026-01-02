/*#ifndef THREADS_H
#define THREADS_H
#include <stdatomic.h>

void* pair_searcher_asc(void* arg);
void* pair_searcher_desc(void* arg);
void* pair_searcher_equal(void* arg);

void* swapper_asc(void* arg);
void* swapper_desc(void* arg);
void* swapper_not_eql(void* arg);

void stop();

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


#endif*/