#ifndef NURL_ENGINE_H
#define NURL_ENGINE_H

#include "nurl.h"
#include "nurl_http.h"

/**
 * Performs a generic HTTP/1.1 request (GET, POST, etc.) including redirections,
 * proxy tunnels, cookie loading/saving, and TLS handshake.
 *
 * @param method The HTTP method (e.g. GET, POST)
 * @param url The initial request URL
 * @param common CLI common arguments
 * @param out_response Out-pointer that will be set to the parsed HTTP response (must be freed with nurl_http_response_free)
 * @param out_effective_url Out-pointer that will be set to the final redirect URL (must be freed with free)
 * @return nurl_err_t exit code (NURL_OK on success, or error code)
 */
int nurl_engine_execute_request(
    const char *method,
    const char *url,
    const CommonArgs *common,
    nurl_http_response_t **out_response,
    char **out_effective_url
);

#endif /* NURL_ENGINE_H */
