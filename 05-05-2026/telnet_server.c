#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

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

// Xử lý zombie process khi tiến trình con kết thúc
void signal_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        return 1;
    }

    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

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

    printf("Multiprocessing Server listening on port %d...\n", PORT);

    // Đăng ký signal để dọn dẹp tiến trình con
    signal(SIGCHLD, signal_handler);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client = accept(listener, (struct sockaddr *)&client_addr, &client_len);
        
        if (client < 0) {
            perror("accept() failed");
            continue;
        }

        printf("New client connected: %d (PID: %d)\n", client, getpid());

        if (fork() == 0) {
            close(listener); // Con không cần socket lắng nghe

            char buf[256];
            int logged_in = 0;

            send(client, "Hay gui user pass de dang nhap:\n", 32, 0);

            while (1) {
                int len = recv(client, buf, sizeof(buf) - 1, 0);
                if (len <= 0) break;

                buf[len] = 0;
                buf[strcspn(buf, "\r\n")] = 0; // Xử lý xuống dòng

                if (!logged_in) {
                    char user[32], pass[32];
                    if (sscanf(buf, "%s %s", user, pass) == 2 && check_login(user, pass)) {
                        logged_in = 1;
                        send(client, "Dang nhap thanh cong. Nhap lenh:\n", 33, 0);
                    } else {
                        send(client, "Sai tai khoan. Nhap lai:\n", 25, 0);
                    }
                } else {
                    // Thực thi lệnh shell
                    printf("[PID %d] Executing: %s\n", getpid(), buf);
                    
                    char tmp_file[32];
                    sprintf(tmp_file, "out_%d.txt", getpid());
                    
                    char cmd[512];
                    sprintf(cmd, "%s > %s 2>&1", buf, tmp_file);
                    
                    system(cmd);

                    FILE *f = fopen(tmp_file, "r");
                    if (f) {
                        char file_buf[1024];
                        while (fgets(file_buf, sizeof(file_buf), f)) {
                            send(client, file_buf, strlen(file_buf), 0);
                        }
                        fclose(f);
                        remove(tmp_file); // Xóa file tạm sau khi dùng
                    }
                    send(client, "\nDone.\n", 7, 0);
                }
            }

            printf("Client on fd %d disconnected. Child process %d exiting.\n", client, getpid());
            close(client);
            exit(0); // Kết thúc tiến trình con
        }
        close(client); // Cha không dùng socket client này nữa
    }

    close(listener);
    return 0;
}