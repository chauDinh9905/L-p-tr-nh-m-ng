#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>

#define MAX_CLIENTS 1024
#define PORT 9000
#define BUF_SIZE 2048

// Cấu trúc lưu trữ thông tin đăng ký của Client
typedef struct {
    int fd;
    char topics[10][64]; // Mỗi client có thể đăng ký tối đa 10 topics
    int num_topics;
} ClientInfo;

ClientInfo clients[MAX_CLIENTS];

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        return 1;
    }
    
    listen(listener, 10);
    printf("Pub/Sub Server listening on port %d...\n", PORT);

    struct pollfd fds[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        fds[i].fd = -1;
        clients[i].num_topics = 0;
    }

    fds[0].fd = listener;
    fds[0].events = POLLIN;
    int nfds = 1;

    char buf[BUF_SIZE];

    while (1) {
        int ret = poll(fds, nfds, -1);
        if (ret < 0) break;

        for (int i = 0; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == listener) {
                    // CHẤP NHẬN KẾT NỐI MỚI
                    int client_fd = accept(listener, NULL, NULL);
                    if (nfds < MAX_CLIENTS) {
                        fds[nfds].fd = client_fd;
                        fds[nfds].events = POLLIN;
                        clients[nfds].fd = client_fd;
                        clients[nfds].num_topics = 0;
                        nfds++;
                        printf("New client connected: fd %d\n", client_fd);
                    } else {
                        close(client_fd);
                    }
                } else {
                    // NHẬN DỮ LIỆU TỪ CLIENT
                    int n = recv(fds[i].fd, buf, sizeof(buf) - 1, 0);
                    if (n <= 0) {
                        printf("Client fd %d disconnected\n", fds[i].fd);
                        close(fds[i].fd);
                        fds[i] = fds[nfds - 1];
                        clients[i] = clients[nfds - 1];
                        nfds--;
                        i--;
                        continue;
                    }

                    buf[n] = 0;
                    buf[strcspn(buf, "\r\n")] = 0; // Xóa ký tự xuống dòng

                    // XỬ LÝ LỆNH SUB
                    if (strncmp(buf, "SUB ", 4) == 0) {
                        char *topic = buf + 4;
                        if (clients[i].num_topics < 10) {
                            strcpy(clients[i].topics[clients[i].num_topics++], topic);
                            char msg[] = "Subscription successful.\n";
                            send(fds[i].fd, msg, strlen(msg), 0);
                            printf("Client %d subscribed to: %s\n", fds[i].fd, topic);
                        }
                    } 
                    // XỬ LÝ LỆNH PUB
                    else if (strncmp(buf, "PUB ", 4) == 0) {
                        char topic[64], message[BUF_SIZE];
                        // Tách topic và message từ lệnh PUB <topic> <msg>
                        char *first_space = strchr(buf + 4, ' ');
                        if (first_space != NULL) {
                            int topic_len = first_space - (buf + 4);
                            strncpy(topic, buf + 4, topic_len);
                            topic[topic_len] = '\0';
                            strcpy(message, first_space + 1);

                            printf("Publishing to %s: %s\n", topic, message);

                            // Chuyển tiếp tin nhắn cho các client đã SUB topic này
                            char send_buf[BUF_SIZE + 100];
                            sprintf(send_buf, "[%s]: %s\n", topic, message);

                            for (int j = 1; j < nfds; j++) {
                                for (int t = 0; t < clients[j].num_topics; t++) {
                                    if (strcmp(clients[j].topics[t], topic) == 0) {
                                        send(fds[j].fd, send_buf, strlen(send_buf), 0);
                                        break; 
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}