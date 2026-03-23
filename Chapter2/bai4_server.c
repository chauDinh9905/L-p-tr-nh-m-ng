/*******************************************************************************
 * @File: server4.c
 * @Date: 2026-03-17
 * @Description: Server nhận dữ liệu bất kỳ
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
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    char cmd[20], port[20], content_from_client[20];
    scanf("%s %s %s", cmd, port, content_from_client);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(atoi(port));

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind() failed");
        exit(EXIT_FAILURE);
    }

    listen(listener, 5);

    int client = accept(listener, NULL, NULL);
    if (client < 0) {
        perror("accept() failed");
        exit(EXIT_FAILURE);
    }

    char buffer[150];
    FILE *f = fopen(content_from_client, "a");
    while (1) {
        int ret = recv(client, buffer, sizeof(buffer), 0);
        //int res = recv(client, thoigian, sizeof(thoigian), 0);
        if (ret <= 0)
            break;
        buffer[ret] = '\0';
        printf("Received: %s\n", buffer);
        fprintf(f, "%s\n", buffer);
        fflush(f);
    }
    fclose(f);
    close(client);
    close(listener);
    return 0;
}