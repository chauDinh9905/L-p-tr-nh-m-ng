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
#include<dirent.h>
#include<sys/stat.h>

int main(){
    int client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(client < 0){
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(9000);

    if(connect(client, (struct sockaddr*)&addr, sizeof(addr))){
        perror("connect() failed");
        exit(EXIT_FAILURE);
    }

    char cwd[256];
    if(getcwd(cwd, sizeof(cwd)) == NULL){
        perror("Current dir is empty");
        exit(EXIT_FAILURE);
    }
    char buf[300];
    sprintf(buf, "%s", cwd);
    send(client, buf, sizeof(buf), 0);

    DIR *d;
    d = opendir(".");
    if(d == NULL){
        perror("Current dir is not exist");
        exit(EXIT_FAILURE);
    }   

    struct dirent *dir;
    struct stat st;
    while((dir = readdir(d)) != NULL){
        stat(dir->d_name, &st);
        sprintf(buf, "%s - %ld bytes", dir->d_name, st.st_size);
        send(client, buf, sizeof(buf), 0);
    }

    closedir(d);
    close(client);
    return 0;
}