#include <stdio.h>
#include<stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>

int main() {
    char port[16], ip[20], cmd [20];
    scanf("%s %s %s", cmd, ip, port);
    int client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port = htons(atoi(port));
    int ret = connect(client, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        perror("connect() failed");
    }
    char msg[] = "Hello server";
    send(client, msg, strlen(msg), 0);
    close(client);
}