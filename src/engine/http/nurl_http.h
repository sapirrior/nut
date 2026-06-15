#ifndef NURL_HTTP_H
#define NURL_HTTP_H

#include "nurl_tls.h"
#include <stddef.h>

typedef struct {
    int status_code;
    char *status_text;
    char **headers;
    size_t header_count;
    unsigned char *body;
    size_t body_len;
} nurl_http_response_t;

/**
 * Sends a structured HTTP/1.1 request via the provided active TLS channel.
 * method: "GET", "POST", etc.
 * path: The request path and query parameters (e.g. "/json?q=1").
 * hostname: The destination Host header value.
 * extra_headers: Null-terminated string containing extra "Name: Value\r\n" headers, or NULL.
 * body: Payload bytes, or NULL.
 * body_len: Length of payload in bytes.
 * Returns a dynamically allocated nurl_http_response_t pointer on success, or NULL on error.
 */
nurl_http_response_t *nurl_http_request(
    nurl_tls_t *tls,
    const char *method,
    const char *path,
    const char *hostname,
    const char *extra_headers,
    const unsigned char *body,
    size_t body_len
);

/**
 * Frees all memory associated with the response structure.
 */
void nurl_http_response_free(nurl_http_response_t *res);

#endif /* NURL_HTTP_H */
