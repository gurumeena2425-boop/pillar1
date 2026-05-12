#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>

#define PORT 5000
#define BUFFER_SIZE 1024

void print_time() {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    printf("[%02d:%02d:%02d] ",
           t->tm_hour,
           t->tm_min,
           t->tm_sec);
}

void handle_client(int client_fd, struct sockaddr_in client_addr) {
    char buffer[BUFFER_SIZE];

    char client_ip[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    int client_port = ntohs(client_addr.sin_port);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);

        int bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);

        if (bytes_received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                print_time();
                printf("[TIMEOUT] No data received from %s:%d for 10 seconds\n",
                       client_ip, client_port);
            } else {
                print_time();
                printf("[ERROR] Receive failed from %s:%d\n",
                       client_ip, client_port);
                perror("recv");
            }

            fflush(stdout);
            break;
        }

        if (bytes_received == 0) {
            print_time();
            printf("[DISCONNECTED] %s:%d\n", client_ip, client_port);
            fflush(stdout);
            break;
        }

        buffer[bytes_received] = '\0';

        print_time();
        printf("[MESSAGE] %s:%d -> %s", client_ip, client_port, buffer);
        fflush(stdout);

        char *reply = "Message received by multi-client server\n";

        int bytes_sent = send(client_fd, reply, strlen(reply), 0);

        if (bytes_sent < 0) {
            print_time();
            printf("[ERROR] Send failed to %s:%d\n", client_ip, client_port);
            perror("send");
            fflush(stdout);
            break;
        }
    }
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;

    signal(SIGCHLD, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    int opt = 1;

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR failed");
        close(server_fd);
        exit(1);
    }

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
    printf("[STARTED] Multi-client TCP server running on port %d\n", PORT);
    fflush(stdout);

    while (1) {
        client_len = sizeof(client_addr);

        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd < 0) {
            print_time();
            perror("[ERROR] Accept failed");
            fflush(stdout);
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];

        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        int client_port = ntohs(client_addr.sin_port);

        print_time();
        printf("[CONNECTED] %s:%d\n", client_ip, client_port);
        fflush(stdout);

        pid_t pid = fork();

        if (pid < 0) {
            print_time();
            perror("[ERROR] Fork failed");
            fflush(stdout);
            close(client_fd);
            continue;
        }

        if (pid == 0) {
            close(server_fd);

            struct timeval timeout;
            timeout.tv_sec = 10;
            timeout.tv_usec = 0;

            if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO,
                           &timeout, sizeof(timeout)) < 0) {
                print_time();
                perror("[ERROR] setsockopt SO_RCVTIMEO failed");
                fflush(stdout);
                close(client_fd);
                exit(1);
            }

            handle_client(client_fd, client_addr);

            close(client_fd);
            exit(0);
        } else {
            close(client_fd);
        }
    }

    close(server_fd);
    return 0;
}