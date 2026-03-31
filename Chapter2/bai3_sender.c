#include<stdio.h>
#include<sys/socket.h>
#include<stdlib.h>
#include<string.h>
#include<arpa/inet.h>
#include<unistd.h>

int main(){
    int sender = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sender < 0){
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }
    char *msg = "Hello, I am sending a message.\n";

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(9000);

    char buf[256];
    while(1){
        printf("Message: ");
        fgets(buf, sizeof(buf), stdin);
        sendto(sender, buf, strlen(buf), 0, (struct sockaddr*)&addr, sizeof(addr));
        int receive = recvfrom(sender, buf, sizeof(buf) - 1, 0, NULL, NULL);
        printf("Sent back: %s", buf);
    }
    close(sender);
    return 0;
}