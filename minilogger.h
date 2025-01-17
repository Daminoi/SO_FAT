#ifndef MINILOGGER_H
#define MINILOGGER_H

#include "common.h"

typedef enum miniLogLevel{
    ERROR,
    WARNING,
    INFO,
    LOG
} miniLogLevel;

/*
    Può essere usato specificando tutti i parametri oppure soltanto:
    - log_level e txt, ponendo function a NULL
*/
void mini_log(miniLogLevel log_level, const char* const function_name, const char* const msg);

#endif /* MINILOGGER_H */