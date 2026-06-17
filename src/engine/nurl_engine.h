#ifndef NURL_ENGINE_H
#define NURL_ENGINE_H

#include "engine/nurl_engine_request.h"
#include "nurl_http.h"

#include "nurl_ctx.h"

typedef struct {
    double connect_time_sec;
    int    num_redirects;
} NurlOperationStats;

int nurl_engine_execute_request(
    NurlCtx *ctx,
    NurlRequest *req,
    nurl_http_response_t **out_response,
    char **out_effective_url,
    NurlOperationStats *out_stats
);

#endif /* NURL_ENGINE_H */
