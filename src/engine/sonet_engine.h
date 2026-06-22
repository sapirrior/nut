#ifndef SONET_ENGINE_H
#define SONET_ENGINE_H

#include "engine/sonet_engine_request.h"
#include "sonet_http.h"

#include "sonet_ctx.h"

typedef struct {
    double connect_time_sec;
    int    num_redirects;
} SonetOperationStats;

int sonet_engine_execute_request(
    SonetCtx *ctx,
    SonetRequest *req,
    sonet_http_response_t **out_response,
    char **out_effective_url,
    SonetOperationStats *out_stats
);

#endif /* SONET_ENGINE_H */
