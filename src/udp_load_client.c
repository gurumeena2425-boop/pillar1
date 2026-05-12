#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <errno.h>

#define SERVER_IP "127.0.0.1"
#define PORT 5001
#define CLIENTS 100
#define BUFFER_SIZE 1024

void run_udp_client(int id) {
    int sock;
    struct sockaddr_in server_addr;
    socklen_t server_len;
    char message[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];

    sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock < 0) {
        perror("UDP socket failed");
        exit(1);
    }

    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                   &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt SO_RCVTIMEO failed");
        close(sock);
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid server IP address");
        close(sock);
        exit(1);
    }

    snprintf(message, sizeof(message), "Hello from UDP client %d\n", id);

    server_len = sizeof(server_addr);

    int bytes_sent = sendto(
        sock,
        message,
        strlen(message),
        0,
        (struct sockaddr *)&server_addr,
        server_len
    );

    if (bytes_sent < 0) {
        printf("UDP client %d: sendto failed\n", id);
        perror("sendto");
        close(sock);
        exit(1);
    }

    memset(buffer, 0, sizeof(buffer));

    int bytes_received = recvfrom(
        sock,
        buffer,
        BUFFER_SIZE - 1,
        0,
        (struct sockaddr *)&server_addr,
        &server_len
    );

    if (bytes_received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            printf("UDP client %d: timeout waiting for reply\n", id);
        } else {
            printf("UDP client %d: recvfrom failed\n", id);
            perror("recvfrom");
        }

        close(sock);
        exit(1);
    }

    buffer[bytes_received] = '\0';

    printf("UDP client %d got reply: %s\n", id, buffer);

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
            run_udp_client(i);
            exit(0);
        }

        usleep(1000);
    }

    while (wait(NULL) > 0);

    printf("UDP load test completed with %d clients\n", CLIENTS);

    return 0;
}