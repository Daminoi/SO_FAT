#ifndef MINILOGGER_H
#define MINILOGGER_H

#include "common.h"

typedef enum miniLogLevel{
    ERROR,
    WARNING,
    INFO,
    LOG
}miniLogLevel;

/*
    Può essere usato specificando tutti i parametri oppure soltanto:
    - log_level e txt, ponendo function a NULL e line a -1
    - log_level e function e txt, ponendo line a -1
*/
void mini_log(miniLogLevel log_level, const char* const function_name, const int line, const char* const txt);

#endif /* MINILOGGER_H */