#include <stdio.h>
#include <stdarg.h>

#include "log.h"

void wavy_log(enum wavy_log_t type, char *fmt, ...) {
    va_list args;
    FILE *stream = stdout;
    char *str_type;

    switch(type) {
    case LOG_WLC:
        if (!wlc_output_enabled) {
            return;
        }
        str_type = "[wlc] ";
        break;
    case LOG_WAVY:
        str_type = "[wavy] ";
        break;
    case LOG_DEBUG:
        if (!debug_enabled) {
            return;
        }
        str_type = "[debug] ";
        break;
    case LOG_ERROR:
        str_type = "[ERROR] ";
        break;
    }

    fputs(str_type, stream);
    va_start(args, fmt);
    vfprintf(stream, fmt, args);
    va_end(args);
    fprintf(stream, "\n");

}
