#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>

#define PORT 8080

// Hàm xử lý zombie process
void signal_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Hàm lấy thời gian hệ thống theo định dạng
void get_formatted_time(char *format, char *output) {
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    if (strcmp(format, "dd/mm/yyyy") == 0) {
        strftime(output, 64, "%d/%m/%Y", timeinfo);
    } else if (strcmp(format, "dd/mm/yy") == 0) {
        strftime(output, 64, "%d/%m/%y", timeinfo);
    } else if (strcmp(format, "mm/dd/yyyy") == 0) {
        strftime(output, 64, "%m/%d/%Y", timeinfo);
    } else if (strcmp(format, "mm/dd/yy") == 0) {
        strftime(output, 64, "%m/%d/%y", timeinfo);
    } else {
        strcpy(output, "INVALID_FORMAT");
    }
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind() failed");
        return 1;
    }

    listen(listener, 10);
    signal(SIGCHLD, signal_handler);

    printf("Time Server is listening on port %d...\n", PORT);

    while (1) {
        int client = accept(listener, NULL, NULL);
        if (client < 0) continue;

        if (fork() == 0) {
            // TIẾN TRÌNH CON
            close(listener);
            char buf[256];
            
            while (1) {
                int len = recv(client, buf, sizeof(buf) - 1, 0);
                if (len <= 0) break;

                buf[len] = 0;
                // Xóa ký tự xuống dòng
                buf[strcspn(buf, "\r\n")] = 0;

                char cmd[16], format[32];
                // Phân tách lệnh: yêu cầu 2 tham số là "GET_TIME" và "[format]"
                int n = sscanf(buf, "%s %s", cmd, format);

                if (n == 2 && strcmp(cmd, "GET_TIME") == 0) {
                    char time_str[64];
                    get_formatted_time(format, time_str);
                    
                    if (strcmp(time_str, "INVALID_FORMAT") == 0) {
                        char *err = "Loi: Dinh dang khong hop le.\n";
                        send(client, err, strlen(err), 0);
                    } else {
                        strcat(time_str, "\n");
                        send(client, time_str, strlen(time_str), 0);
                    }
                } else {
                    char *err = "Loi: Sai cu phap. Dung: GET_TIME [format]\n";
                    send(client, err, strlen(err), 0);
                }
            }
            printf("Client %d disconnected.\n", client);
            close(client);
            exit(0);
        }
        // TIẾN TRÌNH CHA
        close(client);
    }

    return 0;
}