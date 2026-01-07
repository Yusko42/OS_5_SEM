#define _GNU_SOURCE

#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define HTTP_PORT 80
#define PROXY_PORT 8080
#define CONN_QUEUE_SIZE 256
#define BUFF_SIZE 128*1024
#define H_NAME_SIZE 256
#define H_VAL_SIZE 2048

void *handle_client(void *arg);
