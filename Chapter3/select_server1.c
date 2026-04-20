/*******************************************************************************
 * @file    select_server_v1.c
 * @brief   Mô tả ngắn gọn về chức năng của file
 * @date    2026-04-07 07:58
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

#define MAX_CLIENTS 1020

int removeClient(int *clients, int *nClients, int i) {
    if (i < *nClients - 1) 
        clients[i] = clients[*nClients - 1];
    *nClients -= 1;
}

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
    
    int clients[MAX_CLIENTS];
    int nClients = 0; // tong so client

    fd_set fdread;
    char buf[256];
    struct timeval tv;

    while (1) {
        // Khoi tao tap fdread
        FD_ZERO(&fdread);
        FD_SET(listener, &fdread);
        int maxdp = listener + 1;
        for (int i = 0; i < nClients; i++) {
            FD_SET(clients[i], &fdread);
            if (maxdp < clients[i] + 1)
                maxdp = clients[i] + 1;
        }

        // Khoi tao timeout
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        // Goi ham select
        int ret = select(maxdp, &fdread, NULL, NULL, NULL);
        if (ret < 0) {
            perror("select() failed");
            break;
        }
        if (ret == 0) {
            printf("Timed out\n");
            continue;
        }

        if (FD_ISSET(listener, &fdread)) {
            int client = accept(listener, NULL, NULL);
            if (nClients < MAX_CLIENTS) {
                clients[nClients] = client;
                ++nClients;
                printf("New client connected: %d\n", client);
                char *msg = "Welcome\n";
                send(client, msg, strlen(msg), 0);
            } else {
                printf("Too many connections\n");
                char *msg = "Sorry. Out of slots.\n";
                send(client, msg, strlen(msg), 0);
                close(client);
            }
        }

        for (int i = 0; i < nClients; i++) {
            if (FD_ISSET(clients[i], &fdread)) {
                ret = recv(clients[i], buf, sizeof(buf), 0);
                if (ret <= 0) {
                    // Ket noi bi dong
                    printf("Client %d disconnected\n", clients[i]);
                    // Xoa phan tu i ra khoi mang clients
                    removeClient(clients, &nClients, i);
                    i--;
                    continue;
                }
                buf[ret] = 0;
                printf("Received from %d: %s", clients[i], buf);
            }
        }
    }

    close(listener);
    return 0;
}