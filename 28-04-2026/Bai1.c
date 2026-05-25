#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <ctype.h>

#define MAX_CLIENTS 1024
#define PORT 8080

void encrypt_string(char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] >= 'A' && str[i] <= 'Z') {
            if (str[i] == 'Z') str[i] = 'A';
            else str[i] = str[i] + 1;
        }
        else if (str[i] >= 'a' && str[i] <= 'z') {
            if (str[i] == 'z') str[i] = 'a';
            else str[i] = str[i] + 1;
        }
        else if (str[i] >= '0' && str[i] <= '9') {
            str[i] = '9' - (str[i] - '0');
        }
    }
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        return 1;
    }
    
    listen(listener, 5);
    printf("Server listening on port %d...\n", PORT);

    struct pollfd fds[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) fds[i].fd = -1;

    fds[0].fd = listener;
    fds[0].events = POLLIN;
    int nfds = 1;

    char buf[1024];

    while (1) {
        int ret = poll(fds, nfds, -1);
        if (ret < 0) break;

        for (int i = 0; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == listener) {
                    int client = accept(listener, NULL, NULL);
                    if (nfds < MAX_CLIENTS) {
                        fds[nfds].fd = client;
                        fds[nfds].events = POLLIN;
                        nfds++;

                        char welcome[100];
                        sprintf(welcome, "Xin chào. Hiện có %d clients đang kết nối.\n", nfds - 1);
                        send(client, welcome, strlen(welcome), 0);
                        printf("Client ID %d connected. Total: %d\n", client, nfds - 1);
                    } else {
                        close(client);
                    }
                } else {
                    ret = recv(fds[i].fd, buf, sizeof(buf) - 1, 0);
                    if (ret <= 0) {
                        goto disconnect;
                    }

                    buf[ret] = 0;
                    buf[strcspn(buf, "\r\n")] = 0;
                    if (strcmp(buf, "exit") == 0) {
                        send(fds[i].fd, "Tạm biệt!\n", 12, 0);
                        goto disconnect;
                    }

                    // 4. MÃ HÓA VÀ TRẢ KẾT QUẢ
                    encrypt_string(buf);
                    strcat(buf, "\n");
                    send(fds[i].fd, buf, strlen(buf), 0);
                    continue;

                disconnect:
                    printf("Client on fd %d disconnected\n", fds[i].fd);
                    close(fds[i].fd);
                    fds[i] = fds[nfds - 1];
                    nfds--;
                    i--;
                }
            }
        }
    }

    close(listener);
    return 0;
}