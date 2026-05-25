#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <local_port> <remote_ip> <remote_port>\n", argv[0]);
        return 1;
    }

    int local_port = atoi(argv[1]);
    char *remote_ip = argv[2];
    int remote_port = atoi(argv[3]);

    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    //  địa chỉ nhận (local) và bind
    struct sockaddr_in local_addr = {0};
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(local_port);

    if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    // địa chỉ gửi (remote)
    struct sockaddr_in remote_addr = {0};
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_addr.s_addr = inet_addr(remote_ip);
    remote_addr.sin_port = htons(remote_port);

    // 2 sự kiện bàn phím và từ mạng
    struct pollfd fds[2];
    
    // Theo dõi bàn phím (stdin)
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    // Theo dõi socket (mạng)
    fds[1].fd = sockfd;
    fds[1].events = POLLIN;

    printf("UDP Chat started. Local port: %d. Target: %s:%d\n", local_port, remote_ip, remote_port);
    printf("Type your message and press Enter\n");

    char buf[BUF_SIZE];

    while (1) {
        int ret = poll(fds, 2, -1); // Chờ vô hạn cho đến khi có sự kiện
        if (ret < 0) {
            perror("Poll failed");
            break;
        }

        // sự kiện từ bàn phím (fds[0])
        if (fds[0].revents & POLLIN) {
            if (fgets(buf, sizeof(buf), stdin) != NULL) {
                // Gửi dữ liệu qua UDP
                sendto(sockfd, buf, strlen(buf), 0, 
                       (struct sockaddr *)&remote_addr, sizeof(remote_addr));
                
                // Nếu người dùng nhập "exit", thoát chương trình
                if (strncmp(buf, "exit", 4) == 0) break;
            }
        }

        //sự kiện từ socket (fds[1])
        if (fds[1].revents & POLLIN) {
            struct sockaddr_in sender_addr;
            socklen_t addr_len = sizeof(sender_addr);
            
            int n = recvfrom(sockfd, buf, sizeof(buf) - 1, 0, 
                             (struct sockaddr *)&sender_addr, &addr_len);
            
            if (n > 0) {
                buf[n] = '\0';
                printf("Received: %s", buf);
            }
        }
    }

    close(sockfd);
    printf("Chat ended.\n");
    return 0;
}