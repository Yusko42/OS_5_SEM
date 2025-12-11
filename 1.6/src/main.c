#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include "mythread.h"

// Тесты, тесты...

void *mythread(void *arg) {
	printf("mythread [%d %d %d]: Hello from mythread!\n", getpid(), getppid(), gettid());
	return NULL;
}

int main() {
	mythread_t tid[5];
	int err;

	printf("main [%d %d %d]: Hello from main!\n", getpid(), getppid(), gettid());

    for (int i = 0; i < 5; ++i) {
	    err = mythread_create(&tid[i], NULL, mythread);
        /*if (err) {
	        printf("main: pthread_create() failed: %s\n", strerror(err));
		    return -1;
	    }*/
    }
	// JOIN added for sync
    for (int i = 0; i < 5; i++) {
	    mythread_join(tid[i], NULL);
		printf("thread %d completed\n", i);
    }

	return 0;
}