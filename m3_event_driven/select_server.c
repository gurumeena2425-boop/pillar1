#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>

#define PORT 5000
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 1024
#define IDLE_TIMEOUT 30

typedef struct {
    int fd;
    time_t last_active;
    char ip[INET_ADDRSTRLEN];
    int port;
} client_info;

void log_time() {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    printf("[%02d:%02d:%02d] ",
           t->tm_hour, t->tm_min, t->tm_sec);
}

void remove_client(client_info clients[], int index) {
    if (clients[index].fd > 0) {
        log_time();
        printf("[CLEANUP] closing fd=%d ip=%s port=%d\n",
               clients[index].fd,
               clients[index].ip,
               clients[index].port);

        close(clients[index].fd);
        clients[index].fd = 0;
        clients[index].last_active = 0;
        clients[index].ip[0] = '\0';
        clients[index].port = 0;
    }
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    char buffer[BUFFER_SIZE];

    client_info clients[MAX_CLIENTS];

    fd_set readfds;
    int max_fd;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = 0;
        clients[i].last_active = 0;
        clients[i].ip[0] = '\0';
        clients[i].port = 0;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("socket failed");
        exit(1);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(server_fd);
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(1);
    }

    if (listen(server_fd, 128) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(1);
    }

    log_time();
    printf("[START] M3 select() event-driven server running on port %d\n", PORT);

    while (1) {
        FD_ZERO(&readfds);

        FD_SET(server_fd, &readfds);
        max_fd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int fd = clients[i].fd;

            if (fd > 0) {
                FD_SET(fd, &readfds);

                if (fd > max_fd) {
                    max_fd = fd;
                }
            }
        }

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select(max_fd + 1, &readfds, NULL, NULL, &timeout);

        if (activity < 0) {
            if (errno == EINTR) {
                continue;
            }

            perror("select error");
            break;
        }

        time_t now = time(NULL);

        /*
         * Timeout check:
         * Even if no client sends data, select wakes every 1 second.
         * Then we check idle clients and close them.
         */
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd > 0) {
                double idle_time = difftime(now, clients[i].last_active);

                if (idle_time >= IDLE_TIMEOUT) {
                    log_time();
                    printf("[TIMEOUT] fd=%d ip=%s port=%d idle=%.0f seconds\n",
                           clients[i].fd,
                           clients[i].ip,
                           clients[i].port,
                           idle_time);

                    remove_client(clients, i);
                }
            }
        }

        /*
         * New connection event:
         * If server_fd is ready, one new client is waiting.
         */
        if (FD_ISSET(server_fd, &readfds)) {
            client_len = sizeof(client_addr);

            client_fd = accept(server_fd,
                               (struct sockaddr *)&client_addr,
                               &client_len);

            if (client_fd < 0) {
                perror("accept failed");
                continue;
            }

            int added = 0;

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd == 0) {
                    clients[i].fd = client_fd;
                    clients[i].last_active = time(NULL);

                    inet_ntop(AF_INET,
                              &client_addr.sin_addr,
                              clients[i].ip,
                              sizeof(clients[i].ip));

                    clients[i].port = ntohs(client_addr.sin_port);

                    log_time();
                    printf("[CONNECTED] fd=%d ip=%s port=%d slot=%d\n",
                           client_fd,
                           clients[i].ip,
                           clients[i].port,
                           i);

                    added = 1;
                    break;
                }
            }

            if (!added) {
                log_time();
                printf("[REJECTED] too many clients, closing fd=%d\n", client_fd);
                close(client_fd);
            }
        }

        /*
         * Existing client events:
         * If a client fd is ready, read data from it.
         */
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int fd = clients[i].fd;

            if (fd > 0 && FD_ISSET(fd, &readfds)) {
                memset(buffer, 0, BUFFER_SIZE);

                int bytes_received = recv(fd, buffer, BUFFER_SIZE - 1, 0);

                if (bytes_received < 0) {
                    log_time();
                    printf("[ERROR] recv failed fd=%d ip=%s port=%d\n",
                           fd,
                           clients[i].ip,
                           clients[i].port);

                    remove_client(clients, i);
                }
                else if (bytes_received == 0) {
                    log_time();
                    printf("[DISCONNECTED] fd=%d ip=%s port=%d\n",
                           fd,
                           clients[i].ip,
                           clients[i].port);

                    remove_client(clients, i);
                }
                else {
                    buffer[bytes_received] = '\0';
                    clients[i].last_active = time(NULL);

                    log_time();
                    printf("[MESSAGE] fd=%d ip=%s port=%d bytes=%d data=\"%s\"\n",
                           fd,
                           clients[i].ip,
                           clients[i].port,
                           bytes_received,
                           buffer);

                    char *reply = "Message received by M3 select event-driven server\n";

                    int sent = send(fd, reply, strlen(reply), 0);

                    if (sent < 0) {
                        log_time();
                        printf("[ERROR] send failed fd=%d ip=%s port=%d\n",
                               fd,
                               clients[i].ip,
                               clients[i].port);

                        remove_client(clients, i);
                    }
                    else {
                        log_time();
                        printf("[REPLY_SENT] fd=%d bytes=%d\n", fd, sent);
                    }
                }
            }
        }
    }

    close(server_fd);

    return 0;
}