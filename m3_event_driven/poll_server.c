#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <poll.h>

#define PORT 5000
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 1024
#define IDLE_TIMEOUT 30

typedef struct {
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

void remove_client(struct pollfd fds[], client_info clients[], int index) {
    if (fds[index].fd > 0) {
        log_time();
        printf("[CLEANUP] closing fd=%d ip=%s port=%d slot=%d\n",
               fds[index].fd,
               clients[index].ip,
               clients[index].port,
               index);

        close(fds[index].fd);

        fds[index].fd = -1;
        fds[index].events = 0;
        fds[index].revents = 0;

        clients[index].ip[0] = '\0';
        clients[index].port = 0;
        clients[index].last_active = 0;
    }
}

int main() {
    setvbuf(stdout, NULL, _IOLBF, 0);

    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    char buffer[BUFFER_SIZE];

    struct pollfd fds[MAX_CLIENTS + 1];
    client_info clients[MAX_CLIENTS + 1];

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

    for (int i = 0; i < MAX_CLIENTS + 1; i++) {
        fds[i].fd = -1;
        fds[i].events = 0;
        fds[i].revents = 0;

        clients[i].ip[0] = '\0';
        clients[i].port = 0;
        clients[i].last_active = 0;
    }

    /*
     * Slot 0 is reserved for server_fd.
     * Client sockets start from slot 1.
     */
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    log_time();
    printf("[START] M3 poll() event-driven server running on port %d\n", PORT);

    while (1) {
        /*
         * poll() waits until:
         * 1. new client connects
         * 2. existing client sends data
         * 3. timeout happens after 1000 ms
         */
        int activity = poll(fds, MAX_CLIENTS + 1, 1000);

        if (activity < 0) {
            if (errno == EINTR) {
                continue;
            }

            perror("poll error");
            break;
        }

        time_t now = time(NULL);

        /*
         * Inactivity timeout check.
         * Client slots start from 1.
         */
        for (int i = 1; i < MAX_CLIENTS + 1; i++) {
            if (fds[i].fd > 0) {
                double idle_time = difftime(now, clients[i].last_active);

                if (idle_time >= IDLE_TIMEOUT) {
                    log_time();
                    printf("[TIMEOUT] fd=%d ip=%s port=%d idle=%.0f seconds slot=%d\n",
                           fds[i].fd,
                           clients[i].ip,
                           clients[i].port,
                           idle_time,
                           i);

                    remove_client(fds, clients, i);
                }
            }
        }

        /*
         * New connection event on server_fd.
         */
        if (fds[0].revents & POLLIN) {
            client_len = sizeof(client_addr);

            client_fd = accept(server_fd,
                               (struct sockaddr *)&client_addr,
                               &client_len);

            if (client_fd < 0) {
                perror("accept failed");
            } else {
                int added = 0;

                for (int i = 1; i < MAX_CLIENTS + 1; i++) {
                    if (fds[i].fd == -1) {
                        fds[i].fd = client_fd;
                        fds[i].events = POLLIN;
                        fds[i].revents = 0;

                        inet_ntop(AF_INET,
                                  &client_addr.sin_addr,
                                  clients[i].ip,
                                  sizeof(clients[i].ip));

                        clients[i].port = ntohs(client_addr.sin_port);
                        clients[i].last_active = time(NULL);

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
        }

        /*
         * Existing client events.
         */
        for (int i = 1; i < MAX_CLIENTS + 1; i++) {
            int fd = fds[i].fd;

            if (fd > 0 && (fds[i].revents & POLLIN)) {
                memset(buffer, 0, BUFFER_SIZE);

                int bytes_received = recv(fd, buffer, BUFFER_SIZE - 1, 0);

                if (bytes_received < 0) {
                    log_time();
                    printf("[ERROR] recv failed fd=%d ip=%s port=%d slot=%d\n",
                           fd,
                           clients[i].ip,
                           clients[i].port,
                           i);

                    remove_client(fds, clients, i);
                }
                else if (bytes_received == 0) {
                    log_time();
                    printf("[DISCONNECTED] fd=%d ip=%s port=%d slot=%d\n",
                           fd,
                           clients[i].ip,
                           clients[i].port,
                           i);

                    remove_client(fds, clients, i);
                }
                else {
                    buffer[bytes_received] = '\0';
                    clients[i].last_active = time(NULL);

                    log_time();
                    printf("[MESSAGE] fd=%d ip=%s port=%d bytes=%d slot=%d data=\"%s\"\n",
                           fd,
                           clients[i].ip,
                           clients[i].port,
                           bytes_received,
                           i,
                           buffer);

                    char *reply = "Message received by M3 poll event-driven server\n";

                    int sent = send(fd, reply, strlen(reply), 0);

                    if (sent < 0) {
                        log_time();
                        printf("[ERROR] send failed fd=%d ip=%s port=%d slot=%d\n",
                               fd,
                               clients[i].ip,
                               clients[i].port,
                               i);

                        remove_client(fds, clients, i);
                    }
                    else {
                        log_time();
                        printf("[REPLY_SENT] fd=%d bytes=%d slot=%d\n",
                               fd,
                               sent,
                               i);
                    }
                }
            }

            /*
             * Handle abnormal socket conditions.
             */
            if (fd > 0 && (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL))) {
                log_time();
                printf("[SOCKET_ERROR] fd=%d revents=%d slot=%d\n",
                       fd,
                       fds[i].revents,
                       i);

                remove_client(fds, clients, i);
            }

            fds[i].revents = 0;
        }

        fds[0].revents = 0;
    }

    close(server_fd);

    return 0;
}