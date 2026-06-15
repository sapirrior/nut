#include "request.h"
#include <stdlib.h>
#include <string.h>

NurlRequest *nurl_request_new(void) {
    NurlRequest *req = calloc(1, sizeof(NurlRequest));
    if (!req) return NULL;
    req->max_redirects = 10;
    return req;
}

void nurl_request_from_args(NurlRequest *req, const char *method, const char *url, const BaseArgs *a) {
    if (!req || !a) return;

    req->method = method;
    req->url = url;

    // Headers List
    req->headers = nurl_headers_new();
    if (req->headers) {
        for (size_t i = 0; i < a->header_count; i++) {
            nurl_headers_add_raw(req->headers, a->header[i]);
        }
        nurl_headers_apply_auth(req->headers, a);
        nurl_headers_apply_common(req->headers, a);
        if (a->compressed && !nurl_headers_has(req->headers, "Accept-Encoding")) {
            nurl_headers_add(req->headers, "Accept-Encoding", "gzip, deflate");
        }
    }

    req->body = (const uint8_t *)a->data;
    req->body_len = a->data_len;
    req->body_is_stream = a->data_is_stdin;

    req->timeout_sec = a->timeout;
    req->follow_redirect = a->location;
    req->retry_count = a->retry;
    req->retry_delay_sec = a->retry_delay;
    req->fail_on_error = a->fail;

    req->tls_verify = !a->no_verify;
    req->cacert = a->cacert;
    req->cert = a->cert;
    req->key = a->key;

    if (a->tls12) {
        req->tls_version = 12;
    } else if (a->tls13) {
        req->tls_version = 13;
    } else {
        req->tls_version = 0;
    }

    req->proxy = a->proxy;
    req->proxy_user = a->proxy_user;
    req->no_proxy = a->no_proxy;

    req->cookie = a->cookie;
    req->cookie_jar = a->cookie_jar;
    req->session = a->session;

    req->include_headers = a->include;
    req->verbose = a->verbose;
    req->silent = a->silent;
    req->raw_output = a->raw;
    req->decompress = a->compressed;
}

void nurl_request_free(NurlRequest *req) {
    if (!req) return;
    if (req->headers) {
        nurl_headers_free(req->headers);
    }
    free(req);
}
