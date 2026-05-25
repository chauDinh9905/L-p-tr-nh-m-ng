#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>

#define STORAGE_DIR "." // Thư mục cấu hình trên server (mặc định là thư mục hiện tại)

void *client_thread(void *);
int recv_line(int socket, char *buffer, int max_len);

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
    
    printf("Server is listening on port 8080...\n");
    
    while (1) {
        // Cấp phát động để tránh lỗi Race Condition khi nhiều client kết nối cùng lúc
        int *client = malloc(sizeof(int));
        *client = accept(listener, NULL, NULL);
        if (*client < 0) {
            free(client);
            continue;
        }
        printf("New client connected: %d\n", *client);

        pthread_t thread_id;
        pthread_create(&thread_id, NULL, client_thread, client);
        pthread_detach(thread_id);
    }

    close(listener);
    return 0;
}

void *client_thread(void *params) {
    int client = *(int *)params;
    free(params); // Giải phóng con trỏ int đã cấp phát ở hàm main

    DIR *d;
    struct dirent *dir;
    char *file_list[100];
    int file_count = 0;

    // 1. Quét thư mục được thiết lập trên server để lấy danh sách file thông thường
    d = opendir(STORAGE_DIR);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
                continue;
            
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s", STORAGE_DIR, dir->d_name);
            struct stat st;
            // Chỉ lấy file thông thường (Regular file), bỏ qua thư mục con nếu có
            if (stat(full_path, &st) == 0 && S_ISREG(st.st_mode)) {
                file_list[file_count] = strdup(dir->d_name);
                file_count++;
                if (file_count >= 100) break; 
            }
        }
        closedir(d);
    }

    // Yêu cầu 1 & 2: Gửi danh sách file hoặc báo lỗi nếu không có file
    if (file_count == 0) {
        // Nếu không có file nào -> Gửi lỗi và ĐÓNG KẾT NỐI luôn
        char err_msg[] = "ERROR No files to download\r\n";
        send(client, err_msg, strlen(err_msg), 0);
        close(client);
        return NULL;
    } else {
        char response[256];
        // Dòng đầu: OK N\r\n
        snprintf(response, sizeof(response), "OK %d\r\n", file_count);
        send(client, response, strlen(response), 0);

        // Các tên file phân cách bởi \r\n
        for (int i = 0; i < file_count; i++) {
            snprintf(response, sizeof(response), "%s\r\n", file_list[i]);
            send(client, response, strlen(response), 0);
        }
        // Kết thúc danh sách bởi \r\n\r\n (ở đây send thêm \r\n nối tiếp vào \r\n của file cuối)
        send(client, "\r\n", 2, 0);
    }

    // Giải phóng bộ nhớ mảng tạm lưu tên file
    for (int i = 0; i < file_count; i++) {
        free(file_list[i]);
    }

    // Yêu cầu 3: Nhận tên file từ client và xử lý tải file / báo lỗi
    char buf[512];
    while (1) {
        // Đọc tên file do client gửi lên (đọc đến khi gặp dòng mới)
        int len = recv_line(client, buf, sizeof(buf));
        if (len <= 0) break; // Client ngắt kết nối đột ngột

        // Xóa sạch ký tự \r và \n ở cuối chuỗi nhận được để lấy chuẩn tên file
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
            buf[len - 1] = '\0';
            len--;
        }
        if (len == 0) continue;

        // Kiểm tra file có tồn tại hay không
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", STORAGE_DIR, buf);
        struct stat st;

        if (stat(full_path, &st) == 0 && S_ISREG(st.st_mode)) {
            // Trường hợp 3a: File tồn tại -> Gửi "OK N\r\n" (N là kích thước) + Nội dung file + ĐÓNG KẾT NỐI
            long file_size = st.st_size;
            char ok_msg[128];
            snprintf(ok_msg, sizeof(ok_msg), "OK %ld\r\n", file_size);
            send(client, ok_msg, strlen(ok_msg), 0);

            FILE *f = fopen(full_path, "rb");
            if (f) {
                char file_buf[1024];
                int bytes_read;
                while ((bytes_read = fread(file_buf, 1, sizeof(file_buf), f)) > 0) {
                    send(client, file_buf, bytes_read, 0);
                }
                fclose(f);
            }
            break; // Thoát vòng lặp để đóng kết nối theo đúng yêu cầu đề bài
        } else {
            // Trường hợp 3b: File KHÔNG tồn tại -> Gửi thông báo lỗi và TIẾP TỤC đợi client gửi lại tên file
            char err_msg[] = "ERROR File not found. Please try again!\r\n";
            send(client, err_msg, strlen(err_msg), 0);
        }
    }
    
    close(client);
    printf("Closed connection with client.\n");
    return NULL;
}

// Hàm bổ trợ đọc từng ký tự từ Socket cho đến khi gặp dấu xuống dòng \n (Tránh lỗi dính gói TCP)
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
    return total;
}