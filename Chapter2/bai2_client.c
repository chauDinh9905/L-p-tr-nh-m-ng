#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

int main(){
    int client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(client < 0){
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(9000);

    if(connect(client, (struct sockaddr*)&addr, sizeof(addr))){
        perror("connect() failed");
        exit(EXIT_FAILURE);
    }

    char buf[300];
    while(1){
        scanf("%s", buf);
        getchar();
        if(strcmp(buf, "exit") == 0) break;
        send(client, buf, strlen(buf), 0);
        memset(buf, 0, sizeof(buf));
    }
    close(client);
    return 0;
}