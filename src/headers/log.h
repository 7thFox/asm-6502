#ifndef LOG_H
#define LOG_H

#include <stdio.h>

extern long col;
extern long line;
#define errorf(...)                                       \
    {                                                     \
        fprintf(stderr, "[ERROR] (%li,%li) ", line, col); \
        fprintf(stderr, __VA_ARGS__);                     \
        fprintf(stderr, "\n");                            \
    }

#ifdef DEBUG
#define debugf(...)                                           \
    {                                                         \
        if (line > 0) {                                       \
            fprintf(stderr, "[DEBUG] (%li,%li) ", line, col); \
        }                                                     \
        else {                                                \
            fprintf(stderr, "[DEBUG] ");                      \
        }                                                     \
        fprintf(stderr, __VA_ARGS__);                         \
        fprintf(stderr, "\n");                                \
    }
#else
#define debugf(...) ;
#endif

#ifdef TRACE
#define tracef(...)                                       \
    {                                                     \
        fprintf(stderr, "[TRACE] (%li,%li) ", line, col); \
        fprintf(stderr, __VA_ARGS__);                     \
        fprintf(stderr, "\n");                            \
    }
#else
#define tracef(...) ;
#endif

#endif