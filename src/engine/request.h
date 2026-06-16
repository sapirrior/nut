#ifndef NURL_ENGINE_REQUEST_H
#define NURL_ENGINE_REQUEST_H

#include "nurl.h"
#include "engine/utils/nurl_headers.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

typedef struct NurlConnPool NurlConnPool;

typedef enum {
    NURL_BODY_PART_MEM,
    NURL_BODY_PART_FILE
} NurlBodyPartType;

typedef struct {
    NurlBodyPartType type;
    const uint8_t *data;
    size_t len;
    const char *filepath;
} NurlBodyPart;

typedef void (*nurl_progress_cb)(unsigned long downloaded, unsigned long total, bool finished, void *user_data);

typedef struct {
    const char      *method;       /* "GET", "POST", etc. */
    const char      *url;
    NurlHeaderMap  *headers;
    const uint8_t   *body;
    size_t           body_len;
    bool             body_is_stream; /* read from stdin lazily */

    NurlBodyPart    *body_parts;
    size_t           body_parts_count;

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
    bool             progress;
    unsigned long    resume_offset;
    nurl_progress_cb progress_cb;
    void            *progress_data;

    /* Upload-specific */
    const char      *upload_file;

    NurlConnPool *pool;
} NurlRequest;

NurlRequest *nurl_request_new(void);
void         nurl_request_from_args(NurlRequest *req, const char *method,
                                    const char *url, const CommonArgs *a);
void         nurl_request_free(NurlRequest *req);

#endif /* NURL_ENGINE_REQUEST_H */
