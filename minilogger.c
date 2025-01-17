#include <stdio.h>
#include <string.h>

#include "minilogger.h"

void mini_log(miniLogLevel log_level, const char* const function_name, const char* const msg){
    #ifdef DEBUG
    char log_string[10];

    if(msg == NULL){
        return;
    }

    switch(log_level){
    case ERROR:
        snprintf(log_string, 6, "ERROR");
        break;
    case WARNING:
        snprintf(log_string, 8, "WARNING");
        break;
    case INFO:
        snprintf(log_string, 5, "INFO");
        break;
    case LOG:
        snprintf(log_string, 4, "LOG");
        break;
    default:
        snprintf(log_string, 10, "UNDEFINED");
        break;
    }

    if(function_name == NULL){
        printf("%s: %s\n", log_string, msg);
    }
    else{
        printf("%s (func %s): %s\n", log_string, function_name, msg);
    }
    #endif

    return;
}