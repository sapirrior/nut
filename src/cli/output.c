#include "output.h"
#include <stdarg.h>
#include <unistd.h>

#ifdef _WIN32
#include <io.h>
#define check_isatty(fd) _isatty(fd)
#else
#define check_isatty(fd) isatty(fd)
#endif

void nurl_out_init(NurlOutputCtx *ctx, bool verbose, bool silent, bool include_headers, bool raw) {
    if (!ctx) return;
    ctx->verbose = verbose;
    ctx->silent = silent;
    ctx->include_headers = include_headers;
    ctx->raw = raw;
    ctx->is_tty = check_isatty(STDOUT_FILENO);
}

void nurl_out_response_headers(const NurlOutputCtx *ctx, const nurl_http_response_t *res) {
    if (!ctx || !res || ctx->silent) return;

    if (ctx->include_headers && !ctx->verbose) {
        printf("HTTP/1.1 %d %s\n", res->status_code, res->status_text);
        for (size_t i = 0; i < res->header_count; i++) {
            printf("%s\n", res->headers[i]);
        }
        printf("\n");
    }
}

void nurl_out_response_body(const NurlOutputCtx *ctx, const unsigned char *body, size_t len, FILE *out) {
    if (!ctx || !body || len == 0) return;
    // Note: response body is always written to the output file or stdout (even if silent is true, just like curl)
    fwrite(body, 1, len, out);
    fflush(out);
}

void nurl_out_verbose_log(const NurlOutputCtx *ctx, const char *prefix, const char *fmt, ...) {
    if (!ctx || !ctx->verbose || ctx->silent) return;

    fprintf(stderr, "%s ", prefix);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}
