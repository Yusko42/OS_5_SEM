#include "handler.h"

int parse_request_line(char *buffer) {
    char method[16];
    char url[2048];
    char version[12];

    if (sscanf(buffer, "%15s %2047s %11s", method, url, version) != 3) {
        printf("Parse HTTP query request line failed\n");
        return 1;
    }

    if (strncmp(method, "GET", 3) != 0) {
        printf("Only GET method is supported. Method, url tracked: %s, %s\n", method, url);
        return 1;
    }

    if ((strncmp(version, "HTTP/1.0", 8) != 0) && (strncmp(version, "HTTP/1.1", 8) != 0)) {
        printf("Only HTTP/1.0 or HTTP/1.1 version supported. Version, url tracked: %s, %s\n", version, url);
        return 1;
    }

    char *version_pos = strstr(buffer, version);
    if (version_pos)
        memcpy(version_pos, "HTTP/1.1", 8);

    return 0;
}

int parse_headers(char *buffer, char *host) {
    char *headers = strstr(buffer, "\r\n");
    if (!headers) {
        printf("Headers not found\n");
        return 1;
    }

    headers += 2;
    host[0] = '\0';

    char *connection_h = NULL;
    while (1) {
        // завершающие символы
        if ((headers[0] == '\r') && (headers[1] == '\n'))
            break;

        char *colon = strchr(headers, ':');
        if (!colon) {
            printf("Invalid header structure: colon not found\n");
            return 1;
        }
        size_t header_name_len = colon - headers;
        if ((header_name_len == 0) || (header_name_len >= H_NAME_SIZE)) {
            printf("Invalid header structure: invalid colon location\n");
            return 1;
        }

        // Имя заголовка 
        char header_name[H_NAME_SIZE];
        memcpy(header_name, headers, header_name_len);
        header_name[header_name_len] = '\0';

        // Значение заголовка
        char *header_val_start = colon + 1;
        while (*header_val_start == ' ')
            header_val_start++;

        char *header_val_end = strstr(header_val_start, "\r\n");
        if (!header_val_end) {
            printf("Invalid header structure: header end not found\n");
            return 1;
        }

        // Длина значения
        size_t header_value_len = header_val_end - header_val_start;
        if ((header_value_len == 0) || (header_value_len >= H_VAL_SIZE)) {
            printf("Invalid header structure: invalid header end location\n");
            return 1;
        }

        // Передаём host
        if (strcasecmp(header_name, "host") == 0) {
            memcpy(host, header_val_start, header_value_len);
            host[header_value_len] = '\0';
        }

        // Запоминаем connection
        if (strcasecmp(header_name, "connection") == 0)
            connection_h = headers;

        headers = header_val_end + 2;
    }

    if (host[0] == 0) {
        printf("Host header not found\n");
        return 1;
    }

    // Меняем/устанавливаем значение connection на close (для упрощения)
    if (connection_h) {
        char *header_end = strstr(connection_h, "\r\n");
        if (header_end) {
            char *colon = strchr(connection_h, ':');
            if (colon) {
                char *val_start = colon + 1;
                while (*val_start == ' ') val_start++;
                
                size_t old_val_len = header_end - val_start;
                
                memcpy(val_start, "close", 5);
                
                if (old_val_len > 5) {
                    char *after_close = val_start + 5;
                    char *after_old = header_end;
                    memmove(after_close, after_old, strlen(after_old) + 1);
                }
            }
        }
    }
    else {
        char *headers_end = strstr(buffer, "\r\n\r\n");
        if (headers_end) {
            const char *connection_line = "\nConnection: close\r\n";
            size_t connection_len = strlen(connection_line);
            memmove(headers_end + connection_len, headers_end, strlen(headers_end) + 1);
            memcpy(headers_end, connection_line, connection_len);
        }
    }

    return 0;
}

int resolve_host(const char *host, struct sockaddr_in *server) {
    char host_name[H_VAL_SIZE];
    int port = HTTP_PORT;

    const char *colon = strchr(host, ':');
    if (colon) {
        size_t host_len = colon - host;
        memcpy(host_name, host, host_len);
        host_name[host_len] = '\0';

        port = atoi(colon + 1);
        if (port == 0)
            port = HTTP_PORT;
    }
    else {
        strncpy(host_name, host, H_VAL_SIZE);
        host_name[H_VAL_SIZE - 1] = '\0';
    }

    // Структура адреса
    struct addrinfo info, *result;
    memset(&info, 0, sizeof(info));
    info.ai_family = AF_INET;
    info.ai_socktype = SOCK_STREAM;

    // Резловим имя через getaddtmethod
    if (getaddrinfo(host_name, NULL, &info, &result) != 0) {
        perror("Resolve host name failed");
        return 1;
    }

    struct sockaddr_in *ipv4 = (struct sockaddr_in *)result->ai_addr;
    server->sin_family = AF_INET;
    server->sin_addr = ipv4->sin_addr;
    server->sin_port = htons(port);

    freeaddrinfo(result);

    return 0;
}


void *handle_client(void *arg) {
    int client_sock_fd = *(int *)arg;
    free(arg);

    // Буфер для заголовков
    char buffer[BUFF_SIZE];
    ssize_t readed = read(client_sock_fd, buffer, BUFF_SIZE - 1);
    if (readed <= 0) {
        if (readed == -1) perror("Failed when read HTTP query");
        close(client_sock_fd);
        return NULL;
    }
    buffer[readed] = '\0';

    // парсинг заголовков
    if (parse_request_line(buffer)) {
        close(client_sock_fd);
        return NULL;
    }

    char host[H_VAL_SIZE];
    if (parse_headers(buffer, host)) {
        close(client_sock_fd);
        return NULL;
    }

    // Разрешение доменного имени
    struct sockaddr_in remote_addr;
    memset(&remote_addr, 0, sizeof(remote_addr));
    if (resolve_host(host, &remote_addr)) {
        close(client_sock_fd);
        return NULL;
    }
    
    int remote_sock_fd;
    if ((remote_sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Remote socket creation failed");
        close(client_sock_fd);
        return NULL;
    }

    // Установка таймаута на подключение
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(remote_sock_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(remote_sock_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    // Соединение с сокетом
    if (connect(remote_sock_fd, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0) {
        perror("Connect to remote failed");
        close(client_sock_fd);
        close(remote_sock_fd);
        return NULL;
    }
    
    // Находим "http://" в запросе и заменяем на относительный путь
    char *url_start = strstr(buffer, "http://");
    if (url_start) {
        char *path_start = url_start + 7;
        // Ищем следующий слеш после хоста
        char *first_slash = strchr(path_start, '/');
        if (first_slash) {
            size_t path_len = strlen(first_slash);
            memmove(url_start, first_slash, path_len + 1); // +1 для нулевого терминатора
        }
    }

    printf("Modifies request:\n%s\n", buffer);

    // 1. Отправка на удаленный сервер
    ssize_t sent = 0, total_read = readed;
    while (sent < total_read) {
        ssize_t n = write(remote_sock_fd, buffer + sent, total_read - sent);
        if (n <= 0) {
            perror("Write to remote from client failed");
            close(client_sock_fd);
            close(remote_sock_fd);
            return NULL;
        }

        sent += n;
    }

     printf("The request has been sent\n");

    // 2. Чтение ответа от сервера, пересылка клиенту
    ssize_t remote_read;
    while ((remote_read = read(remote_sock_fd, buffer, BUFF_SIZE)) > 0) {
        ssize_t client_sent = 0;
        while (client_sent < remote_read) {
            ssize_t n = write(client_sock_fd, buffer + client_sent, remote_read - client_sent);
            if (n <= 0) {
                perror("Write to client from remote failed");
                close(client_sock_fd);
                close(remote_sock_fd);
                return NULL;
            }
            client_sent += n;
        }
    }

    if (remote_read == -1)
        perror("Read from remote failed");
    else if (remote_read == 0)
        printf("Remote has closed connection\n");

    printf("Connection has been processed.\n");

    close(client_sock_fd);
    close(remote_sock_fd);

    return NULL;
}