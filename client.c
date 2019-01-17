#include<sys/types.h>
#include<sys/socket.h>
#include<netdb.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>

#define BUF_LEN 32

int main(int argc, char *argv[]){

    if(argc !=3){
        fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    struct addrinfo hints, *servinfo, *p;
    int rv;
    int sockfd;
    char buf[BUF_LEN];

    memset(&hints, 0 , sizeof hints);
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if( (rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo )) != 0 ){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return EXIT_FAILURE;
    }

    for(p = servinfo; p != NULL; p = p->ai_next){
        if( (sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("Error creating socket");
            continue;
        }
        if( (rv = connect(sockfd, p->ai_addr, p->ai_addrlen)) == -1){
            close(sockfd);
            perror("Error connecting");
            continue;
        }
        break;
    }
    freeaddrinfo(servinfo);

    if(p == NULL){
        fprintf(stderr, "Error binding a socket");
        return EXIT_FAILURE;
    }

    if(send(sockfd, "hello, world$", 13,0) <=0){
        perror("error sending");
    }

    while( (rv = recv(sockfd, &buf, BUF_LEN, 0))  > 0){
        buf[rv]='\0';
        fprintf(stdout, "Receive msg: %s\n", buf);
    }

    if( close(sockfd) == -1){
        perror("Error closing socket");

    }

    return EXIT_SUCCESS;

}
