#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

void *client_handler(void *);
int recv_line(int socket, char *buffer, int max_len);
void process_time_command(const char *cmd, char *response, size_t max_len);

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
    addr.sin_port = htons(8888); // Chạy Time Server trên port 8888

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

    printf("Time Server (Multi-thread) is listening on port 8888...\n");

    while (1) {
        int *client_sock = malloc(sizeof(int));
        *client_sock = accept(listener, NULL, NULL);
        if (*client_sock < 0) {
            free(client_sock);
            continue;
        }

        printf("New client connected: %d\n", *client_sock);

        // Tạo luồng độc lập xử lý riêng cho từng Client kết nối vào
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, client_handler, client_sock) == 0) {
            pthread_detach(thread_id);
        } else {
            close(*client_sock);
            free(client_sock);
        }
    }

    close(listener);
    return 0;
}

void *client_handler(void *arg) {
    int sock = *(int *)arg;
    free(arg);

    char buf[512];
    char response[256];

    char welcome[] = "--- Welcome to Time Server ---\nEnter command (e.g., GET_TIME dd/mm/yyyy):\n";
    send(sock, welcome, strlen(welcome), 0);

    while (1) {
        // Nhận lệnh từ client (đọc cho tới khi gặp ký tự dòng mới \n)
        int len = recv_line(sock, buf, sizeof(buf));
        if (len <= 0) break; // Client ngắt kết nối

        // Nếu client muốn thoát
        if (strcmp(buf, "exit") == 0 || strcmp(buf, "quit") == 0) {
            char bye[] = "Goodbye!\n";
            send(sock, bye, strlen(bye), 0);
            break;
        }

        // Xử lý và kiểm tra tính đúng đắn của lệnh
        process_time_command(buf, response, sizeof(response));

        // Trả kết quả về cho client
        send(sock, response, strlen(response), 0);
    }

    printf("Client %d disconnected.\n", sock);
    close(sock);
    return NULL;
}

// Hàm phân tích cú pháp lệnh và trả về chuỗi thời gian tương ứng
void process_time_command(const char *cmd, char *response, size_t max_len) {
    // Kiểm tra tiền tố lệnh xem có đúng bắt đầu bằng "GET_TIME" không
    if (strncmp(cmd, "GET_TIME", 8) != 0) {
        snprintf(response, max_len, "ERROR Invalid command syntax. Use GET_TIME [format]\n");
        return;
    }

    // Lấy phần định dạng phía sau chữ "GET_TIME "
    const char *format_part = cmd + 8;
    
    // Bỏ qua các dấu khoảng trắng nếu client gõ dư (ví dụ: GET_TIME    dd/mm/yyyy)
    while (*format_part == ' ') {
        format_part++;
    }

    // Lấy thời gian hiện tại của hệ thống
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    // Kiểm tra từng cấu trúc format được yêu cầu trong đề bài
    if (strcmp(format_part, "dd/mm/yyyy") == 0) {
        strftime(response, max_len, "%d/%m/%Y\n", timeinfo);
    } 
    else if (strcmp(format_part, "dd/mm/yy") == 0) {
        strftime(response, max_len, "%d/%m/%y\n", timeinfo);
    } 
    else if (strcmp(format_part, "mm/dd/yyyy") == 0) {
        strftime(response, max_len, "%m/%d/%Y\n", timeinfo);
    } 
    else if (strcmp(format_part, "mm/dd/yy") == 0) {
        strftime(response, max_len, "%m/%d/%y\n", timeinfo);
    } 
    // Nếu client chỉ gõ "GET_TIME" không kèm format -> Mặc định trả về dd/mm/yyyy theo thói quen
    else if (strlen(format_part) == 0) {
        strftime(response, max_len, "%d/%m/%Y\n", timeinfo);
    } 
    // Trường hợp gõ sai định dạng hệ thống hỗ trợ
    else {
        snprintf(response, max_len, "ERROR Unsupported format. Allowed: dd/mm/yyyy, dd/mm/yy, mm/dd/yyyy, mm/dd/yy\n");
    }
}

// Hàm bổ trợ đọc dữ liệu từ socket đến khi gặp dấu \n và bóc tách sạch ký tự điều khiển \r \n ở cuối
int recv_line(int socket, char *buffer, int max_len) {
    int total = 0;
    char c;
    while (total < max_len - 1) {
        int n = recv(socket, &c, 1, 0);
        if (n <= 0) return n;
        buffer[total++] = c;
        if (c == '\n') break;
    }
    buffer[total] = '\0';

    // Xóa sạch \r và \n thừa ở cuối chuỗi thu được
    while (total > 0 && (buffer[total - 1] == '\n' || buffer[total - 1] == '\r')) {
        buffer[total - 1] = '\0';
        total--;
    }
    return total;
}