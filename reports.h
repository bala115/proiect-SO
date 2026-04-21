#ifndef REPORTS_H
#define REPORTS_H

#include <time.h>

typedef struct {
    int id;
    char inspector[50];
    float lat;
    float lon;
    char category[30];
    int severity;
    time_t timestamp;
    char description[200];
} Report;

#endif
