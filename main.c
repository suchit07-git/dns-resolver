#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Struct for the DNS Header */
typedef struct {
    unsigned short id;
    unsigned char rd : 1;
    unsigned char tc : 1;
    unsigned char aa : 1;
    unsigned char opcode : 4;
    unsigned char qr : 1;
    unsigned char rcode : 4;
    unsigned char z : 3;
    unsigned char ra : 1;
    unsigned short qdcount;
    unsigned short ancount;
    unsigned short nscount;
    unsigned short arcount;
} DNSHeader;

/* Struct for the flags for the DNS Question */
typedef struct {
    unsigned short qtype;
    unsigned short qclass;
} DNSQuestionFlags;

/* Struct for the flags for the DNS RRs */
typedef struct {
    unsigned short type;
    unsigned short class;
    unsigned int ttl;
    unsigned short rdlength;
} DNSResourceRecordFlags;

#define BUFFER_SIZE 65536

void get_dns_servers(char *dns_servers[], size_t count) {
    FILE *resolv_file = fopen("/etc/resolv.conf", "r");
    if (!resolv_file) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    char line[100];
    size_t i = 0;

    while (fgets(line, sizeof(line), resolv_file) && i < count) {
        if (strncmp(line, "nameserver", 10) == 0) {
            char *token = strtok(line, " ");
            if (token) {
                token = strtok(NULL, "\n");
                if (token) {
                    strncpy(dns_servers[i], token, INET_ADDRSTRLEN - 1);
                    dns_servers[i][INET_ADDRSTRLEN - 1] = '\0';
                    ++i;
                }
            }
        }
    }

    fclose(resolv_file);
}

void domain_to_dns_format(const char *domain, unsigned char *dns) {
    int idx = 0, len = 0;
    strcat((char *)domain, ".");
    for (int i = 0; i < (int)strlen(domain); ++i) {
        if (domain[i] == '.') {
            dns[idx++] = i - len;
            memcpy(&dns[idx], &domain[len], i - len);
            idx += i - len;
            len = i + 1;
        }
    }
    dns[idx] = '\0';
}

void dns_format_to_domain(unsigned char *str) {
    int i = 0, j;
    unsigned char temp[256];
    int idx = 0;

    while (str[i] != '\0') {
        unsigned int len = str[i++];
        for (j = 0; j < len; ++j) {
            temp[idx++] = str[i++];
        }
        temp[idx++] = '.';
    }
    if (idx > 0) {
        temp[idx - 1] = '\0';
    }
    memcpy(str, temp, idx);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: resolver <hostname>\n");
        return EXIT_FAILURE;
    }

    DNSHeader *header = NULL;
    unsigned char *qname;
    DNSQuestionFlags *qflags = NULL;
    unsigned char name[10][254];
    DNSResourceRecordFlags *rrflags = NULL;
    unsigned char rdata[10][254];
    unsigned int type[10];
    unsigned char packet[BUFFER_SIZE];
    unsigned char *temp;
    int i, j, offset = 0;

    char *dns_servers[10];
    for (i = 0; i < 10; ++i) {
        dns_servers[i] = malloc(INET_ADDRSTRLEN);
    }
    get_dns_servers(dns_servers, 10);
    // for (int i = 0; i < 10; i++)
        // printf("%s\n", dns_servers[i]);

    header = (DNSHeader *)packet;
    header->id = htons(getpid());
    header->qr = 0;
    header->opcode = 0;
    header->aa = 0;
    header->tc = 0;
    header->rd = 1;
    header->ra = 0;
    header->z = 0;
    header->rcode = 0;
    header->qdcount = htons(1); 
    header->ancount = 0;
    header->nscount = 0;
    header->arcount = 0;

    offset = sizeof(DNSHeader);

    qname = packet + offset;
    domain_to_dns_format(argv[1], qname);
    offset += strlen((const char *)qname) + 1;

    qflags = (DNSQuestionFlags *)(packet + offset);
    qflags->qtype = htons(1);  
    qflags->qclass = htons(1); 
    offset += sizeof(DNSQuestionFlags);

    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(53);
    if (inet_pton(AF_INET, dns_servers[0], &servaddr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address/ Address not supported\n");
        exit(EXIT_FAILURE);
    }

    if (connect(sock_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    if (write(sock_fd, packet, offset) < 0) {
        perror("write");
        exit(EXIT_FAILURE);
    }

    ssize_t n = read(sock_fd, packet, BUFFER_SIZE);
    if (n <= 0) {
        perror("read");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    close(sock_fd);

    header = (DNSHeader *)packet;
    offset = sizeof(DNSHeader);

    qname = packet + offset;
    dns_format_to_domain(qname);
    offset += strlen((const char *)qname) + 2;

    qflags = (DNSQuestionFlags *)(packet + offset);
    offset += sizeof(DNSQuestionFlags);

    for (i = 0; i < ntohs(header->ancount); ++i) {
        temp = packet + offset - 1;
        j = 0;
        while (*temp != 0) {
            if (*temp == 0xc0) {
                ++temp;
                temp = packet + *temp;
            } else {
                name[i][j++] = *temp++;
            }
        }
        name[i][j] = '\0';
        dns_format_to_domain(name[i]);
        offset += 2;

        rrflags = (DNSResourceRecordFlags *)(packet + offset);
        offset += sizeof(DNSResourceRecordFlags) - 2;

        if (ntohs(rrflags->type) == 1) {
            memcpy(rdata[i], packet + offset, ntohs(rrflags->rdlength));
            type[i] = ntohs(rrflags->type);
        } else if (ntohs(rrflags->type) == 5) {
            temp = packet + offset;
            j = 0;
            while (*temp != 0) {
                if (*temp == 0xc0) {
                    ++temp;
                    temp = packet + *temp;
                } else {
                    rdata[i][j++] = *temp++;
                }
            }
            rdata[i][j] = '\0';
            dns_format_to_domain(rdata[i]);
            type[i] = ntohs(rrflags->type);
        }
        offset += ntohs(rrflags->rdlength);
    }

    printf("QNAME: %s\n", qname);
    printf("ANCOUNT: %d\n", ntohs(header->ancount));
    printf("\nRDATA:");
    for (i = 0; i < ntohs(header->ancount); ++i) {
        printf("\nNAME: %s\n", name[i]);
        if (type[i] == 5) {
            printf("CNAME: %s", rdata[i]);
        } else if (type[i] == 1) {
            printf("IPv4: ");
            for (j = 0; j < ntohs(rrflags->rdlength); ++j) {
                printf("%d.", rdata[i][j]);
            }
            printf("\b ");
        }
    }
    printf("\n");

    for (i = 0; i < 10; ++i) {
        free(dns_servers[i]);
    }
    return 0;
}
