// SPDX-License-Identifier: ISC
//
// Copyright (C) 2025-2026 Antonio Niño Díaz

#ifndef LOG_H__
#define LOG_H__

typedef enum
{
    LOG_ERROR = 0,
    LOG_WARNING = 1,
    LOG_INFO = 2,
    LOG_VERBOSE = 3,
}
log_level;

void set_log_level(log_level level);
void print_log(log_level level, const char *msg, ...);

#define ERROR(...)      print_log(LOG_ERROR, __VA_ARGS__)
#define WARNING(...)    print_log(LOG_WARNING, __VA_ARGS__)
#define INFO(...)       print_log(LOG_INFO, __VA_ARGS__)
#define VERBOSE(...)    print_log(LOG_VERBOSE, __VA_ARGS__)

#endif // LOG_H__
