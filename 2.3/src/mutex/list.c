#include "list.h"
#include <stdio.h>
#include <stdlib.h>

char* generate_gandom_string(char* buf) {
    const char* charset = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    size_t charset_size = sizeof(charset) - 1;

    size_t max_size = sizeof(buf) - 1;
    int str_len = rand() % (int)max_size + 1; // from 1 to max_size (= buf_size - 1) symbols to buf
    for (int i = 0; i < str_len; i++) {
        buf[i] = charset[rand() % (int)charset_size];
    }
    buf[str_len] = '\0';
}

Storage* storage_init(int n) {
    if (n <= 0) return NULL;

    Storage* s = malloc(sizeof(Storage));
    if (!s) {
        perror("Malloc for storage error");
        return -1;
    }

    Node* first = malloc(sizeof(Node));
    if (!first) {
        perror("Malloc for first node error");
        return -1;
    }
    // First node init
    first->value[0] = '\0';
    first->next = NULL;
    pthread_mutex_init(&first->sync, NULL);

    // Storage list init
    s->first = first;
    s->size = (size_t)n;

    Node* prev = first;
    for (int i = 0; i < n; i++) {
        Node* cur = malloc(sizeof(Node));
        if (!cur) {
            perror("Malloc for node error");
            return -1;
        }
        // New node init 
        generate_gandom_string(cur->value);
        cur->next = NULL;
        pthread_mutex_init(&cur->sync, NULL);

        // Next step
        prev->next = cur;
        prev = cur;
    }
    return s;
}

void storage_destroy(Storage* s) {
    if (!s) return;

	while (s->first != NULL) {
        pthread_mutex_destroy(&s->first->sync);
		Node* tmp = s->first->next;
		free(s->first);
		s->first = tmp; 
	}
	
	free(s);
}