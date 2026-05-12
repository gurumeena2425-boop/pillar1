#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <sys/epoll.h>

#define PORT 5000
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 1024
#define MAX_EVENTS 1024
#define IDLE_TIMEOUT 30

typedef struct {
    int fd;
    char ip[INET_ADDRSTRLEN];
    int port;
    time_t last_active;
} client_info;

void log_time() {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    printf("[%02d:%02d:%02d] ",
           t->tm_hour, t->tm_min, t->tm_sec);
}

int find_client(client_info clients[], int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == fd) {
            return i;
        }
    }
    return -1;
}

int add_client(client_info clients[], int fd, struct sockaddr_in *client_addr) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == -1) {
            clients[i].fd = fd;
            clients[i].last_active = time(NULL);

            inet_ntop(AF_INET,
                      &client_addr->sin_addr,
                      clients[i].ip,
                      sizeof(clients[i].ip));

            clients[i].port = ntohs(client_addr->sin_port);

            return i;
        }
    }

    return -1;
}

void remove_client(int epoll_fd, client_info clients[], int index) {
    if (index < 0 || clients[index].fd < 0) {
        return;
    }

    int fd = clients[index].fd;

    log_time();
    printf("[CLEANUP] closing fd=%d ip=%s port=%d slot=%d\n",
           fd,
           clients[index].ip,
           clients[index].port,
           index);

    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);

    clients[index].fd = -1;
    clients[index].ip[0] = '\0';
    clients[index].port = 0;
    clients[index].last_active = 0;
}

int main() {
    setvbuf(stdout, NULL, _IOLBF, 0);

    int server_fd, client_fd, epoll_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    char buffer[BUFFER_SIZE];

    client_info clients[MAX_CLIENTS];
    struct epoll_event event;
    struct epoll_event events[MAX_EVENTS];

    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].ip[0] = '\0';
        clients[i].port = 0;
        clients[i].last_active = 0;
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

    /*
     * epoll_create1() creates an epoll instance.
     * This epoll instance will watch server_fd and all client fds.
     */
    epoll_fd = epoll_create1(0);

    if (epoll_fd < 0) {
        perror("epoll_create1 failed");
        close(server_fd);
        exit(1);
    }

    /*
     * Add server_fd to epoll.
     * Whenever a new client connects, server_fd becomes readable.
     */
    event.events = EPOLLIN;
    event.data.fd = server_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) < 0) {
        perror("epoll_ctl ADD server_fd failed");
        close(server_fd);
        close(epoll_fd);
        exit(1);
    }

    log_time();
    printf("[START] M3 epoll() event-driven server running on port %d\n", PORT);

    while (1) {
        /*
         * epoll_wait waits for active events.
         * Timeout = 1000 ms, so every 1 second we also check idle clients.
         */
        int activity = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);

        if (activity < 0) {
            if (errno == EINTR) {
                continue;
            }

            perror("epoll_wait error");
            break;
        }

        time_t now = time(NULL);

        /*
         * Inactivity timeout check.
         */
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd > 0) {
                double idle_time = difftime(now, clients[i].last_active);

                if (idle_time >= IDLE_TIMEOUT) {
                    log_time();
                    printf("[TIMEOUT] fd=%d ip=%s port=%d idle=%.0f seconds slot=%d\n",
                           clients[i].fd,
                           clients[i].ip,
                           clients[i].port,
                           idle_time,
                           i);

                    remove_client(epoll_fd, clients, i);
                }
            }
        }

        /*
         * Handle all events returned by epoll_wait.
         */
        for (int i = 0; i < activity; i++) {
            int fd = events[i].data.fd;

            /*
             * New connection event.
             */
            if (fd == server_fd) {
                client_len = sizeof(client_addr);

                client_fd = accept(server_fd,
                                   (struct sockaddr *)&client_addr,
                                   &client_len);

                if (client_fd < 0) {
                    perror("accept failed");
                    continue;
                }

                int slot = add_client(clients, client_fd, &client_addr);

                if (slot < 0) {
                    log_time();
                    printf("[REJECTED] too many clients, closing fd=%d\n", client_fd);
                    close(client_fd);
                    continue;
                }

                event.events = EPOLLIN;
                event.data.fd = client_fd;

                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) < 0) {
                    perror("epoll_ctl ADD client failed");
                    close(client_fd);
                    clients[slot].fd = -1;
                    continue;
                }

                log_time();
                printf("[CONNECTED] fd=%d ip=%s port=%d slot=%d\n",
                       client_fd,
                       clients[slot].ip,
                       clients[slot].port,
                       slot);
            }

            /*
             * Existing client event.
             */
            else {
                int slot = find_client(clients, fd);

                if (slot < 0) {
                    log_time();
                    printf("[UNKNOWN_FD] fd=%d not found in client table\n", fd);
                    continue;
                }

                if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                    log_time();
                    printf("[SOCKET_ERROR] fd=%d ip=%s port=%d slot=%d events=%u\n",
                           fd,
                           clients[slot].ip,
                           clients[slot].port,
                           slot,
                           events[i].events);

                    remove_client(epoll_fd, clients, slot);
                    continue;
                }

                if (events[i].events & EPOLLIN) {
                    memset(buffer, 0, BUFFER_SIZE);

                    int bytes_received = recv(fd, buffer, BUFFER_SIZE - 1, 0);

                    if (bytes_received < 0) {
                        log_time();
                        printf("[ERROR] recv failed fd=%d ip=%s port=%d slot=%d\n",
                               fd,
                               clients[slot].ip,
                               clients[slot].port,
                               slot);

                        remove_client(epoll_fd, clients, slot);
                    }
                    else if (bytes_received == 0) {
                        log_time();
                        printf("[DISCONNECTED] fd=%d ip=%s port=%d slot=%d\n",
                               fd,
                               clients[slot].ip,
                               clients[slot].port,
                               slot);

                        remove_client(epoll_fd, clients, slot);
                    }
                    else {
                        buffer[bytes_received] = '\0';
                        clients[slot].last_active = time(NULL);

                        log_time();
                        printf("[MESSAGE] fd=%d ip=%s port=%d bytes=%d slot=%d data=\"%s\"\n",
                               fd,
                               clients[slot].ip,
                               clients[slot].port,
                               bytes_received,
                               slot,
                               buffer);

                        char *reply = "Message received by M3 epoll event-driven server\n";

                        int sent = send(fd, reply, strlen(reply), 0);

                        if (sent < 0) {
                            log_time();
                            printf("[ERROR] send failed fd=%d ip=%s port=%d slot=%d\n",
                                   fd,
                                   clients[slot].ip,
                                   clients[slot].port,
                                   slot);

                            remove_client(epoll_fd, clients, slot);
                        }
                        else {
                            log_time();
                            printf("[REPLY_SENT] fd=%d bytes=%d slot=%d\n",
                                   fd,
                                   sent,
                                   slot);
                        }
                    }
                }
            }
        }
    }

    close(server_fd);
    close(epoll_fd);

    return 0;
}