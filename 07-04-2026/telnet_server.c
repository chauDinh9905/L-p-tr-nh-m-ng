#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>

#define MAX_CLIENTS FD_SETSIZE

int check_login(char *user, char *pass) {
    FILE *f = fopen("databases.txt", "r");
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
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);

    bind(listener, (struct sockaddr *)&addr, sizeof(addr));
    listen(listener, 5);

    fd_set fdread, fdtest;
    FD_ZERO(&fdread);
    FD_SET(listener, &fdread);

    int logged_in[MAX_CLIENTS] = {0}; // Trạng thái đăng nhập của từng socket
    char buf[256], tmp_file[32];

    while (1) {
        fdtest = fdread;
        select(MAX_CLIENTS, &fdtest, NULL, NULL, NULL);

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (FD_ISSET(i, &fdtest)) {
                if (i == listener) {
                    int client = accept(listener, NULL, NULL);
                    FD_SET(client, &fdread);
                    send(client, "Hay gui user pass de dang nhap:\n", 32, 0);
                } else {
                    int ret = recv(i, buf, sizeof(buf) - 1, 0);
                    if (ret <= 0) {
                        FD_CLR(i, &fdread);
                        close(i);
                        logged_in[i] = 0;
                    } else {
                        buf[ret] = 0;
                        if (buf[ret-1] == '\n') buf[ret-1] = 0; // Xử lý dấu xuống dòng

                        if (logged_in[i] == 0) {
                            // XỬ LÝ ĐĂNG NHẬP
                            char user[32], pass[32];
                            if (sscanf(buf, "%s %s", user, pass) == 2 && check_login(user, pass)) {
                                logged_in[i] = 1;
                                send(i, "Dang nhap thanh cong. Nhap lenh:\n", 33, 0);
                            } else {
                                send(i, "Sai tai khoan. Nhap lai:\n", 25, 0);
                            }
                        } else {
                            // THỰC THI LỆNH
                            sprintf(tmp_file, "out_%d.txt", i);
                            char cmd[512];
                            // Tạo lệnh: "dir > out_i.txt" hoặc "ls > out_i.txt"
                            sprintf(cmd, "%s > %s", buf, tmp_file);
                            system(cmd);

                            // Đọc file kết quả gửi trả client
                            FILE *f = fopen(tmp_file, "r");
                            if (f) {
                                char file_buf[1024];
                                while (fgets(file_buf, sizeof(file_buf), f)) {
                                    send(i, file_buf, strlen(file_buf), 0);
                                }
                                fclose(f);
                            }
                            send(i, "\nDone.\n", 7, 0);
                        }
                    }
                }
            }
        }
    }
    return 0;
}