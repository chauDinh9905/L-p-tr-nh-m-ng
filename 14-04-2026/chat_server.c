#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <poll.h> // Sử dụng poll thay vì select

#define PORT 8080
#define MAX_CLIENTS 1024 // Có thể tăng con số này lên dễ dàng với poll

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
    
    printf("Chat Server (Poll) is listening on port %d...\n", PORT);
    
    // Mảng cấu trúc pollfd để quản lý các socket
    struct pollfd fds[MAX_CLIENTS];
    // Mảng lưu tên client tương ứng với từng phần tử trong fds
    char *client_names[MAX_CLIENTS];

    // Khởi tạo các phần tử
    for (int i = 0; i < MAX_CLIENTS; i++) {
        fds[i].fd = -1; 
        client_names[i] = NULL;
    }

    // Gán listener vào phần tử đầu tiên
    fds[0].fd = listener;
    fds[0].events = POLLIN;
    int nfds = 1; // Số lượng descriptor hiện tại đang theo dõi

    char buf[256];
    char send_buf[512];

    while (1) {
        // Chờ đợi sự kiện (timeout = -1 nghĩa là chờ vô hạn)
        int ret = poll(fds, nfds, -1);

        if (ret < 0) {
            perror("poll() failed");
            break;
        }

        int current_size = nfds;
        for (int i = 0; i < current_size; i++) {
            // Kiểm tra xem socket này có dữ liệu để đọc hay không
            if (fds[i].revents & POLLIN) {
                
                if (fds[i].fd == listener) {
                    // Chấp nhận kết nối mới
                    int client = accept(listener, NULL, NULL);
                    printf("New connection: %d\n", client);
                    
                    if (nfds < MAX_CLIENTS) {
                        fds[nfds].fd = client;
                        fds[nfds].events = POLLIN;
                        client_names[nfds] = NULL;
                        nfds++;
                        send(client, "Hay nhap ten theo cu phap 'client_id: client_name':\n", 53, 0);
                    } else {
                        printf("Server full!\n");
                        close(client);
                    }
                } else {
                    // Nhận dữ liệu từ client hiện hữu
                    ret = recv(fds[i].fd, buf, sizeof(buf) - 1, 0);
                    if (ret <= 0) {
                        printf("Client on fd %d disconnected\n", fds[i].fd);
                        close(fds[i].fd);
                        free(client_names[i]);
                        
                        // Thu dọn mảng bằng cách đưa phần tử cuối cùng vào vị trí vừa trống
                        fds[i] = fds[nfds - 1];
                        client_names[i] = client_names[nfds - 1];
                        nfds--;
                        i--; // Kiểm tra lại vị trí này với dữ liệu mới vừa đổi sang
                    } else {
                        buf[ret] = 0;
                        if (buf[ret-1] == '\n') buf[ret-1] = 0;
                        if (buf[ret-1] == '\r') buf[ret-1] = 0; // Xử lý cả \r\n

                        if (client_names[i] == NULL) {
                            char cmd[32], name[64];
                            int n = sscanf(buf, "%[^:]: %s", cmd, name);
                            
                            if (n == 2 && strcmp(cmd, "client_id") == 0) {
                                client_names[i] = strdup(name);
                                sprintf(send_buf, "Chao %s! Ban co the bat dau chat.\n", name);
                                send(fds[i].fd, send_buf, strlen(send_buf), 0);
                            } else {
                                char *msg = "Sai cu phap. Yeu cau 'client_id: client_name':\n";
                                send(fds[i].fd, msg, strlen(msg), 0);
                            }
                        } else {
                            // Gửi tin nhắn cho mọi người (trừ listener và chính mình)
                            time_t rawtime;
                            struct tm *timeinfo;
                            time(&rawtime);
                            timeinfo = localtime(&rawtime);
                            char time_str[20];
                            strftime(time_str, sizeof(time_str), "%Y/%m/%d %I:%M:%p", timeinfo);

                            sprintf(send_buf, "%s %s: %s\n", time_str, client_names[i], buf);
                            
                            for (int j = 1; j < nfds; j++) {
                                if (j != i && client_names[j] != NULL) {
                                    send(fds[j].fd, send_buf, strlen(send_buf), 0);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Cleanup
    for (int i = 0; i < nfds; i++) {
        if (fds[i].fd != -1) close(fds[i].fd);
        free(client_names[i]);
    }

    return 0;
}