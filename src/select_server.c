#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#define PORT 5000
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 1024

void print_time() {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    printf("[%02d:%02d:%02d] ",
           t->tm_hour,
           t->tm_min,
           t->tm_sec);
}

int send_all(int fd, const char *msg) {
    int total_sent = 0;
    int msg_len = strlen(msg);

    while (total_sent < msg_len) {
        int sent = send(fd, msg + total_sent, msg_len - total_sent, 0);

        if (sent < 0) {
            return -1;
        }

        if (sent == 0) {
            break;
        }

        total_sent += sent;
    }

    return total_sent;
}

int main() {
    int server_fd, client_fd;
    int client_sockets[MAX_CLIENTS];
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    char buffer[BUFFER_SIZE];

    fd_set readfds;
    int max_fd;

    signal(SIGPIPE, SIG_IGN);

    setvbuf(stdout, NULL, _IONBF, 0);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_sockets[i] = 0;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(1);
    }

    if (listen(server_fd, 1024) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(1);
    }

    print_time();
    printf("[STARTED] select() multi-client server running on port %d\n", PORT);

    while (1) {
        FD_ZERO(&readfds);

        FD_SET(server_fd, &readfds);
        max_fd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int fd = client_sockets[i];

            if (fd > 0) {
                FD_SET(fd, &readfds);

                if (fd > max_fd) {
                    max_fd = fd;
                }
            }
        }

        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0) {
            if (errno == EINTR) {
                continue;
            }

            perror("select error");
            continue;
        }

        if (FD_ISSET(server_fd, &readfds)) {
            client_len = sizeof(client_addr);

            client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

            if (client_fd < 0) {
                perror("Accept failed");
                continue;
            }

            char client_ip[INET_ADDRSTRLEN];

            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
            int client_port = ntohs(client_addr.sin_port);

            if (client_fd >= FD_SETSIZE) {
                print_time();
                printf("[ERROR] client fd %d is too high for select(). Connection rejected.\n", client_fd);
                close(client_fd);
                continue;
            }

            print_time();
            printf("[CONNECTED] %s:%d fd=%d\n", client_ip, client_port, client_fd);

            int added = 0;

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = client_fd;
                    added = 1;
                    break;
                }
            }

            if (!added) {
                print_time();
                printf("[ERROR] Too many clients. Connection rejected.\n");
                close(client_fd);
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int fd = client_sockets[i];

            if (fd > 0 && FD_ISSET(fd, &readfds)) {
                memset(buffer, 0, BUFFER_SIZE);

                int bytes_received = recv(fd, buffer, BUFFER_SIZE - 1, 0);

                if (bytes_received < 0) {
                    print_time();
                    printf("[ERROR] recv failed from fd %d\n", fd);
                    perror("recv");

                    close(fd);
                    client_sockets[i] = 0;
                } else if (bytes_received == 0) {
                    print_time();
                    printf("[DISCONNECTED] client fd %d\n", fd);

                    close(fd);
                    client_sockets[i] = 0;
                } else {
                    buffer[bytes_received] = '\0';

                    print_time();
                    printf("[MESSAGE] fd %d -> %s", fd, buffer);

                    char *reply = "Message received by select server\n";

                    if (send_all(fd, reply) < 0) {
                        print_time();
                        printf("[ERROR] send failed to fd %d\n", fd);
                        perror("send");

                        close(fd);
                        client_sockets[i] = 0;
                    }
                }
            }
        }
    }

    close(server_fd);
    return 0;
}