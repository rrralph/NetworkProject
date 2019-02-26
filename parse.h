#ifndef PARSE_H_
#define PARSE_H_

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define SUCCESS 0

#ifndef BUF_LEN
#define BUF_LEN 8192
#endif

//Header field
typedef struct
{
	char header_name[4096];
	char header_value[4096];
} Request_header;

//HTTP Request Header
typedef struct
{
	char http_version[50];
	char http_method[50];
	char http_uri[4096];
	Request_header *headers;
	int header_count;
    int header_capacity;
} Request;

Request* parse(char *buffer, int size,int socketFd);


typedef struct{
    const char *key;
    const char *value;
} mime_type_t;

typedef struct {
    int connection;
    char host[128];
    char ip[128];
} http_out_t;

typedef int (*http_handler_t) (const char*, http_out_t *) ;
int checkAndResp(Request* , http_out_t *, const char *, int );

#endif

