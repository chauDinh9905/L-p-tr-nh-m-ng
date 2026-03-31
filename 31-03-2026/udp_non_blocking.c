#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s udp_chat <port_s> <ip_d> <port_d>\n", argv[0]);
        return 1;
    }

    int port_s = atoi(argv[2]);
    char *ip_d = argv[3];
    int port_d = atoi(argv[4]);

    // 1. Tạo socket UDP
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == -1) {
        perror("socket() failed");
        return 1;
    }

    // 2. Thiết lập địa chỉ nhận (Local) và bind
    struct sockaddr_in addr_s = {0};
    addr_s.sin_family = AF_INET;
    addr_s.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_s.sin_port = htons(port_s);

    if (bind(s, (struct sockaddr *)&addr_s, sizeof(addr_s))) {
        perror("bind() failed");
        return 1;
    }

    // 3. Thiết lập địa chỉ gửi (Destination)
    struct sockaddr_in addr_d = {0};
    addr_d.sin_family = AF_INET;
    addr_d.sin_addr.s_addr = inet_addr(ip_d);
    addr_d.sin_port = htons(port_d);

    // 4. Chuyển Socket và STDIN sang Non-blocking
    unsigned long ul = 1;
    ioctl(s, FIONBIO, &ul);
    ioctl(STDIN_FILENO, FIONBIO, &ul);

    printf("UDP Chat started. Listening on port %d...\n", port_s);
    printf("Sending to %s:%d. Type 'exit' to quit.\n", ip_d, port_d);

    char buf[256];
    struct sockaddr_in remote_addr;
    socklen_t remote_addr_len = sizeof(remote_addr);

    while (1) {
        
        int len = recvfrom(s, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&remote_addr, &remote_addr_len);
        if (len > 0) {
            buf[len] = 0;
            printf("\n[Received from %s]: %s", inet_ntoa(remote_addr.sin_addr), buf);
            fflush(stdout);
        }

        if (fgets(buf, sizeof(buf), stdin) != NULL) {
            if (strncmp(buf, "exit", 4) == 0) break;

            // Gửi dữ liệu đi
            sendto(s, buf, strlen(buf), 0, 
                   (struct sockaddr *)&addr_d, sizeof(addr_d));
            printf("send: "); 
            fflush(stdout);
        }

    }

    close(s);
    return 0;
}