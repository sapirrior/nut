#ifndef NURL_OUTPUT_H
#define NURL_OUTPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "nurl_http.h"

typedef struct {
    bool verbose;
    bool silent;
    bool include_headers;
    bool raw;
    bool is_tty;
} NurlOutputCtx;

void nurl_out_init(NurlOutputCtx *ctx, bool verbose, bool silent, bool include_headers, bool raw);
void nurl_out_response_headers(const NurlOutputCtx *ctx, const nurl_http_response_t *res);
void nurl_out_response_body(const NurlOutputCtx *ctx, const unsigned char *body, size_t len, FILE *out);
void nurl_out_verbose_log(const NurlOutputCtx *ctx, const char *prefix, const char *fmt, ...);

#endif /* NURL_OUTPUT_H */
