#include<stdarg.h>
#include<sys/stat.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>

#include "logging.h"

#define DEBUG 1
#define LOGNAME "log.txt"

FILE* log_fptr = NULL;

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


int logging( const char *format, ... ){
    
    char buffer[BUFSIZ];

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof buffer, format, args);
    va_end(args);
    if(log_fptr == NULL && init_log() == -1) return -1;
    int rv = fprintf(log_fptr, "%s", buffer);

#ifdef DEBUG
    printf("%s", buffer);
#endif

    return rv;
}

int init_log(){
    struct stat statbuf;
    if(stat(LOGNAME, &statbuf) != -1){
        printf("%s exists, force replacing it!\n", LOGNAME);
    }
    log_fptr = fopen(LOGNAME,"w");
    if(log_fptr == NULL){
        perror("error init log file");
        return -1;
    }
    return 0;
}

int close_log(){
    int rv = fclose(log_fptr);
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
