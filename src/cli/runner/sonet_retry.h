#ifndef SONET_RETRY_H
#define SONET_RETRY_H

#include "sonet.h"
#include "engine/sonet_engine_request.h"
#include "sonet_engine.h"

/**
 * Executes a request with automatic retries on transient errors.
 */
int execute_with_retry(SonetCtx *ctx, SonetRequest *req, const CommonArgs *common, sonet_http_response_t **out_res, char **out_effective_url, SonetOperationStats *stats);

#endif /* SONET_RETRY_H */
