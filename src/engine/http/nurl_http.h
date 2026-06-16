#ifndef NURL_HTTP_H
#define NURL_HTTP_H

#include "net/nurl_stream.h"
#include <stddef.h>

typedef struct {
    int status_code;
    char *status_text;
    char **headers;
    size_t header_count;
    unsigned char *body;
    size_t body_len;
} nurl_http_response_t;

#include "errors/nurl_error.h"

/**
 * Sends a structured HTTP/1.1 request via the provided active buffered stream.
 * method: "GET", "POST", etc.
 * path: The request path and query parameters (e.g. "/json?q=1").
 * hostname: The destination Host header value.
 * extra_headers: Null-terminated string containing extra "Name: Value\r\n" headers, or NULL.
 * body: Payload bytes, or NULL.
 * body_len: Length of payload in bytes.
 * Returns NURL_OK on success, or an explicit nurl_err_t on failure.
 * If successful, *out_response will contain the dynamically allocated response.
 */
#include "engine/request.h"
#include <stdio.h>

nurl_err_t nurl_http_request(
    NurlStream *stream,
    const char *method,
    const char *path,
    const char *hostname,
    const char *extra_headers,
    const unsigned char *body,
    size_t body_len,
    NurlBodyPart *body_parts,
    size_t body_parts_count,
    FILE *body_out,
    bool show_progress,
    bool silent,
    unsigned long resume_offset,
    nurl_progress_cb progress_cb,
    void *progress_data,
    nurl_http_response_t **out_response
);

/**
 * Frees all memory associated with the response structure.
 */
void nurl_http_response_free(nurl_http_response_t *res);

#endif /* NURL_HTTP_H */
