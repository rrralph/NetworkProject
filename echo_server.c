#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netdb.h>




#define PORT "9998"
#define BACKLOG 5



int main(){

    struct addrinfo hints, *servinfo, *p;

    int rv;
    int listener;
    int sockfd;
    int yes = 1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if( (rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return EXIT_FAILURE;
    }

    for(p = servinfo; p != NULL; p = p->ai_next){
        if( (listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            perror("Error creating socket");
            continue;
        }

        if( bind(listener, p->ai_addr, p->ai_addrlen) == -1){
            close(listener);
            perror("Error binding");
            continue;
        }
        break;
    }

    freeaddrinfo(servinfo);

    if( p == NULL){
        fprintf(stderr, "Error binding");
        return EXIT_FAILURE;
    }

    if(listen(listener, BACKLOG) == -1){
        close(listener);
        perror("Error listening");
        return EXIT_FAILURE;
    }

    struct sockaddr_storage theiraddr;
    socklen_t sin_size;

    while(1){
        if( (sockfd = accept(listener, (struct sockaddr*) &theiraddr, &sin_size)) == -1){
            close(listener);
            perror("Error accepting");
            return EXIT_FAILURE;
        }

        if( send(sockfd, "Hello, world!", 13,0) == -1){
            perror("Error sending");
            continue;
        }
        close(sockfd);
    }

    return EXIT_SUCCESS;

}
