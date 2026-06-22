#ifndef SONET_REQUEST_H
#define SONET_REQUEST_H

#include "sonet.h"
#include "engine/sonet_ctx.h"

/**
 * Performs a generic, secure HTTP/1.1 request (GET, POST, etc.) including redirections,
 * and outputs the response.
 */
int sonet_request_generic(SonetCtx *ctx, const char *method, const char *url, const CommonArgs *common);
void dump_headers_to_file(const char *filename, const sonet_http_response_t *res);

#endif /* SONET_REQUEST_H */
