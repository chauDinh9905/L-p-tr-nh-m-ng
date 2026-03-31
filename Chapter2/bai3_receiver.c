#include<stdio.h>
#include<sys/socket.h>
#include<stdlib.h>
#include<string.h>
#include<netdb.h>
#include<unistd.h>

int main(){
    int receiver = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(receiver < 0){
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(9000);

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    bind(receiver, (struct sockaddr *)&addr, sizeof(addr));
    char buf[256];
    while(1){
        int res = recvfrom(receiver, buf, sizeof(buf) - 1, 0, (struct sockaddr*)&client_addr, &client_len);
        if(res < 0){
            perror("recvfrom() failed");
            exit(EXIT_FAILURE);
        }
        buf[res] = '\0';
        printf("%s", buf);
        sendto(receiver, buf, strlen(buf), 0, (struct sockaddr*)&client_addr, client_len);
    }
    close(receiver);
}