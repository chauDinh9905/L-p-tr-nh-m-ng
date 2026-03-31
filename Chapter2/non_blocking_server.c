/*******************************************************************************
* @file    non_blocking_server.c
* @brief   Mô tả ngắn gọn về chức năng của file
* @date    2026-03-31 07:10
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <errno.h>
#include<ctype.h>


typedef enum{WAIT_NAME, WAIT_MSSV} client_state;
typedef struct{
    int id;
    client_state state;
    char hoten[100];
    char mssv[10];
} client_infor;

void generate_hust_email(char *fullname, char *mssv, char *result) {
    char temp[128], *words[20];
    int count = 0;
    
    strcpy(temp, fullname);
    for(int i = 0; temp[i]; i++) temp[i] = tolower(temp[i]);
    char *token = strtok(temp, " ");
    while (token != NULL && count < 20) {
        words[count++] = token;
        token = strtok(NULL, " ");
    }

    if (count > 0) {
        // Lấy tên chính + dấu chấm
        strcpy(result, words[count - 1]);
        strcat(result, ".");
        // Lấy chữ cái đầu các từ đệm
        for (int i = 0; i < count - 1; i++) {
            strncat(result, words[i], 1);
        }
        // Ghép với mã số (Ví dụ 20235277 lấy từ số thứ 2 trở đi -> 235277)
        // Nếu muốn lấy toàn bộ thì dùng: strcat(result, mssv);
        if (strlen(mssv) > 2) strcat(result, mssv + 2);
        else strcat(result, mssv);
        
        strcat(result, "@sis.hust.edu.vn\n");
    }
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        return 1;
    }

    // Chuyen socket listener sang non-blocking
    unsigned long ul = 1;
    ioctl(listener, FIONBIO, &ul);

    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))) {
        perror("setsockopt() failed");
        close(listener);
        return 1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind() failed");
        close(listener);
        return 1;
    }

    if (listen(listener, 5)) {
        perror("listen() failed");
        close(listener);
        return 1;
    }

    // Server is now listening for incoming connections
    printf("Server is listening on port 8080...\n");

    client_infor clients[64];
    int nclients = 0;

    char buf[256];
    int len;

    while (1) {
        // Chap nhan ket noi
        int client = accept(listener, NULL, NULL);
        if (client == -1) {
            if (errno == EWOULDBLOCK) {
                // Loi do dang cho ket noi
                // Bo qua
            } else {
                // Loi khac
            }
        } else {
            printf("New client accepted: %d\n", client);
            clients[nclients].id = client;
            clients[nclients].state = WAIT_NAME;
            nclients++;
            ul = 1;
            ioctl(client, FIONBIO, &ul);
            send(client, "Ho ten: ", 8, 0);
        }

        // Nhan du lieu tu cac client
        for (int i = 0; i < nclients; i++) {
            len = recv(clients[i].id, buf, sizeof(buf), 0);
            if (len == -1) {
                if (errno == EWOULDBLOCK) {
                    // Loi do cho du lieu
                    // Bo qua 
                } else {
                    continue;
                }
            } else {
                if (len == 0)
                    continue;
                buf[strcspn(buf, "\r\n")] = 0;
                printf("Received from %d: %s\n", clients[i].id, buf);
                if(clients[i].state == WAIT_NAME){
                    strcpy(clients[i].hoten, buf);
                    clients[i].state = WAIT_MSSV;
                    send(clients[i].id, "MSSV: ", 6, 0);
                }else{
                    char email[256];
                    strcpy(clients[i].mssv, buf);
                    clients[i].state = WAIT_NAME;
                    generate_hust_email(clients[i].hoten, clients[i].mssv, email);
                    send(clients[i].id, email, strlen(email), 0);
                    send(clients[i].id, "Ho ten: ", 8, 0);
                }
            }

        }
    }

    close(listener);
    return 0;
}