#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    int server_fd, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char buffer[1024];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket failed");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(5000);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(1);
    }

    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        exit(1);
    }

    printf("Server running on port 5000...\n");

    while (1) {
        addr_size = sizeof(client_addr);

        printf("Waiting for client...\n");

        client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &addr_size);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        printf("Client connected\n");

        while (1) {
            memset(buffer, 0, sizeof(buffer));

            int bytes = read(client_socket, buffer, sizeof(buffer) - 1);

            if (bytes < 0) {
                perror("Read failed");
                break;
            }

            if (bytes == 0) {
                printf("Client disconnected\n");
                break;
            }

            printf("Client says: %s\n", buffer);

            char *reply = "Message received by server\n";
            write(client_socket, reply, strlen(reply));
        }

        close(client_socket);
        printf("Ready for next client...\n");
    }

    close(server_fd);
    return 0;
}