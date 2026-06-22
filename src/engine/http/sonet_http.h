#ifndef SONET_HTTP_H
#define SONET_HTTP_H

#include "net/sonet_stream.h"
#include "engine/sonet_engine_types.h"
#include "errors/sonet_error.h"
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

/**
 * Sends a structured HTTP/1.1 request via the provided active buffered stream.
 * method: "GET", "POST", etc.
 * path: The request path and query parameters (e.g. "/json?q=1").
 * hostname: The destination Host header value.
 * extra_headers: Null-terminated string containing extra "Name: Value\r\n" headers, or NULL.
 * body: Payload bytes, or NULL.
 * body_len: Length of payload in bytes.
 * Returns SONET_OK on success, or an explicit sonet_err_t on failure.
 * If successful, *out_response will contain the dynamically allocated response.
 */

typedef struct SonetHttpParams SonetHttpParams;
typedef void (*sonet_headers_cb)(SonetHttpParams *p, const sonet_http_response_t *res, void *user_data);

struct SonetHttpParams {
    const char        *method;
    const char        *path;
    const char        *hostname;
    const char        *extra_headers;
    const uint8_t     *body;
    size_t             body_len;
    SonetBodyPart      *body_parts;
    size_t             body_parts_count;
    FILE              *body_out;
    unsigned long      resume_offset;
    sonet_progress_cb   progress_cb;
    void              *progress_data;
    sonet_headers_cb    header_cb;
    void              *header_data;
    bool               http10;
};

sonet_err_t sonet_http_request(
    SonetStream *stream,
    SonetHttpParams *p,
    sonet_http_response_t **out_response
);

/**
 * Frees all memory associated with the response structure.
 */
void sonet_http_response_free(sonet_http_response_t *res);

#endif /* SONET_HTTP_H */
