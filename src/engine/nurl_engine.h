#ifndef NURL_ENGINE_H
#define NURL_ENGINE_H

#include "engine/request.h"
#include "nurl_http.h"

int nurl_engine_execute_request(
    NurlRequest *req,
    nurl_http_response_t **out_response,
    char **out_effective_url
);

#endif /* NURL_ENGINE_H */
