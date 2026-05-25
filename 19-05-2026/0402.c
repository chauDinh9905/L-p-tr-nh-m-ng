#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define DB_FILE "database.txt" // File chứa tài khoản hệ thống

void *client_handler(void *);
int recv_line(int socket, char *buffer, int max_len);
int check_login(const char *username, const char *password);

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
    addr.sin_port = htons(9999); // Chạy Telnet Server trên port 9999

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

    printf("Telnet Server (Multi-thread) is listening on port 9999...\n");

    while (1) {
        int *client_sock = malloc(sizeof(int));
        *client_sock = accept(listener, NULL, NULL);
        if (*client_sock < 0) {
            free(client_sock);
            continue;
        }

        printf("New client connected: %d\n", *client_sock);

        // Tạo luồng riêng biệt xử lý cho từng Client
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
    char username[256], password[256];
    int logged_in = 0;

    // --- GIAI ĐOẠN 1: XÁC THỰC TÀI KHOẢN ---
    while (!logged_in) {
        char welcome[] = "--- Welcome to Telnet Server ---\nUsername: ";
        send(sock, welcome, strlen(welcome), 0);
        
        // Nhận Username
        int len = recv_line(sock, username, sizeof(username));
        if (len <= 0) goto cleanup;

        char pass_prompt[] = "Password: ";
        send(sock, pass_prompt, strlen(pass_prompt), 0);

        // Nhận Password
        len = recv_line(sock, password, sizeof(password));
        if (len <= 0) goto cleanup;

        // Kiểm tra thông tin trong file cơ sở dữ liệu
        if (check_login(username, password)) {
            char success_msg[] = "Login successful! You can now send commands.\n$ ";
            send(sock, success_msg, strlen(success_msg), 0);
            logged_in = 1;
        } else {
            char fail_msg[] = "Login failed! Incorrect username or password.\n\n";
            send(sock, fail_msg, strlen(fail_msg), 0);
        }
    }

    // --- GIAI ĐOẠN 2: ĐỢI LỆNH VÀ THỰC THI LỆNH ---
    while (1) {
        int len = recv_line(sock, buf, sizeof(buf));
        if (len <= 0) break; // Client ngắt kết nối

        // Nếu client gõ "exit" -> Thoát phòng kết nối
        if (strcmp(buf, "exit") == 0) {
            char bye[] = "Goodbye!\n";
            send(sock, bye, strlen(bye), 0);
            break;
        }

        // Tạo chuỗi lệnh điều hướng kết quả ra file: "đoạn_lệnh > out_XXXX.txt"
        // Sử dụng ID socket trong tên file để tránh các luồng ghi đè lên file của nhau
        char cmd[1024];
        char filename[64];
        snprintf(filename, sizeof(filename), "out_%d.txt", sock);
        snprintf(cmd, sizeof(cmd), "%s > %s 2>&1", buf, filename); // Điều hướng cả lỗi (2>&1)

        // Thực thi lệnh hệ thống
        system(cmd);

        // Đọc nội dung file kết quả văn bản gửi trả lại cho client
        FILE *f = fopen(filename, "r");
        if (f) {
            char file_buf[1024];
            int bytes_read;
            while ((bytes_read = fread(file_buf, 1, sizeof(file_buf), f)) > 0) {
                send(sock, file_buf, bytes_read, 0);
            }
            fclose(f);
            unlink(filename); // Xóa file tạm sau khi đã gửi xong dữ liệu
        } else {
            char err[] = "Error executing command.\n";
            send(sock, err, strlen(err), 0);
        }

        // Gửi dấu nhắc lệnh tiếp theo
        send(sock, "\n$ ", 3, 0);
    }

cleanup:
    printf("Client %d disconnected.\n", sock);
    close(sock);
    return NULL;
}

// Hàm đọc dữ liệu từ socket đến khi gặp dấu xuống dòng, xóa sạch \r \n ở cuối
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

    // Chuẩn hóa xóa bỏ \r và \n ở cuối chuỗi
    while (total > 0 && (buffer[total - 1] == '\n' || buffer[total - 1] == '\r')) {
        buffer[total - 1] = '\0';
        total--;
    }
    return total;
}

// Hàm so khớp tài khoản trong file text dữ liệu
int check_login(const char *username, const char *password) {
    FILE *f = fopen(DB_FILE, "r");
    if (!f) {
        // Nếu chưa có file db, tự động tạo tài khoản mặc định để dễ test
        f = fopen(DB_FILE, "w+");
        if (f) {
            fprintf(f, "admin admin\nguest nopass\n");
            fseek(f, 0, SEEK_SET);
        } else {
            return 0;
        }
    }

    char file_user[256], file_pass[256];
    // Đọc từng dòng, mỗi dòng chứa cặp user + pass cách nhau bởi dấu cách
    while (fscanf(f, "%255s %255s", file_user, file_pass) == 2) {
        if (strcmp(username, file_user) == 0 && strcmp(password, file_pass) == 0) {
            fclose(f);
            return 1; // Khớp tài khoản thành công
        }
    }

    fclose(f);
    return 0; // Không tìm thấy tài khoản
}