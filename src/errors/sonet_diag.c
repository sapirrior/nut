#include "sonet_diag.h"
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

#ifdef _WIN32
#include <io.h>
#endif

void sonet_diag_err(const char *fmt, ...) {
    fprintf(stderr, "sonet: error: ");
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void sonet_diag_hint(const char *fmt, ...) {
    fprintf(stderr, "      hint: ");
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void sonet_diag_warn(const char *fmt, ...) {
    fprintf(stderr, "sonet: warning: ");
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

static const char oom_msg[] = "sonet: error: out of memory\n";

void sonet_diag_oom(void) {
#ifdef _WIN32
    _write(2, oom_msg, sizeof(oom_msg) - 1);
#else
    write(STDERR_FILENO, oom_msg, sizeof(oom_msg) - 1);
#endif
}
