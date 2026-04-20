#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h> // Thay thế select.h

#define MAX_CLIENTS 1024
#define PORT 8080

// Hàm kiểm tra đăng nhập từ file
int check_login(char *user, char *pass) {
    FILE *f = fopen("database.txt", "r");
    if (f == NULL) return 0;

    char f_user[32], f_pass[32];
    while (fscanf(f, "%s %s", f_user, f_pass) != EOF) {
        if (strcmp(user, f_user) == 0 && strcmp(pass, f_pass) == 0) {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    // Cấu hình SO_REUSEADDR để khởi động lại server nhanh hơn
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

    // Mảng cấu trúc pollfd để quản lý các socket
    struct pollfd fds[MAX_CLIENTS];
    // Mảng lưu trạng thái đăng nhập tương ứng với từng index trong fds
    int logged_in[MAX_CLIENTS];

    // Khởi tạo mảng
    for (int i = 0; i < MAX_CLIENTS; i++) {
        fds[i].fd = -1;
        logged_in[i] = 0;
    }

    // Gán socket listener vào vị trí đầu tiên
    fds[0].fd = listener;
    fds[0].events = POLLIN;
    int nfds = 1;

    char buf[256], tmp_file[32];

    while (1) {
        // Chờ sự kiện, timeout = -1 (chờ vô hạn)
        int ret = poll(fds, nfds, -1);
        if (ret < 0) {
            perror("poll failed");
            break;
        }

        int current_size = nfds;
        for (int i = 0; i < current_size; i++) {
            // Kiểm tra xem socket i có sự kiện POLLIN (dữ liệu đến) không
            if (fds[i].revents & POLLIN) {
                
                if (fds[i].fd == listener) {
                    // KẾT NỐI MỚI
                    int client = accept(listener, NULL, NULL);
                    if (nfds < MAX_CLIENTS) {
                        fds[nfds].fd = client;
                        fds[nfds].events = POLLIN;
                        logged_in[nfds] = 0; // Reset trạng thái login cho client mới
                        nfds++;
                        send(client, "Hay gui user pass de dang nhap:\n", 32, 0);
                        printf("New client connected on fd %d\n", client);
                    } else {
                        send(client, "Server full\n", 12, 0);
                        close(client);
                    }
                } else {
                    // NHẬN DỮ LIỆU TỪ CLIENT
                    ret = recv(fds[i].fd, buf, sizeof(buf) - 1, 0);
                    
                    if (ret <= 0) {
                        // CLIENT NGẮT KẾT NỐI
                        printf("Client on fd %d disconnected\n", fds[i].fd);
                        close(fds[i].fd);
                        
                        // Thu gọn mảng: Đổi phần tử cuối lên vị trí i
                        fds[i] = fds[nfds - 1];
                        logged_in[i] = logged_in[nfds - 1];
                        nfds--;
                        i--; // Kiểm tra lại index này với phần tử mới chuyển đến
                    } else {
                        buf[ret] = 0;
                        // Xóa các ký tự xuống dòng (\n hoặc \r)
                        buf[strcspn(buf, "\r\n")] = 0;

                        if (logged_in[i] == 0) {
                            // XỬ LÝ ĐĂNG NHẬP
                            char user[32], pass[32];
                            if (sscanf(buf, "%s %s", user, pass) == 2 && check_login(user, pass)) {
                                logged_in[i] = 1;
                                send(fds[i].fd, "Dang nhap thanh cong. Nhap lenh:\n", 33, 0);
                            } else {
                                send(fds[i].fd, "Sai tai khoan. Nhap lai:\n", 25, 0);
                            }
                        } else {
                            // THỰC THI LỆNH
                            printf("Executing command for fd %d: %s\n", fds[i].fd, buf);
                            sprintf(tmp_file, "out_%d.txt", fds[i].fd);
                            char cmd[512];
                            sprintf(cmd, "%s > %s 2>&1", buf, tmp_file); // Gộp cả lỗi stderr vào file
                            system(cmd);

                            FILE *f = fopen(tmp_file, "r");
                            if (f) {
                                char file_buf[1024];
                                while (fgets(file_buf, sizeof(file_buf), f)) {
                                    send(fds[i].fd, file_buf, strlen(file_buf), 0);
                                }
                                fclose(f);
                                //remove(tmp_file); // Xóa file sau khi gửi xong
                            }
                            send(fds[i].fd, "\nDone.\n", 7, 0);
                        }
                    }
                }
            }
        }
    }

    close(listener);
    return 0;
}