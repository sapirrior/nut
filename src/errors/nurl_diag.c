#include "nurl_diag.h"
#include <stdio.h>
#include <stdarg.h>

void nurl_diag_block(const char *block_type, const char *fmt, ...) {
    fprintf(stderr, "[ %s ]\n", block_type);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}
