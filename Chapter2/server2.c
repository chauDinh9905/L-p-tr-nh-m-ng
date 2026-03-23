/*******************************************************************************
 * @File: server2.c
 * @Date: 2026-03-17
 * @Description: Server nhận dữ liệu từ netcat client
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    char cmd[20], port[20], file_say_hi[20], file_content_from_client[20];
    scanf("%s %s %s %s", cmd, port, file_say_hi, file_content_from_client);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(atoi(port));

    int opt = 1; 
    // Thiết lập SO_REUSEADDR để giải phóng cổng ngay khi server tắt 
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) { 
        perror("setsockopt failed"); 
        exit(EXIT_FAILURE); 
    }

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind() failed");
        exit(EXIT_FAILURE);
    }

    listen(listener, 5);

    int client = accept(listener, NULL, NULL);
    if (client < 0) {
        perror("accept() failed");
        exit(EXIT_FAILURE);
    }

    FILE *f = fopen("say_hi_to_client.txt", "r");
    fgets(file_say_hi, sizeof(file_say_hi), f);
    fclose(f);
    f = fopen("content_from_client.txt", "w");

    send(client, file_say_hi, strlen(file_say_hi), 0);
    char buf[16];
    while (1) {
        int len = recv(client, buf, sizeof(buf), 0);
        if (len <= 0) {
            break;
        }
        buf[len] = 0;
        fputs(buf, f);
        printf("Received: %s", buf);
    }

    close(client);
    close(listener);
    fclose(f);
    return 0;
}