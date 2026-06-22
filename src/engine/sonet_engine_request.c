#include "sonet_engine_request.h"
#include "sonet_multipart.h"
#include "sonet_buf.h"
#include "utils/sonet_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

SonetRequest *sonet_request_new(void) {
    SonetRequest *req = calloc(1, sizeof(SonetRequest));
    if (!req) return NULL;
    req->max_redirects = 10;
    return req;
}

sonet_err_t sonet_headermap_apply_auth(SonetHeaderMap *m, const CommonArgs *a) {
    if (!m || !a) return SONET_ERR_GENERIC;
    if (a->no_auth) return SONET_OK;

    if (sonet_headermap_has(m, "Authorization")) {
        return SONET_OK;
    }

    if (a->bearer || a->token) {
        const char *tok = a->bearer ? a->bearer : a->token;
        SonetBuf auth_val;
        sonet_buf_init(&auth_val);
        if (!sonet_buf_printf(&auth_val, "Bearer %s", tok)) {
            sonet_buf_free(&auth_val);
            return SONET_ERR_OOM;
        }
        sonet_err_t err = sonet_headermap_set(m, "Authorization", auth_val.data);
        sonet_buf_free(&auth_val);
        return err;
    } else if (a->user) {
        char *b64 = sonet_utils_base64_encode((const unsigned char *)a->user, strlen(a->user));
        if (!b64) return SONET_ERR_OOM;
        
        SonetBuf auth_val;
        sonet_buf_init(&auth_val);
        if (!sonet_buf_printf(&auth_val, "Basic %s", b64)) {
            free(b64);
            sonet_buf_free(&auth_val);
            return SONET_ERR_OOM;
        }
        free(b64);
        sonet_err_t err = sonet_headermap_set(m, "Authorization", auth_val.data);
        sonet_buf_free(&auth_val);
        return err;
    }

    return SONET_OK;
}

sonet_err_t sonet_headermap_apply_common(SonetHeaderMap *m, const CommonArgs *a) {
    if (!m || !a) return SONET_ERR_GENERIC;

    if (a->user_agent && !sonet_headermap_has(m, "User-Agent")) {
        sonet_err_t err = sonet_headermap_set(m, "User-Agent", a->user_agent);
        if (err != SONET_OK) return err;
    }
    if (a->referer && !sonet_headermap_has(m, "Referer")) {
        sonet_err_t err = sonet_headermap_set(m, "Referer", a->referer);
        if (err != SONET_OK) return err;
    }
    if (a->cookie && a->cookie[0] != '@' && !sonet_headermap_has(m, "Cookie")) {
        sonet_err_t err = sonet_headermap_set(m, "Cookie", a->cookie);
        if (err != SONET_OK) return err;
    }

    return SONET_OK;
}

void sonet_request_from_args(SonetRequest *req, const char *method, const char *url, const CommonArgs *a) {
    if (!req || !a) return;

    req->method = method;
    req->url = url;

    // Headers Map
    req->headers = sonet_headermap_new();
    if (req->headers) {
        for (size_t i = 0; i < a->header_count; i++) {
            sonet_headermap_add_raw(req->headers, a->header[i]);
        }

        if (a->upload_file || a->upload_fields_count > 0) {
            SonetMultipart *m = sonet_multipart_new();
            if (m) {
                if (a->upload_file) {
                    sonet_multipart_add_file(m, a->upload_name ? a->upload_name : "file", a->upload_file, a->upload_mime);
                }
                for (size_t i = 0; i < a->upload_fields_count; i++) {
                    char *field = strdup(a->upload_fields[i]);
                    char *eq = strchr(field, '=');
                    if (eq) {
                        *eq = '\0';
                        sonet_multipart_add_field(m, field, eq + 1);
                    }
                    free(field);
                }
                sonet_multipart_into_request(m, req);
                sonet_multipart_free(m);
            }
        }

        sonet_headermap_apply_auth(req->headers, a);
        sonet_headermap_apply_common(req->headers, a);
        if (a->json && !sonet_headermap_has(req->headers, "Content-Type")) {
            sonet_headermap_set(req->headers, "Content-Type", "application/json");
        }
        if (a->compressed && !sonet_headermap_has(req->headers, "Accept-Encoding")) {
            sonet_headermap_set(req->headers, "Accept-Encoding", "gzip, deflate");
        }
    }

    req->body = (const uint8_t *)a->data;
    req->body_len = a->data_len;
    req->body_is_stream = false;

    req->read_timeout_sec = (unsigned int)a->timeout;
    req->connect_timeout_sec = (unsigned int)a->connect_timeout;
    req->follow_redirect = a->location;
    req->max_redirects = a->is_set.max_redirects ? a->max_redirects : 10;
    req->retry_count = a->retry;
    req->retry_delay_sec = a->retry_delay;
    req->limit_rate = a->limit_rate;
    req->fail_on_error = a->fail;
    req->fail_with_body = a->fail_with_body;

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
    req->connect_to = a->connect_to;

    req->cookie = a->cookie;
    req->cookie_jar = a->cookie_jar;
    req->session = a->session;

    req->include_headers = a->include;
    req->verbose = a->verbose;
    req->silent = a->silent;
    req->raw_output = a->raw;
    req->decompress = a->compressed;
    req->http10 = a->http10;

    req->resume = a->resume;
    req->progress = a->progress;
}

void sonet_request_free(SonetRequest *req) {
    if (!req) return;
    if (req->headers) {
        sonet_headermap_free(req->headers);
    }
    if (req->body_parts) {
        for (size_t i = 0; i < req->body_parts_count; i++) {
            if (req->body_parts[i].type == SONET_BODY_PART_MEM) {
                free((void *)req->body_parts[i].data);
            } else if (req->body_parts[i].type == SONET_BODY_PART_FILE) {
                free((void *)req->body_parts[i].filepath);
            }
        }
        free(req->body_parts);
    }
    free(req);
}
