
#ifndef ECHO_SERVER_H_
#define ECHO_SERVER_H_

int send_all(int, char*, int);
int set_header(char *buf, const char *format, ...);
char *get_current_time();

#endif
