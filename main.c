#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

// Struct for DNS message header
typedef struct {
    unsigned short id;
    unsigned char qr : 1;
    unsigned char opcode : 4;
    unsigned char aa : 1;
    unsigned char tc : 1;
    unsigned char rd : 1;
    unsigned char ra : 1;
    unsigned char z : 3;
    unsigned char rcode : 4;
    unsigned short qdcount;
    unsigned short ancount;
    unsigned short nscount;
    unsigned short arcount;
} msg_header;

void pack_header(msg_header header, char *buf) {
    buf[0] = (header.id >> 8) & 0xff;
    buf[1] = header.id & 0xff;
    buf[2] = (header.qr << 7) | (header.opcode << 3) | (header.aa << 2) |
             (header.tc << 1) | (header.rd);
    buf[3] = (header.ra << 7) | (header.z << 4) | (header.rcode);
    buf[4] = (header.qdcount >> 8) & 0xff;
    buf[5] = header.qdcount & 0xff;
    buf[6] = (header.ancount >> 8) & 0xff;
    buf[7] = header.ancount & 0xff;
    buf[8] = (header.nscount >> 8) & 0xff;
    buf[9] = header.nscount & 0xff;
    buf[10] = (header.arcount >> 8) & 0xff;
    buf[11] = header.arcount & 0xff;
}

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
    if (setsockopt(udpSocket, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) <
        0) {
        perror("SO_REUSEPORT failed");
        return EXIT_FAILURE;
    }
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(2053),
        .sin_addr = {htonl(INADDR_ANY)},
    };
    if (bind(udpSocket, (struct sockaddr *)&server_addr, sizeof(server_addr)) !=
        0) {
        perror("Bind failed");
        return EXIT_FAILURE;
    }
    int bytesRead;
    char buffer[512];
    socklen_t client_addr_len = sizeof(client_addr);
    while (1) {
        bytesRead = recvfrom(udpSocket, buffer, sizeof(buffer), 0,
                             (struct sockaddr *)&client_addr, &client_addr_len);
        if (bytesRead == -1) {
            perror("Error receiving data");
            break;
        }
        buffer[bytesRead] = '\0';
        printf("Received %d bytes: %s\n", bytesRead, buffer);
        msg_header _header = {
            .id = 22,
            .qr = 1,
            .opcode = 0,
            .aa = 0,
            .tc = 0,
            .rd = 0,
            .ra = 0,
            .z = 0,
            .rcode = 0,
            .qdcount = 0,
            .ancount = 0,
            .nscount = 0,
            .arcount = 0,
        };
        char header[12];
        pack_header(_header, header);
        if (sendto(udpSocket, &header, sizeof(header), 0,
                   (struct sockaddr *)&client_addr,
                   sizeof(client_addr)) == -1) {
            perror("Failed to send response");
        }
    }
    close(udpSocket);
    return 0;
}
