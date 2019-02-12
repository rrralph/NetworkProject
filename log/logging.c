#include<stdarg.h>
#include<stdio.h>
#include<time.h>

#include "logging.h"


int get_current_time(char *buf){
    time_t t = time(NULL);
    struct tm *tm = gmtime(&t);
    int rv = strftime(buf, 80, "%a, %d %b %Y %X GMT", tm);
    if(rv == 0){
        perror("error in strftime in get_current_time");
    }
    return rv == 0 ? -1 : rv;
}

int logging(enum log_mode mode, FILE* fp, const char *format, ... ){
    
    char buffer[BUFSIZ];
    int rv = get_current_time(buffer);
    
    switch(mode){
        case LOG_INFO:
            rv += sprintf(buffer + rv, " [info]: ");
            break;
        case LOG_WARNING:
            rv += sprintf(buffer + rv, " [warning]: ");
            break;
        case LOG_ERROR:
            rv += sprintf(buffer + rv, " [error]: ");
            break;
        default:
            fprintf(stderr, "Unimplemented log_mode %d", mode);
    }

    va_list args;
    va_start(args, format);
    vsnprintf(buffer + rv, sizeof buffer, format, args);
    va_end(args);

    rv = fprintf(fp, "%s", buffer);
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
    logging(LOG_INFO,fp, "this is %s %d\n", "test", 1);
    logging(LOG_ERROR,fp, "this is %s %d\n", "test", 1);
    logging(LOG_WARNING,fp, "this is %s %d\n", "test", 1);

    return 0;
}
*/
