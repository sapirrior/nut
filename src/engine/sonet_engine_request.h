#ifndef SONET_ENGINE_REQUEST_H
#define SONET_ENGINE_REQUEST_H

#include "sonet.h"
#include "engine/utils/sonet_headers.h"
#include "engine/sonet_engine_types.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

typedef struct SonetConnPool SonetConnPool;

typedef struct SonetRequest SonetRequest;
typedef void (*sonet_req_headers_cb)(SonetRequest *req, const sonet_http_response_t *res, void *user_data);

struct SonetRequest {
    const char      *method;       /* "GET", "POST", etc. */
    const char      *url;
    SonetHeaderMap  *headers;
    const uint8_t   *body;
    size_t           body_len;
    bool             body_is_stream; /* read from stdin lazily */

    SonetBodyPart    *body_parts;
    size_t           body_parts_count;

    /* Transfer config */
    unsigned int     read_timeout_sec;
    unsigned int     connect_timeout_sec;
    bool             follow_redirect;
    unsigned int     max_redirects;   /* default: 10 */
    unsigned int     retry_count;
    unsigned int     retry_delay_sec;
    unsigned long    limit_rate;
    bool             fail_on_error;
    bool             fail_with_body;

    /* TLS config */
    bool             tls_verify;
    const char      *cacert;
    const char      *cert;
    const char      *key;
    int              tls_version;     /* 0=auto, 12=TLSv1.2, 13=TLSv1.3 */

    /* Proxy */
    const char      *proxy;
    const char      *proxy_user;
    const char      *no_proxy;
    const char      *connect_to;

    /* Cookies */
    const char      *cookie;
    const char      *cookie_jar;
    const char      *session;

    /* Output */
    FILE            *out;             /* stdout or file handle */
    bool             include_headers; /* print response headers */
    bool             verbose;
    bool             silent;
    bool             raw_output;
    bool             decompress;
    bool             http10;

    /* Download-specific */
    bool             resume;
    bool             progress;
    unsigned long    resume_offset;
    sonet_progress_cb progress_cb;
    void            *progress_data;
    sonet_req_headers_cb header_cb;
    void            *header_data;

    SonetConnPool *pool;
    struct SonetStream *stream;
    char             last_tls_error[256];
};

SonetRequest *sonet_request_new(void);
void         sonet_request_from_args(SonetRequest *req, const char *method,
                                    const char *url, const CommonArgs *a);
void         sonet_request_free(SonetRequest *req);

/* Internal request building helpers */
sonet_err_t sonet_headermap_apply_auth(SonetHeaderMap *m, const CommonArgs *a);
sonet_err_t sonet_headermap_apply_common(SonetHeaderMap *m, const CommonArgs *a);

#endif /* SONET_ENGINE_REQUEST_H */
