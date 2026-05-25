#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#define MAX_CLIENTS 100

// Cấu trúc quản lý thông tin từng Client
typedef struct {
    int socket;
    char id[64];
    int is_registered; // 0: Chưa đăng ký tên đúng cú pháp, 1: Đã đăng ký thành công
} ClientInfo;

ClientInfo clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void *client_handler(void *arg);
void broadcast_message(const char *message, int sender_socket);
void get_current_time_string(char *buffer, size_t max_len);

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
    addr.sin_port = htons(8888);

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

    // Khởi tạo danh sách các socket trống
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = -1;
        clients[i].is_registered = 0;
        memset(clients[i].id, 0, sizeof(clients[i].id));
    }

    printf("Chat Server (Multi-thread) is listening on port 8888...\n");

    while (1) {
        int client_sock = accept(listener, NULL, NULL);
        if (client_sock < 0) continue;

        pthread_mutex_lock(&clients_mutex);
        int added = 0;
        // Tìm vị trí trống trong mảng toàn cục để lưu client mới vào
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket == -1) {
                clients[i].socket = client_sock;
                clients[i].is_registered = 0;
                
                int *pclient = malloc(sizeof(int));
                *pclient = client_sock;

                pthread_t thread_id;
                if (pthread_create(&thread_id, NULL, client_handler, pclient) == 0) {
                    pthread_detach(thread_id);
                    added = 1;
                } else {
                    free(pclient);
                    clients[i].socket = -1;
                }
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);

        if (!added) {
            char full_msg[] = "Server full. Try again later.\n";
            send(client_sock, full_msg, strlen(full_msg), 0);
            close(client_sock);
        }
    }

    close(listener);
    return 0;
}

void *client_handler(void *arg) {
    int sock = *(int *)arg;
    free(arg);

    char buf[2048];
    int my_index = -1;

    // Xác định vị trí của bản thân luồng này trong mảng cấu trúc toàn cục
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == sock) {
            my_index = i;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    if (my_index == -1) {
        close(sock);
        return NULL;
    }

    // Giai đoạn 1: Ép buộc client gửi đúng cú pháp "client_id: client_name"
    char req_msg[] = "Please register using format -> client_id: client_name\n";
    send(sock, req_msg, strlen(req_msg), 0);

    while (1) {
        int len = recv(sock, buf, sizeof(buf) - 1, 0);
        if (len <= 0) goto cleanup;

        buf[len] = '\0';
        // Chuẩn hóa xóa dấu xuống dòng dư thừa từ nc hay telnet (\r hoặc \n)
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
            buf[len - 1] = '\0';
            len--;
        }

        if (len == 0) continue;

        // Kiểm tra xem chuỗi có chứa từ khóa "client_id: " hay không
        char *prefix = strstr(buf, "client_id: ");
        if (prefix == buf) { // Cú pháp phải bắt đầu ngay từ đầu chuỗi
            char *name_part = buf + 11; // Nhảy qua chuỗi "client_id: " (11 ký tự)
            
            // Xóa khoảng trắng thừa nếu người dùng nhập kiểu "client_id:  name"
            while (*name_part == ' ') name_part++;

            if (strlen(name_part) > 0 && strchr(name_part, ' ') == NULL) {
                // Đăng ký định danh thành công (xâu viết liền không chứa dấu cách)
                pthread_mutex_lock(&clients_mutex);
                strncpy(clients[my_index].id, name_part, sizeof(clients[my_index].id) - 1);
                clients[my_index].is_registered = 1;
                pthread_mutex_unlock(&clients_mutex);

                char welcome[128];
                snprintf(welcome, sizeof(welcome), "Welcome [%s]! You joined the chat room.\n", name_part);
                send(sock, welcome, strlen(welcome), 0);
                break; // Thoát vòng lặp đăng ký tên, chuyển sang chế độ chat phòng chung
            }
        }

        // Nếu sai cú pháp hoặc tên chứa khoảng trắng, bắt nhập lại
        char err_msg[] = "Invalid syntax or name contains space! Try again: ";
        send(sock, err_msg, strlen(err_msg), 0);
    }

    // Giai đoạn 2: Nhận dữ liệu chat từ client và chuyển tiếp (Broadcast) kèm mốc thời gian
    while (1) {
        int len = recv(sock, buf, sizeof(buf) - 1, 0);
        if (len <= 0) break;

        buf[len] = '\0';
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
            buf[len - 1] = '\0';
            len--;
        }

        if (len == 0) continue;

        // Lấy thời gian hiện tại theo định dạng: 2026/05/26 11:00:00PM
        char time_str[64];
        get_current_time_string(time_str, sizeof(time_str));

        // Format lại gói tin gửi đi: "Thời gian id: nội dung chat"
        char broadcast_buf[4096];
        snprintf(broadcast_buf, sizeof(broadcast_buf), "%s %s: %s\n", 
                 time_str, clients[my_index].id, buf);

        // Phát tín hiệu tới toàn bộ thành viên khác trong phòng chat công cộng
        broadcast_message(broadcast_buf, sock);
    }

cleanup:
    // Khi client ngắt kết nối, dọn dẹp và trả lại slot trống cho mảng
    pthread_mutex_lock(&clients_mutex);
    printf("Client [%s] (Socket %d) disconnected.\n", clients[my_index].id, sock);
    clients[my_index].socket = -1;
    clients[my_index].is_registered = 0;
    memset(clients[my_index].id, 0, sizeof(clients[my_index].id));
    pthread_mutex_unlock(&clients_mutex);

    close(sock);
    return NULL;
}

// Gửi tin nhắn đến tất cả các client còn lại ngoại trừ người gửi và những người chưa đăng ký tên
void broadcast_message(const char *message, int sender_socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != -1 && clients[i].socket != sender_socket && clients[i].is_registered == 1) {
            send(clients[i].socket, message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Hàm phụ trợ tạo định dạng chuỗi thời gian chính xác theo đề bài nhu cầu
void get_current_time_string(char *buffer, size_t max_len) {
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    // strftime tự động xử lý định dạng %p cho AM/PM
    strftime(buffer, max_len, "%Y/%m/%d %I:%M:%S%p", timeinfo);
}