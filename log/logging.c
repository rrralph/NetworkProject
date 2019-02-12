#include<stdarg.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>

#include "logging.h"

#define DEBUG 1

char* get_log_time(){
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char *buf = malloc(30);
    memset(buf, 0, 30);
    int rv = strftime(buf,30, "[%d/%b/%Y %X]", tm);
    if(!rv){
        perror("error in get_log_time");
    }
    return buf;
}


int logging( FILE* fp, const char *format, ... ){
    
    char buffer[BUFSIZ];

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof buffer, format, args);
    va_end(args);

    int rv = fprintf(fp, "%s", buffer);

#ifdef DEBUG
    printf("%s", buffer);
#endif

    return rv;
}

int close_log_file(FILE *fp){
    int rv = fclose(fp);
    if(rv != 0){
        perror("error closing log file");
    }
    return rv;
}

/*
int main(){
    FILE* fp = fopen("log.txt","w");
    logging(fp, "- - - %s - - -", get_log_time());
    close_log_file(fp);
    return 0;
}
*/
