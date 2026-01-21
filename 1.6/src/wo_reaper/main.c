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

int test_multiple_detach() {
    mythread_t *t;
    int err = mythread_create(&t, mythread_2, NULL);
    if (err) {
        printf("mythread_create() failed: %s\n", strerror(errno));
        return -1;
    }

    err = mythread_detach(t);
    if (err) {
        printf("mythread_detach() failed (1): %s\n", strerror(errno));
        return -1;
    }

    // Повторный detach не должен падать
    err = mythread_detach(t);
    if (err) {
        printf("mythread_detach() failed (2): %s\n", strerror(errno));
        return -1;
    }

    printf("test_multiple_detach OK\n");
    return 0;
}

int test_detach_long_running() {
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

    // Дадим reaper'у время поработать
    sleep(1);

    printf("test_detach_long_running OK\n");
    return 0;
}

void *fast_exit(void *arg) {
    return NULL;
}

int test_race_detach_vs_exit() {
    mythread_t *t;
    int err = mythread_create(&t, fast_exit, NULL);
    if (err) {
        printf("mythread_create() failed: %s\n", strerror(errno));
        return -1;
    }

    // Пытаемся поймать момент между запуском и exit
    for (int i = 0; i < 1000; i++) {
        err = mythread_detach(t);
        if (err && errno != EINVAL) {
            printf("unexpected error in detach: %s\n", strerror(errno));
            return -1;
        }
    }

    sleep(1);
    printf("test_race_detach_vs_exit OK\n");
    return 0;
}

int test_detach_then_join_fails() {
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

    void *res;
    err = mythread_join(t, &res);
    if (err == 0) {
        printf("ERROR: join succeeded on detached thread\n");
        return -1;
    }

    if (errno != EINVAL) {
        printf("ERROR: wrong errno on join after detach: %s\n", strerror(errno));
        return -1;
    }

    printf("test_detach_then_join_fails OK\n");
    return 0;
}

int test_many_detached_threads() {
    const int N = 50;
    mythread_t *threads[N];

    for (int i = 0; i < N; i++) {
        int err = mythread_create(&threads[i], mythread_2, NULL);
        if (err) {
            printf("mythread_create failed at %d: %s\n", i, strerror(errno));
            return -1;
        }

        err = mythread_detach(threads[i]);
        if (err) {
            printf("mythread_detach failed at %d: %s\n", i, strerror(errno));
            return -1;
        }
    }

    // Ждём, пока все должны быть собраны
    sleep(2);

    printf("test_many_detached_threads OK\n");
    return 0;
}

int main() {
	if (test_join()) {printf("main: test_join() failed.\n"); return -1;};
	sleep(3);
	if (test_detach_before_exit()) {printf("main: test_detach_before_exit() failed.\n"); return -1;};
	sleep(3);
	if (test_detach_after_exit()) {printf("main: test_detach_after_exit() failed.\n"); return -1;}
	sleep(3);

    if (test_multiple_detach()) return -1;
    if (test_detach_long_running()) return -1;
    if (test_race_detach_vs_exit()) return -1;
    if (test_detach_then_join_fails()) return -1;
    if (test_many_detached_threads()) return -1;

    printf("ALL TESTS PASSED\n");
	return 0;
}