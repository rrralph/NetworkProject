#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netdb.h>
#include<time.h>
#include<arpa/inet.h>
#include<parse.h>

#include"log/logging.h"

#define PORT "9998"
#define BACKLOG 5
#define BUF_LEN 8192
#define RESP400 "HTTP/1.1 400 Bad Request\r\n\r\n"


int get_current_time(char *buf){
    time_t t = time(NULL);
    struct tm *tm = gmtime(&t);
    int rv = strftime(buf, 80, "%a, %d %b %Y %X GMT", tm);
    if(rv == 0){
        perror("error in strftime in get_current_time");
    }
    return rv == 0 ? -1 : rv;
}

void *get_in_addr(struct sockaddr *sa){
    if(sa->sa_family == AF_INET){
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


int recv_http_header(int sockfd, char *buf, int size){
    int rv = 0;
    int offset = 0;
    do{
        rv = recv(sockfd, buf + offset, size - offset, 0);
        if(rv != -1)
            offset += rv;
        if(rv <= 0 ||  (offset >= 4 && strncmp(buf+offset - 4, "\r\n\r\n", 4) == 0))
            break;
    }while(rv > 0);
    
    fprintf(stdout, "recv_http_header() gets: %d bytes\n ", offset);
    return rv == -1 ? -1 : offset;
}

int pack_crlf(char *buf){
    int rv = sprintf(buf, "\r\n");
    if(rv == -1){
        perror("error sprintf in pack_crlf");
    }
    return rv;
}


int pack_code_msg(char *buf, int code,const char *msg){
    int rv = sprintf(buf, "HTTP/1.1 %d %s\r\n", code, msg);
    if(rv == -1){
        perror("error sprintf in pack_code_msg");
    }
    return rv;
}
int pack_server_info(char *buf){
    int rv = sprintf(buf , "Server: Liso/1.0\r\n");
    if(rv == -1){
        perror("error sprintf in pack_server_info");
    }
    return rv;
}
int pack_time(char *buf){
    int rv = sprintf(buf, "Date: ");
    if(rv == -1){
        perror("error sprintf in pack_time");
        return rv;
    }
    int rv2 = get_current_time(buf + rv);
    if(rv2 == -1){
        perror("error sprintf in pack_time");
        return rv2;
    }
    int rv3 = pack_crlf(buf + rv + rv2);

    return rv3 == -1 ? rv3 : rv + rv2 + rv3;
}

int pack_connection(char *buf, int close){
    int rv;
    if(close){
        rv = sprintf(buf, "Connection: close\r\n");
    }else
        rv = sprintf(buf, "Connection: open\r\n");
    return rv;
}

int pack_cnt_type(char *buf, const char *msg){
    int rv = sprintf(buf, "Content-Type: %s\r\n", msg);
    if(rv == -1){
        perror("error sprintf in pack_cnt_type");
    }
    return rv;
}

int pack_error_msg(char *buf, int errorCode, const char* errorMsg){
    int totalRv = 0;
    int rv = pack_code_msg(buf, errorCode, errorMsg);
    if(rv == -1) return rv;
    else totalRv += rv;

    rv = pack_server_info(buf + totalRv);
    if(rv == -1) return rv;
    else totalRv += rv;

    rv = pack_time(buf + totalRv);
    if(rv == -1) return rv;
    else totalRv += rv;

    rv = pack_connection(buf+totalRv, 1);
    if(rv == -1) return rv;
    else totalRv += rv;

    rv = pack_cnt_type(buf + totalRv, "text/html");
    if(rv == -1) return rv;
    else totalRv += rv;

    return totalRv;
}

int send_all(int sockfd, char *buf, int size){
    int rv = 0;
    int offset = 0;
    do{
        rv = send(sockfd, buf + offset, size - offset, 0);
        if(rv == -1)
            break;
        offset += rv;
    }while(offset < size);

    return rv == -1 ? rv : offset;
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
    char recvbuf[8192];

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
                        /*
                        if( send(newfd, "Hello, world!", 13,0) == -1){
                            perror("Error sending");
                            continue;
                        }
                        */
                    }else{
                        memset(recvbuf, 0, sizeof(recvbuf));
                        if( (rv = recv_http_header(i, recvbuf, BUF_LEN - 1)) <= 0){
                            if( rv == 0){
                                fprintf(stdout, "socket %d hang up\n", i);
                            }else{
                                perror("Error recving");
                            }
                            close(i);
                            FD_CLR(i, &master);
                        }else{
                            Request *request = NULL;
                            request = parse(recvbuf, rv , i);
                            if(request != NULL){
                                printf("sending the same back\n");
                                if(send(i, recvbuf, rv,0) == -1){
                                    perror("Error sending");
                                }
                           }else{
                                printf("sending 400 back\n");
                                char buf[BUF_LEN];
                                int rv = pack_error_msg(buf, 400, "Bad Request");
                                if( (rv = send_all(i, buf, rv)) == -1){
                                    perror("Error sending");
                                }
                                printf("send %d btyes back\n",rv);
                                close(i);
                                FD_CLR(i, &master);
                            }
                        }
                    }
                }
            }
        }
    }

    return EXIT_SUCCESS;
}
