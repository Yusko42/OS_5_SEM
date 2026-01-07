#define _GNU_SOURCE

#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "handler.h"

int main() {
    int listen_sock_fd;
    if ((listen_sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation failed");
        return 1;
    }

    struct sockaddr_in listen_addr;
    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port = htons(PROXY_PORT);

    int is_reuse_addr = 1;
    if (setsockopt(listen_sock_fd, SOL_SOCKET, SO_REUSEADDR, &is_reuse_addr, sizeof(is_reuse_addr)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(listen_sock_fd);
        return 1;
    }

    if (bind(listen_sock_fd, (const struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0) {
        perror("socket bind failed");
        close(listen_sock_fd);
        return 1;
    }

    if (listen(listen_sock_fd, CONN_QUEUE_SIZE) < 0) {
        perror("socket listen failed");
        close(listen_sock_fd);
        return 1;
    }

    printf("[HTTP-PROXY] Listening at the port %d\n", PROXY_PORT);
    while(1) {
        // Структура для сокета клиента
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_sock_fd;
        
        // Ожидание присоединения клиента к прокси
        if ((client_sock_fd = accept(listen_sock_fd, (struct sockaddr *)&client_addr, &client_addr_len)) < 0) {
            perror("new client accept failed");
            continue;
        }

        // Выделяем память под args
        int *arg = (int *)malloc(sizeof(int));
        if (!arg) {
            perror("memory alloc for thread function arg failed");
            continue;
        }
        *arg = client_sock_fd;
         
        pthread_t connection_tid;
        pthread_attr_t connection_attr;

        pthread_attr_init(&connection_attr);
        pthread_attr_setdetachstate(&connection_attr, PTHREAD_CREATE_DETACHED);

        if (pthread_create(&connection_tid, &connection_attr, handle_client, arg) != 0) {
            perror("Thread creation for client handling failed");
            free(arg);
            close(client_sock_fd);
        }

        pthread_attr_destroy(&connection_attr);
    }

    close(listen_sock_fd);

    return 0;
}