#include "nurl_diag.h"
#include <stdio.h>
#include <stdarg.h>

void nurl_diag_err(const char *fmt, ...) {
    fprintf(stderr, "nurl: error: ");
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void nurl_diag_hint(const char *fmt, ...) {
    fprintf(stderr, "      hint: ");
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}
