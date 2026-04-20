/*******************************************************************************
 * @file    select_server_v2.c
 * @brief   Mô tả ngắn gọn về chức năng của file
 * @date    2026-04-07 08:43
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
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        return 1;
    }
    
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))) {
        perror("setsockopt() failed");
        close(listener);
        return 1;
    }
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);
    
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind() failed");
        close(listener);
        return 1;
    }
    
    if (listen(listener, 5)) {
        perror("listen() failed");
        close(listener);
        return 1;
    }
    
    // Server is now listening for incoming connections
    printf("Server is listening on port 8080...\n");
    
    fd_set fdread, fdtest;
    FD_ZERO(&fdread);
    FD_SET(listener, &fdread);

    struct timeval tv;
    char buf[256];

    while (1) {
        // Reset tap fdtest
        fdtest = fdread;

        // Reset timeout
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        int ret = select(FD_SETSIZE, &fdtest, NULL, NULL, &tv);
        if (ret < 0) {
            perror("select() failed");
            break;
        }
        if (ret == 0) {
            printf("Timed out.");
            continue;
        }

        for (int i = 0; i < FD_SETSIZE; i++) {
            if (FD_ISSET(i, &fdtest)) {
                if (i == listener) {
                    int client = accept(listener, NULL, NULL);
                    if (client < FD_SETSIZE) {
                        printf("New client connected %d\n", client);
                        FD_SET(client, &fdread);
                    } else {
                        close(client);
                    }
                } else {
                    ret = recv(i, buf, sizeof(buf), 0);
                    if (ret <= 0) {
                        printf("Client %d disconnected\n", i);    
                        FD_CLR(i, &fdread);
                    } else {
                        buf[ret] = 0;
                        printf("Received from %d: %s\n", i, buf);
                    }
                }            
            }
        }
    }

    close(listener);
    return 0;
}