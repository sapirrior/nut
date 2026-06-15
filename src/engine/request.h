#ifndef NURL_ENGINE_REQUEST_H
#define NURL_ENGINE_REQUEST_H

#include "cli/args.h"
#include "engine/utils/nurl_headers.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

typedef struct {
    const char      *method;       /* "GET", "POST", etc. */
    const char      *url;
    NurlHeaderList  *headers;
    const uint8_t   *body;
    size_t           body_len;
    bool             body_is_stream; /* read from stdin lazily */

    /* Transfer config */
    unsigned int     timeout_sec;
    bool             follow_redirect;
    unsigned int     max_redirects;   /* default: 10 */
    unsigned int     retry_count;
    unsigned int     retry_delay_sec;
    bool             fail_on_error;

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

    /* Download-specific */
    bool             resume;
    unsigned long    resume_offset;

    /* Upload-specific */
    const char      *upload_file;
} NurlRequest;

NurlRequest *nurl_request_new(void);
void         nurl_request_from_args(NurlRequest *req, const char *method,
                                    const char *url, const BaseArgs *a);
void         nurl_request_free(NurlRequest *req);

#endif /* NURL_ENGINE_REQUEST_H */
