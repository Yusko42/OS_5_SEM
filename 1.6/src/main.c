#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <assert.h>
#include <unistd.h>
#include <sched.h>

#include "mythread.h"

// Тесты, тесты...

void *mythread_1(void *arg) {
    long v = (long)arg;
    return (void*)(v * 2);
}

void *mythread_2(void *arg) {
    for (volatile int i = 0; i < 100000000; i++);
    return NULL;
}

void *mythread_3(void *arg) {
    return NULL;
}

int test_detach_after_exit() {
    mythread_t *t;
	int err = mythread_create(&t, mythread_3, NULL);
    if (err) {
		printf("mythread_create() failed: %s\n", strerror(errno));
		return -1;
	};

    sched_yield();

    err = mythread_detach(t);
	if (err) {
		printf("mythread_detach() failed: %s\n", strerror(errno));
		return -1;
	};

    printf("test_detach_after_exit OK\n");
	return 0;
}

int test_detach_before_exit() {
    mythread_t *t;
    int err = mythread_create(&t, mythread_2, NULL);
    if (err) {
        printf("mythread_create() failed: %s\n", strerror(errno));
        return -1;
    }
    
    err = mythread_detach(t);
    if (err) {
        printf("mythread_detach() failed: %s\n", strerror(errno));
        return -1;
    }

    // Join должен завершиться с ошибкой для detached потока
    void *res;
    err = mythread_join(t, &res);
    if (err == 0) {
        printf("mythread_join() worked (NOT OK)\n");
        return -1;
    }

    printf("test_detach_before_exit OK\n");
    
    return 0;
}

int test_join() {
    const int N = 10;
    mythread_t *threads[N];
	int err;

    for (long i = 0; i < N; i++) {
        err = mythread_create(&threads[i], mythread_1, (void*)i);
		if (err) {
			{
				printf("mythread_create() failed: %s\n", strerror(errno));
				return -1;
			};
		}
    }

    for (int i = N - 1; i >= 0; i--) {
        void *res;
        err = mythread_join(threads[i], &res);
		if (err) {
			printf("mythread_join() failed: %s\n", strerror(errno));
			return -1;
		};
        assert(res == (void*)(i * 2));
    }

    printf("test_join OK\n");
	return 0;
}

int main() {
	if (test_join()) {printf("main: test_join() failed.\n"); return -1;};
	sleep(3);
	if (test_detach_before_exit()) {printf("main: test_detach_before_exit() failed.\n"); return -1;};
	sleep(5);
	if (test_detach_after_exit()) {printf("main: test_detach_after_exit() failed.\n"); return -1;}
	sleep(5);
	return 0;
}