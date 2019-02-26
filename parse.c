#include<stdio.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<time.h>
#include<sys/mman.h>
#include<fcntl.h>


#include "parse.h"
#include "echo_server.h"
#include "log/logging.h"

extern FILE* logFp;

int connection_handler(const char*, http_out_t *);
int host_handler(const char*, http_out_t *);


char* handler_names[] = {
    "connection",
    "host",
};

http_handler_t http_handlers [] = {
    connection_handler,
    host_handler,
};





mime_type_t FILE_TYPES [] = {
    {".html", "text/html"},
    {".css", "text/css"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".gif", "image/gif"},
    {NULL, "application/octet-stream"},
};

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
    int i = 0;
    for(; FILE_TYPES[i].key != NULL; i++){
        if(strcmp(ptr, FILE_TYPES[i].key) == 0)
            break;
    };
    return FILE_TYPES[i].value;
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


int checkAndResp(Request *request, http_out_t *out_ptr, const char* buf,  int sockfd  ){

    char msgBuf[BUF_LEN * 10];
    for(int i = 0; i < BUF_LEN * 10; i++) msgBuf[i] = '\0';

    int rv = 0;
    if(request == NULL){
        rv = pack_error_msg(msgBuf, 400, "Bad request syntax");
        rv += sprintf(msgBuf + rv, "\r\n");
        rv += sprintf(msgBuf + rv, errMsgTmp, 400, "Bad request syntax",buf, 400, code_explanation(400));
        send_all(sockfd, msgBuf, rv );
    }else{
        if(strcmp(request->http_version, "HTTP/1.1")){
            rv = pack_error_msg(msgBuf, 400, "Bad request syntax");
            rv += sprintf(msgBuf + rv, "\r\n");
            rv += sprintf(msgBuf + rv, errMsgTmp, 400, "Bad request version",request->http_version, 400, code_explanation(400));
            send_all(sockfd, msgBuf, rv );
        }else if(strcmp(request->http_method, "GET") 
            && strcmp(request->http_method, "POST")
            && strcmp(request->http_method, "HEAD") ){
            rv = pack_error_msg(msgBuf, 501,"Unsupported method");
            rv += sprintf(msgBuf + rv, "\r\n");
            rv += sprintf(msgBuf + rv, errMsgTmp, 501, "Unsupported method", request->http_method, 501, code_explanation(501));
            send_all(sockfd, msgBuf,  rv );
        }else {

            struct stat attrib;
            struct stat * attrib_ptr = &attrib; 
            char *fullPath;
            if( (fullPath = getFileStat(request->http_uri, &attrib_ptr)) == NULL){
                perror(request->http_uri);
                rv = pack_error_msg(msgBuf, 404, "Not found");
                rv += sprintf(msgBuf + rv, "\r\n");
                rv += sprintf(msgBuf + rv, errMsgTmp, 404, "Not found",request->http_version, 404, code_explanation(404));
                send_all(sockfd, msgBuf, rv );
            }else{

                setup_http_out(request, out_ptr);

                rv = set_header(msgBuf, "HTTP/1.1 %d %s\r\n", 200, "OK");
                rv += set_header(msgBuf + rv, "server: Liso/1.0\r\n");
                rv += set_header(msgBuf + rv, "date: %s\r\n",get_current_time());

                char* ft = get_filetype(fullPath);

                rv += set_header(msgBuf + rv, "content-type: %s\r\n", ft);

                int filesize = attrib_ptr->st_size;

                rv += set_header(msgBuf + rv, "content-length: %d\r\n", filesize);
                if(out_ptr->connection == 1)
                    rv += set_header(msgBuf + rv, "connection: Keep-Alive\r\n");
                else
                    rv += set_header(msgBuf + rv, "connection: Close\r\n");

                struct tm *tm = localtime(& ( attrib_ptr->st_ctime) );
                rv += strftime(msgBuf + rv, 80, "last-modified: %a, %d %b %Y %X\r\n", tm );
                rv += sprintf(msgBuf + rv, "\r\n");
                send_all(sockfd, msgBuf, rv);

                if(strcmp(request->http_method, "GET")==0){
                    int srcfd = open(fullPath, O_RDONLY, 0);
                    char* fileaddr = (char*)mmap(NULL,filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); 
                    close(srcfd);
                    int n = send_all(sockfd, fileaddr, filesize);
                    logging("filesize :%d, sent %d\n", filesize, n);
                    rv += n;
                    munmap(fileaddr, filesize );
                    rv += send_all(sockfd,"\r\n",2);

                }
            }
        }
    }
    printf("sent %d\n", rv);
    //printf("%s",msgBuf);
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

int setup_http_out( Request* request, http_out_t *out_ptr){

    Request_header *p = request->headers;
    for(int i = 0; i < request->header_count; i++){
        for(int j = 0; j < sizeof(handler_names)/ sizeof(handler_names[0]); j++){
            if(strcasecmp(handler_names[j], p[i].header_name) == 0){
                http_handlers[j](p[i].header_value, out_ptr);
                break;
            }
        }
    }
    return 0;
}

int connection_handler( const char *info, http_out_t *out_ptr){
    if(strcasecmp(info , "keep-alive") == 0){
        out_ptr->connection = 1;
    }else
        out_ptr->connection = 0;
    return 0;
}

int host_handler(const char *info, http_out_t *out_ptr){
    strcpy(out_ptr->host, info);
    return 0;
}




