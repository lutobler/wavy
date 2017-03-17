#include <stdio.h>
#include <stdarg.h>

#include "log.h"

#define ANSI_COLOR_RED      "\x1b[31m"
#define ANSI_COLOR_BLUE     "\x1b[34m"
#define ANSI_COLOR_YELLOW   "\x1b[33m"
#define ANSI_COLOR_RESET    "\x1b[0m"

void wavy_log(enum wavy_log_t type, char *fmt, ...) {
    va_list args;
    FILE *stream = stdout;
    char *str_prefix;

    switch(type) {
    case LOG_WLC:
        if (!wlc_output_enabled) {
            return;
        }
        str_prefix = "[wlc] ";
        break;
    case LOG_WAVY:
        if (color_log_enabled) {
            str_prefix = ANSI_COLOR_BLUE "[wavy] " ANSI_COLOR_RESET;
        } else {
            str_prefix = "[wavy] ";
        }
        break;
    case LOG_DEBUG:
        if (!debug_enabled) {
            return;
        }
        if (color_log_enabled) {
            str_prefix = ANSI_COLOR_YELLOW "[debug] " ANSI_COLOR_RESET;
        } else {
            str_prefix = "[debug] ";
        }
        break;
    case LOG_ERROR:
        if (color_log_enabled) {
            str_prefix = ANSI_COLOR_RED "[ERROR] " ANSI_COLOR_RESET;
        } else {
            str_prefix = "[ERROR] ";
        }
        break;
    }

    fputs(str_prefix, stream);
    va_start(args, fmt);
    vfprintf(stream, fmt, args);
    va_end(args);
    fprintf(stream, "\n");

}
