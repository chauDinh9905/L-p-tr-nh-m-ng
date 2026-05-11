#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#define PORT 8080
#define NUM_PROCESSES 5

void handle_client(int client) {
    char buf[1024];
    int ret = recv(client, buf, sizeof(buf) - 1, 0);
    if (ret > 0) {
        buf[ret] = 0;
        printf("[Process %d] Request:\n%s\n", getpid(), buf);

        // Trả lời HTTP Response chuẩn
        char *msg = "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/html; charset=UTF-8\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "<html><body><h1>Xin chào các bạn!</h1>"
                    "<p>Được xử lý bởi PID: %d</p></body></html>";
        
        char response[2048];
        sprintf(response, msg, getpid());
        send(client, response, strlen(response), 0);
    }
    close(client);
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket failed");
        return 1;
    }

    // Cho phép tái sử dụng port nhanh chóng
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind failed");
        return 1;
    }

    if (listen(listener, 20)) {
        perror("listen failed");
        return 1;
    }

    printf("Preforking HTTP Server started on port %d với %d processes...\n", PORT, NUM_PROCESSES);


    for (int i = 0; i < NUM_PROCESSES; i++) {
        if (fork() == 0) {
            while (1) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                
                // Nhiều tiến trình cùng gọi accept trên 1 listener chung
                // OS sẽ đảm bảo chỉ 1 tiến trình nhận được kết nối
                int client = accept(listener, (struct sockaddr *)&client_addr, &client_len);
                
                if (client < 0) {
                    continue;
                }

                handle_client(client);
            }
            exit(0); // Không bao giờ chạm tới, nhưng tốt cho cấu trúc
        }
    }

    // Tiến trình cha chỉ việc đợi để dọn dẹp các con nếu cần
    // Hoặc giữ server sống
    while (1) {
        pause(); 
    }

    close(listener);
    return 0;
}