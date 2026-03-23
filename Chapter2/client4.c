/*******************************************************************************
 * @File: client2.c
 * @Date: 2026-03-17
 * @Description: Client truyền dữ liệu bất kỳ
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

struct sinhvien {
    int mssv;
    char hoten[64];
    char dob[15];
    double cpa;
};

int main() {
    int client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client < 0) {
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }

    char cmd[20], ip[20], port[10];
    scanf("%s %s %s", cmd, ip, port);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port = htons(atoi(port));

    if (connect(client, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("connect() failed");
        exit(EXIT_FAILURE);
    }

    struct sinhvien sv;

    while (1) {
        printf("Enter mssv: ");
        fflush(stdin); scanf("%d", &sv.mssv);
        getchar();
        printf("Enter hoten: ");
        fgets(sv.hoten, sizeof(sv.hoten), stdin);
        printf("Enter ngay sinh (VD: 09/09/2005): ");
        fflush(stdin); scanf("%s", sv.dob);
        printf("Enter CPA: ");
        fflush(stdin); scanf("%lf", &sv.cpa);

        send(client, &sv, sizeof(sv), 0);
        if (sv.cpa < 0)
            break;
    }

    close(client);

    return 0;
}