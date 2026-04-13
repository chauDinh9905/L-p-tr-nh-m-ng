#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>

#define PORT 8080
#define MAX_CLIENTS FD_SETSIZE

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        return 1;
    }
    
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))) {
        perror("setsockopt() failed");
        return 1;
    }
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);
    
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind() failed");
        return 1;
    }
    
    if (listen(listener, 5)) {
        perror("listen() failed");
        return 1;
    }
    
    printf("Chat Server is listening on port %d...\n", PORT);
    
    fd_set fdread, fdtest;
    FD_ZERO(&fdread);
    FD_SET(listener, &fdread);

    // Mảng lưu tên client, dùng để kiểm tra đã đăng nhập chưa
    char *client_names[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) client_names[i] = NULL;

    char buf[256];
    char send_buf[512];

    while (1) {
        fdtest = fdread;
        int ret = select(MAX_CLIENTS, &fdtest, NULL, NULL, NULL);

        if (ret < 0) break;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (FD_ISSET(i, &fdtest)) {
                if (i == listener) {
                    // Chấp nhận kết nối mới
                    int client = accept(listener, NULL, NULL);
                    printf("New connection: %d\n", client);
                    FD_SET(client, &fdread);
                    send(client, "Hay nhap ten theo cu phap 'client_id: client_name':\n", 53, 0);
                } else {
                    // Nhận dữ liệu từ client
                    ret = recv(i, buf, sizeof(buf) - 1, 0);
                    if (ret <= 0) {
                        printf("Client %d disconnected\n", i);
                        free(client_names[i]);
                        client_names[i] = NULL;
                        FD_CLR(i, &fdread);
                        close(i);
                    } else {
                        buf[ret] = 0;
                        // Xóa ký tự xuống dòng nếu có
                        if (buf[ret-1] == '\n') buf[ret-1] = 0;

                        if (client_names[i] == NULL) {
                            // CHƯA ĐĂNG NHẬP: Kiểm tra cú pháp "client_id: client_name"
                            char cmd[32], name[64];
                            int n = sscanf(buf, "%[^:]: %s", cmd, name);
                            
                            if (n == 2 && strcmp(cmd, "client_id") == 0) {
                                client_names[i] = strdup(name);
                                sprintf(send_buf, "Chao %s! Ban co the bat dau chat.\n", name);
                                send(i, send_buf, strlen(send_buf), 0);
                            } else {
                                char *msg = "Sai cu phap. Yeu cau 'client_id: client_name':\n";
                                send(i, msg, strlen(msg), 0);
                            }
                        } else {
                            // ĐÃ ĐĂNG NHẬP: Chuyển tiếp tin nhắn cho người khác
                            time_t rawtime;
                            struct tm *timeinfo;
                            time(&rawtime);
                            timeinfo = localtime(&rawtime);
                            char time_str[20];
                            strftime(time_str, sizeof(time_str), "%Y/%m/%d %I:%M:%p", timeinfo);

                            sprintf(send_buf, "%s %s: %s\n", time_str, client_names[i], buf);
                            
                            for (int j = 0; j < MAX_CLIENTS; j++) {
                                if (FD_ISSET(j, &fdread) && j != listener && j != i && client_names[j] != NULL) {
                                    send(j, send_buf, strlen(send_buf), 0);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}