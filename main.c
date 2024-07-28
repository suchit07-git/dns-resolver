#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <unistd.h>

int main() {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    int udpSocket;
    struct sockaddr_in client_addr;
    udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket == -1) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }
    int reuse = 1;
    if (setsockopt(udpSocket, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
        perror("SO_REUSEPORT failed");
        return EXIT_FAILURE;
    }
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(2053),
        .sin_addr = { htonl(INADDR_ANY) },
    };
    if (bind(udpSocket, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        perror("Bind failed");
        return EXIT_FAILURE;
    }
    int bytesRead;
    char buffer[512];
    socklen_t client_addr_len = sizeof(client_addr);
    while (1) {
        bytesRead = recvfrom(udpSocket, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_addr_len);
        if (bytesRead == -1) {
            perror("Error receiving data");
            break;
        }
        buffer[bytesRead] = '\0';
        printf("Received %d bytes: %s\n", bytesRead, buffer);
        char response[1] = { '\0' };
        if (sendto(udpSocket, response, sizeof(response), 0, (struct sockaddr*)&client_addr, sizeof(client_addr)) == -1) {
            perror("Failed to send response");
        }
    }
    close(udpSocket);
    return 0;
}
