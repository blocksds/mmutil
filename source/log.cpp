// SPDX-License-Identifier: ISC
//
// Copyright (C) 2025-2026 Antonio Niño Díaz

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "log.h"

#define DEFAULT "\033[0m"
#define RED     "\033[91m"
#define YELLOW  "\033[93m"
#define CYAN    "\033[96m"
#define BOLD    "\033[1m"

static log_level curr_log_level = LOG_INFO;
static bool logging_initialized = false;
static bool log_color_enabled = false;

void set_log_level(log_level level)
{
    curr_log_level = level;
}

void print_log(log_level level, const char *msg, ...)
{
    if (!logging_initialized)
    {
        logging_initialized = true;

        if (isatty(STDOUT_FILENO))
        {
            // https://no-color.org/
            //
            // Command-line software which adds ANSI color to its output by
            // default should check for a NO_COLOR environment variable that,
            // when present and not an empty string (regardless of its value),
            // prevents the addition of ANSI color.

            char *no_color = getenv("NO_COLOR");

            log_color_enabled = true;
            if (no_color != NULL && no_color[0] != '\0')
                log_color_enabled = false;
        }
        else
        {
            log_color_enabled = false;
        }
    }

    if (curr_log_level < level)
        return;

    if (level == LOG_ERROR)
    {
        if (log_color_enabled)
            printf(RED);
        printf("error: ");
    }
    else if (level == LOG_WARNING)
    {
        if (log_color_enabled)
            printf(YELLOW);
        printf("warning: ");
    }
    else if (level == LOG_VERBOSE)
    {
        if (log_color_enabled)
            printf(CYAN);
    }

    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);

    if (log_color_enabled)
    {
        printf(DEFAULT);
    }
}
