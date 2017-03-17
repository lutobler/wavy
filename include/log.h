#ifndef __LOG_H
#define __LOG_H
#include <stdbool.h>

enum wavy_log_t {
    LOG_WLC,
    LOG_WAVY,
    LOG_DEBUG,
    LOG_ERROR,
};

extern bool debug_enabled;
extern bool wlc_output_enabled;
extern bool color_log_enabled;

void wavy_log(enum wavy_log_t type, char *fmt, ...);

#endif
