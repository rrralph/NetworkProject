#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netdb.h>
#include<arpa/inet.h>

#define PORT "9998"
#define BACKLOG 5

void *get_in_addr(struct sockaddr *sa){
    if(sa->sa_family == AF_INET){
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(){

    struct addrinfo hints, *servinfo, *p;

    int rv;
    int listener;
    int newfd;
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

        if(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
            close(listener);
            perror("Error settsockopt");
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
    socklen_t sin_size = sizeof(struct sockaddr_in);

    int fdmax = listener;
    fd_set master, readfds;
    char addrbuf[32];
    char recvbuf[128];

    FD_SET(listener, &master);


    while(1){
        readfds = master;
        rv = select(fdmax+1, &readfds, NULL, NULL, NULL);
        if(rv == -1){
            close(listener);
            perror("Error selecting");
            return EXIT_FAILURE;
        }else if(rv == 0){
            fprintf(stdout, "No client/msg");
            continue;
        }else{
            for(int i =0; i <= fdmax; i++){
                if(FD_ISSET(i, &readfds)){
                    if(i == listener){
                        if( (newfd = accept(listener, (struct sockaddr*) &theiraddr, &sin_size)) == -1){
                            close(listener);
                            perror("Error accepting");
                            return EXIT_FAILURE;
                        }
                        FD_SET(newfd, &master);
                        if(newfd > fdmax)
                            fdmax = newfd;
                        fprintf(stdout,"New connection from client: %s\n", inet_ntop(theiraddr.ss_family, get_in_addr((struct sockaddr*) &theiraddr), addrbuf, sin_size));
                        if( send(newfd, "Hello, world!", 13,0) == -1){
                            perror("Error sending");
                            continue;
                        }
                    }else{
                        if( (rv = recv(i, recvbuf, 127, 0)) <= 0){
                            if( rv == 0){
                                fprintf(stdout, "socket %d hang up", i);
                            }else{
                                perror("Error recving");
                            }
                            close(i);
                            FD_CLR(i, &master);
                        }else{
                            recvbuf[rv] = '\0';
                            fprintf(stdout, "Msg from socket %d: %s",i,recvbuf);
                        }
                    }
                }
            }
        }
    }

    return EXIT_SUCCESS;
}
