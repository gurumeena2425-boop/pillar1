#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[1024];

    // 1. Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0) {
        printf("Socket creation failed\n");
        return -1;
    }

    // 2. Server details
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(5000);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // 3. Connect to server
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Connection failed\n");
        return -1;
    }

    printf("Connected to server\n");

    while (1) {
        // 4. Take input from user
        printf("Enter message: ");
        fgets(buffer, sizeof(buffer), stdin);

        // 5. Send to server
        send(sock, buffer, strlen(buffer), 0);

        // Exit condition
        if (strncmp(buffer, "exit", 4) == 0) {
            break;
        }

        // 6. Receive reply from server
        memset(buffer, 0, sizeof(buffer));
        read(sock, buffer, sizeof(buffer));

        printf("Server says: %s\n", buffer);
    }

    // 7. Close socket
    close(sock);
    return 0;
}
