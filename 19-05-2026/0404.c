#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

void *client_handler(void *arg);

int main() {
    // 1. Tạo socket lắng nghe
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        return 1;
    }
    
    // Tái sử dụng địa chỉ/cổng tránh lỗi "Address already in use" khi khởi động lại
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))) {
        perror("setsockopt() failed");
        close(listener);
        return 1;
    }
    
    // 2. Cấu hình địa chỉ Server (Listen ở port 8080)
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);
    
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind() failed");
        close(listener);
        return 1;
    }
    
    if (listen(listener, 10)) {
        perror("listen() failed");
        close(listener);
        return 1;
    }
    
    printf("HTTP Server is listening on port 8080...\n");
    printf("Open browser and access: http://127.0.0.1:8080\n");
    
    // 3. Vòng lặp vô hạn chấp nhận các kết nối mới
    while (1) {
        // Cấp phát động bộ nhớ cho biến socket của client để tránh lỗi Race Condition (Dính luồng)
        int *pclient = malloc(sizeof(int));
        if (pclient == NULL) {
            perror("malloc() failed");
            continue;
        }

        // Chờ kết nối mới
        *pclient = accept(listener, NULL, NULL);
        if (*pclient < 0) {
            free(pclient);
            continue;
        }
        
        printf("New client connected: %d\n", *pclient);
        
        // Tạo luồng mới để xử lý riêng biệt cho client này
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, client_handler, pclient) == 0) {
            // Giải phóng tài nguyên tự động của luồng khi hàm client_handler kết thúc
            pthread_detach(thread_id);
        } else {
            perror("pthread_create() failed");
            close(*pclient);
            free(pclient);
        }
    }

    close(listener);
    return 0;
}

// Hàm xử lý độc lập cho từng client (Hàm luồng)
void *client_handler(void *arg) {
    int client = *(int *)arg;
    free(arg); // Giải phóng vùng nhớ int đã cấp phát động ở vòng lặp main

    char buf[2048]; // Tăng kích thước buffer lên một chút vì HTTP Request thường khá dài
    
    // Nhận dữ liệu (HTTP Request) từ client
    int ret = recv(client, buf, sizeof(buf) - 1, 0);
    if (ret > 0) {
        buf[ret] = '\0'; // Đảm bảo chuỗi kết thúc bằng ký tự null
        printf(" Received Request from client %d \n", client);
        puts(buf);
        printf("\n");
        
        // Trả lại kết quả (HTTP Response) cho client theo chuẩn giao thức Web
        char *msg = "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/html; charset=UTF-8\r\n"
                    "Content-Length: 17\r\n"
                    "\r\n"
                    "Xin chao cac ban\n";
                    
        send(client, msg, strlen(msg), 0);
    }
    
    // Đóng kết nối sau khi hoàn thành chu trình Request - Response
    printf("Closing connection for client: %d\n", client);
    close(client);
    
    return NULL;
}