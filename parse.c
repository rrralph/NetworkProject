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
<head>\
<title>Error response</title>\
</head>\
<body>\
<h1>Error response</h1>\
<p>Error code %d.\
<p>Message: %s(%s).\
<p>Error code explanation: %d = %s.\
</body>";

char *prePath = "tmp/www/";
char msgBuf[BUF_LEN];

int getFileStat(const char *, struct stat**);

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
        rv = sprintf(msgBuf, errMsgTmp, 400, "Bad request syntax",buf, 400, code_explanation(400));
    }else{
        if(strcmp(request->http_version, "HTTP/1.1")){
            sprintf(msgBuf, errMsgTmp, 400, "Bad request version",request->http_version, 400, code_explanation(400));
        }else if(strcmp(request->http_method, "GET") 
            && strcmp(request->http_method, "POST")
            && strcmp(request->http_method, "HEAD") ){
            sprintf(msgBuf, errMsgTmp, 501, "Unsupported method", request->http_method, 501, code_explanation(501));
        }else {
            struct stat attrib;
            struct stat * attrib_ptr = &attrib; 
            if(getFileStat(request->http_uri, &attrib_ptr) == -1){
                perror(request->http_uri);
                int rv = set_header(msgBuf, "HTTP/1.1 %d %s\r\n", 404, "Not Found");
                rv += set_header(msgBuf + rv, "Server: Liso/1.0\r\n");
                rv += set_header(msgBuf + rv, "Date: %s\r\n",get_current_time());
                rv += set_header(msgBuf + rv, "Content-Type: text/html\r\n");

            }else{
                int rv = set_header(msgBuf, "HTTP/1.1 %d %s\r\n", 200, "OK");
                rv += set_header(msgBuf + rv, "Server: Liso/1.0\r\n");
                rv += set_header(msgBuf + rv, "Date: %s\r\n",get_current_time());
                rv += set_header(msgBuf + rv, "Content-Type: text/html\r\n");

                rv += set_header(msgBuf + rv, "Content-Length: %d\r\n", attrib_ptr->st_size);
                struct tm *tm = localtime(& ( attrib_ptr->st_mtime) );
                strftime(msgBuf + rv, 80, "Last-Modified: %a, %d %b %Y %X\r\n", tm );
            }
        }
    }
    send_all(sockfd, msgBuf, sizeof msgBuf );
    return 0;
}

int getFileStat(const char *pathStr, struct stat ** attrib_ptr){

    char fullPath[128];
    memset(fullPath, 0, 128);
    strcpy(fullPath, prePath);
    strcat(fullPath, pathStr);
    if(stat(fullPath, *attrib_ptr) == -1){
        perror("file not found");
        return -1;
    }
    if(S_ISDIR( (*attrib_ptr)->st_mode)){
        strcat(fullPath, "/index.html");
        if(stat(fullPath, *attrib_ptr) == -1){
            return -1;
        }
    }

    return 0;
}
