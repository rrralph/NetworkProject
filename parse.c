#include<stdio.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<time.h>


#include "parse.h"
#include "echo_server.h"
#include "log/logging.h"

extern FILE* logFp;

char *errMsgTmp = "\
<head>\n\
<title>Error response</title>\n\
</head>\n\
<body>\n\
<h1>Error response</h1>\n\
<p>Error code %d.\n\
<p>Message: %s(%s).\n\
<p>Error code explanation: %d = %s.\n\
</body>\n";

char *prePath = "tmp/www/";
char msgBuf[BUF_LEN * 10];

char* getFileStat(const char *, struct stat**);

/**
* Given a char buffer returns the parsed request headers
*/
Request * parse(char *buffer, int size, int socketFd) {
  //Differant states in the state machine
    printf("in parse(): \n%s", buffer);
	enum {
		STATE_START = 0, STATE_CR, STATE_CRLF, STATE_CRLFCR, STATE_CRLFCRLF
	};

	int i = 0, state;
	size_t offset = 0;
	char ch;
	char buf[8192];
	memset(buf, 0, 8192);


	state = STATE_START;
	while (state != STATE_CRLFCRLF) {
		char expected = 0;

		if (i == size)
			break;

		ch = buffer[i++];
		buf[offset++] = ch;

		switch (state) {
		case STATE_START:
		case STATE_CRLF:
			expected = '\r';
			break;
		case STATE_CR:
		case STATE_CRLFCR:
			expected = '\n';
			break;
		default:
			state = STATE_START;
			continue;
		}

		if (ch == expected){
			state++;
        }
		else
			state = STATE_START;

	}

  //Valid End State
	if (state == STATE_CRLFCRLF) {
		Request *request = (Request *) malloc(sizeof(Request));
        request->header_count=0;
        request->header_capacity = 1;
    //TODO You will need to handle resizing this in parser.y
        request->headers = (Request_header *) malloc(sizeof(Request_header));
		set_parsing_options(buf, i, request);

        printf("parsing %d bytes\n", i);
        yyrestart();
		if (yyparse() == SUCCESS) {
            printf("Parsing Succeeded\n");
            return request;
		}else{
            printf("Parsing Failed\n");
            free(request->headers);
            free(request);
            return NULL;
        }
	}
  //TODO Handle Malformed Requests
  printf("Final state is not crlfcrlf\n");
  return NULL;
}

char *get_filetype(const char *path){
    char* ptr = strrchr(path ,'.');
    if(strcmp(ptr, ".html") == 0)
        return "text/html";
    else if (strcmp(ptr, ".css") == 0)
        return "text/css";
    else if(strcmp(ptr, ".png") == 0)
        return "image/png";
    else if(strcmp(ptr, ".jpg") == 0)
        return "image/jpeg";
    else if(strcmp(ptr, ".gif") == 0)
        return "image/gif";
    return "application/octet-stream";
}

char *code_explanation(int code){
    switch(code){
        case 400:
            return "Bad request syntax or unsupported method";
        case 404:
            return "Nothing matches the given URI";
        case 501:
            return "Server does not support this operation";
        default:
            perror("code is unknown");
    }
    return NULL;
}


int checkAndResp(Request *request, const char* buf, int sockfd){
    for(int i = 0; i < BUF_LEN; i++) msgBuf[i] = '\0';
    int rv = 0;
    if(request == NULL){
        rv = pack_error_msg(msgBuf, 400, "Bad request syntax");
        rv += sprintf(msgBuf + rv, "\r\n");
        rv += sprintf(msgBuf + rv, errMsgTmp, 400, "Bad request syntax",buf, 400, code_explanation(400));
    }else{
        if(strcmp(request->http_version, "HTTP/1.1")){
            rv = pack_error_msg(msgBuf, 400, "Bad request syntax");
            rv += sprintf(msgBuf + rv, "\r\n");
            rv += sprintf(msgBuf + rv, errMsgTmp, 400, "Bad request version",request->http_version, 400, code_explanation(400));
        }else if(strcmp(request->http_method, "GET") 
            && strcmp(request->http_method, "POST")
            && strcmp(request->http_method, "HEAD") ){
            rv = pack_error_msg(msgBuf, 501,"Unsupported method");
            rv += sprintf(msgBuf + rv, "\r\n");
            rv += sprintf(msgBuf + rv, errMsgTmp, 501, "Unsupported method", request->http_method, 501, code_explanation(501));
        }else {
            struct stat attrib;
            struct stat * attrib_ptr = &attrib; 
            char *fullPath;
            if( (fullPath = getFileStat(request->http_uri, &attrib_ptr)) == NULL){
                perror(request->http_uri);
                rv = pack_error_msg(msgBuf, 404, "Not found");
                rv += sprintf(msgBuf + rv, "\r\n");
                rv += sprintf(msgBuf + rv, errMsgTmp, 404, "Not found",request->http_version, 404, code_explanation(404));
            }else{
                rv = set_header(msgBuf, "HTTP/1.1 %d %s\r\n", 200, "OK");
                rv += set_header(msgBuf + rv, "server: Liso/1.0\r\n");
                rv += set_header(msgBuf + rv, "date: %s\r\n",get_current_time());

                char* ft = get_filetype(fullPath);

                rv += set_header(msgBuf + rv, "content-type: %s\r\n", ft);

                rv += set_header(msgBuf + rv, "content-length: %d\r\n", attrib_ptr->st_size);
                rv += set_header(msgBuf + rv, "connection: Keep-Alive\r\n");

                struct tm *tm = localtime(& ( attrib_ptr->st_ctime) );
                rv += strftime(msgBuf + rv, 80, "last-modified: %a, %d %b %Y %X\r\n", tm );
                rv += sprintf(msgBuf + rv, "\r\n");
                if(strcmp(request->http_method, "GET")==0){
                    FILE* fp = fopen(fullPath, "r");
                    if(fp == NULL)
                        printf("error open file");
                    char c;
                    while( (c = getc(fp)) != EOF){
                        msgBuf[rv++] = c;
                    }
                    free(fullPath);
                    rv += sprintf(msgBuf+rv, "\r\n");
                }
            }
        }
    }
    send_all(sockfd, msgBuf, sizeof msgBuf );
    printf("sent %d\n", strlen(msgBuf));
    printf("%s",msgBuf);
    return rv;
}

char* getFileStat(const char *pathStr, struct stat ** attrib_ptr){
    char *fullPath = malloc(128);
    memset(fullPath, 0, 128);
    strcpy(fullPath, prePath);
    strcat(fullPath, pathStr);
    if(stat(fullPath, *attrib_ptr) == -1){
        perror("file not found");
        return NULL;
    }
    if(S_ISDIR( (*attrib_ptr)->st_mode)){
        strcat(fullPath, "/index.html");
        if(stat(fullPath, *attrib_ptr) == -1){
            return NULL;
        }
    }
    return fullPath;
}
