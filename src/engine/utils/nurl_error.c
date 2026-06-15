#include "nurl_error.h"
#include <stdio.h>
#include <stdarg.h>

nurl_err_t nurl_err(nurl_err_t code, const char *fmt, ...) {
    fprintf(stderr, "nurl: error [%d]: ", code);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    return code;
}

void nurl_hint(const char *fmt, ...) {
    fprintf(stderr, "      hint: ");
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}
