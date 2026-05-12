#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>

#define SERVER_IP "127.0.0.1"
#define PORT 5001
#define BUFFER_SIZE 1024

int main() {
    int client_fd;
    struct sockaddr_in server_addr;
    socklen_t server_len;
    char buffer[BUFFER_SIZE];

    client_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (client_fd < 0) {
        perror("UDP socket creation failed");
        exit(1);
    }

    /*
       Timeout for recvfrom().
       UDP does not guarantee reply.
       So without timeout, client may wait forever.
    */
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO,
                   &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt SO_RCVTIMEO failed");
        close(client_fd);
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid server IP address");
        close(client_fd);
        exit(1);
    }

    printf("UDP client started. Type 'exit' to quit.\n");

    while (1) {
        printf("Enter message: ");

        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            printf("\nInput closed. Exiting UDP client.\n");
            break;
        }

        /*
           Remove newline only for checking exit.
           We still send the message as entered.
        */
        if (strncmp(buffer, "exit", 4) == 0) {
            printf("Closing UDP client...\n");
            break;
        }

        int bytes_sent = sendto(
            client_fd,
            buffer,
            strlen(buffer),
            0,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)
        );

        if (bytes_sent < 0) {
            perror("sendto failed");
            continue;
        }

        printf("[SENT] %d bytes to UDP server\n", bytes_sent);

        memset(buffer, 0, BUFFER_SIZE);
        server_len = sizeof(server_addr);

        int bytes_received = recvfrom(
            client_fd,
            buffer,
            BUFFER_SIZE - 1,
            0,
            (struct sockaddr *)&server_addr,
            &server_len
        );

        if (bytes_received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("[TIMEOUT] No reply received from UDP server within 5 seconds\n");
            } else {
                perror("recvfrom failed");
            }

            continue;
        }

        buffer[bytes_received] = '\0';

        printf("[REPLY] Server replied: %s\n", buffer);
    }

    close(client_fd);
    return 0;
}