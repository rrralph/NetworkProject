/* C declarations used in actions */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "parse.h"

int main(int argc, char **argv){
  //Read from the file the sample
  int fd_in = open(argv[1], O_RDONLY);
  int index;
  char buf[8192];
	if(fd_in < 0) {
		printf("Failed to open the file\n");
		return 0;
	}
  int readRet = read(fd_in,buf,8192);
  //Parse the buffer to the parse function. You will need to pass the socket fd and the buffer would need to
  //be read from that fd
  Request *request = parse(buf,readRet,fd_in);
  //Just printing everything
  printf("Http Method %s\n",request->http_method);
  printf("Http Version %s\n",request->http_version);
  printf("Http Uri %s\n",request->http_uri);
  for(index = 0;index < request->header_count;index++){
    printf("Request Header\n");
    printf("Header name %s Header Value %s\n",request->headers[index].header_name,request->headers[index].header_value);
  }
  free(request->headers);
  free(request);

  int fd2 = open(argv[2], O_RDONLY);
  char buf2[8192];
  int rv = read(fd2, buf2, 8192);
  request = parse(buf2, rv, fd2);
  if(request == NULL)
      printf("Second Failed\n");
  else
      printf("Second file Succeed\n");

  request = parse(buf,readRet,fd_in);
  if(request == NULL)
      printf("Third Failed\n");
  else
      printf("Third file Succeed\n");

  return 0;
}
