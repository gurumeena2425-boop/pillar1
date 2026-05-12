#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <errno.h>

#define SERVER_IP "127.0.0.1"
#define PORT 5000
#define CLIENTS 200
#define BUFFER_SIZE 1024

void run_client(int id) {
    int sock;
    struct sockaddr_in server_addr;
    char message[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket failed");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("invalid server IP address");
        close(sock);
        exit(1);
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Client %d: connect failed\n", id);
        perror("connect");
        close(sock);
        exit(1);
    }

    snprintf(message, sizeof(message), "Hello from client %d\n", id);

    int bytes_sent = send(sock, message, strlen(message), 0);
    if (bytes_sent < 0) {
        printf("Client %d: send failed\n", id);
        perror("send");
        close(sock);
        exit(1);
    }

    memset(buffer, 0, sizeof(buffer));

    int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received < 0) {
        printf("Client %d: recv failed\n", id);
        perror("recv");
        close(sock);
        exit(1);
    }

    if (bytes_received == 0) {
        printf("Client %d: server closed connection without reply\n", id);
        close(sock);
        exit(1);
    }

    buffer[bytes_received] = '\0';

    printf("Client %d got reply: %s", id, buffer);
    fflush(stdout);

    close(sock);
}

int main() {
    for (int i = 1; i <= CLIENTS; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork failed");
            exit(1);
        }

        if (pid == 0) {
            run_client(i);
            exit(0);
        }

        usleep(1000);
    }

    while (wait(NULL) > 0);

    printf("Load test completed with %d clients\n", CLIENTS);

    return 0;
}