/*******************************************************************************
 * @file    select_client.c
 * @brief   Client sử dụng select() để kết nối đến server
 * @date    2026-04-07 07:27
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/select.h>

int main() {
    int client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client == -1) {
        perror("socket() failed");
        return 1;
    }
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(8080);
    
    if (connect(client, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("connect() failed");
        close(client);
        return 1;
    }
    
    // Client is now connected to the server
    printf("Connected to server on port 8080...\n");
    
    fd_set fdread;
    FD_ZERO(&fdread);
    struct timeval tv;
    char buf[256];

    while (1) {
        // Reset tap fdread
        FD_SET(STDIN_FILENO, &fdread);
        FD_SET(client, &fdread);

        // Reset timeout
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        // Cho su kien
        int ret = select(client + 1, &fdread, NULL, NULL, &tv);
        if (ret < 0) {
            perror("select() failed");
            break;
        }

        if (ret == 0) {
            printf("Timeout occurred!\n");
            continue;
        }
        

        if (FD_ISSET(STDIN_FILENO, &fdread)) {
            // Su kien co du lieu tu ban phim
            fgets(buf, sizeof(buf), stdin);
            send(client, buf, strlen(buf), 0);
        }

        if (FD_ISSET(client, &fdread)) {
            // Su kien co du lieu tu server
            ret = recv(client, buf, sizeof(buf), 0);
            if (ret <= 0) {
                break;
            }
            buf[ret] = 0;
            printf("Received: %s\n", buf);
        }
    }

    close(client);
    return 0;
}