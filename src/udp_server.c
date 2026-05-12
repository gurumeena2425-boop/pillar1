#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>

#define PORT 5001
#define BUFFER_SIZE 1024

void print_time() {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    printf("[%02d:%02d:%02d] ",
           t->tm_hour,
           t->tm_min,
           t->tm_sec);
}

int main() {
    int server_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    char buffer[BUFFER_SIZE];

    setvbuf(stdout, NULL, _IONBF, 0);

    server_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (server_fd < 0) {
        perror("UDP socket creation failed");
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
        perror("UDP bind failed");
        close(server_fd);
        exit(1);
    }

    print_time();
    printf("[STARTED] UDP server running on port %d\n", PORT);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        memset(&client_addr, 0, sizeof(client_addr));
        client_len = sizeof(client_addr);

        int bytes_received = recvfrom(
            server_fd,
            buffer,
            BUFFER_SIZE - 1,
            0,
            (struct sockaddr *)&client_addr,
            &client_len
        );

        if (bytes_received < 0) {
            print_time();
            printf("[ERROR] recvfrom failed\n");
            perror("recvfrom");
            continue;
        }

        buffer[bytes_received] = '\0';

        char client_ip[INET_ADDRSTRLEN];

        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        int client_port = ntohs(client_addr.sin_port);

        print_time();
        printf("[UDP MESSAGE] %s:%d -> %s\n",
               client_ip,
               client_port,
               buffer);

        char *reply = "Message received by UDP server";

        int bytes_sent = sendto(
            server_fd,
            reply,
            strlen(reply),
            0,
            (struct sockaddr *)&client_addr,
            client_len
        );

        if (bytes_sent < 0) {
            print_time();
            printf("[ERROR] sendto failed to %s:%d\n", client_ip, client_port);
            perror("sendto");
            continue;
        }

        print_time();
        printf("[UDP REPLY SENT] to %s:%d\n", client_ip, client_port);
    }

    close(server_fd);
    return 0;
}