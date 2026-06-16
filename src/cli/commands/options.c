#include "commands.h"
#include "nurl_net.h"
#include "nurl_tls.h"
#include "nurl_utils.h"
#include "nurl_http.h"
#include "errors/nurl_error_handler.h"
#include "errors/nurl_diag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <ctype.h>

int nurl_cmd_options(const char *url, const CommonArgs *common) {
    char *scheme = NULL;
    char *host = NULL;
    char *path = NULL;
    int port = 0;

    if (nurl_utils_parse_url(url, &scheme, &host, &port, &path) != 0) {
        nurl_diag_err("malformed URL '%s' provided for OPTIONS request.", url);
        nurl_diag_hint("ensure the URL uses a supported scheme like 'https://' and has a valid hostname.");
        return NURL_ERR_INVALID_URL;
    }

    int sock_fd = nurl_net_connect_proxy(host, port, common->proxy, common->proxy_user, common->no_proxy);
    if (sock_fd < 0) {
        if (!common->silent) {
            nurl_handle_request_error(NURL_ERR_NETWORK, NULL, url);
        }
        free(scheme); free(host); free(path);
        return NURL_ERR_NETWORK;
    }

    if (common->timeout > 0) {
        nurl_net_set_timeout(sock_fd, common->timeout);
    }

    nurl_tls_t *tls = nurl_tls_create(!common->no_verify, common->cacert, common->cert, common->key, common->tls12, common->tls13);
    if (!tls) {
        if (!common->silent) {
            nurl_handle_request_error(NURL_ERR_TLS, NULL, url);
        }
        nurl_net_close(sock_fd);
        free(scheme); free(host); free(path);
        return NURL_ERR_TLS;
    }

    if (common->verbose && !common->silent) {
        fprintf(stderr, "* Connected to %s port %d\n", host, port);
        fprintf(stderr, "* TLS handshake complete\n*\n");
    }

    if (nurl_tls_handshake(tls, sock_fd, host) != 0) {
        if (!common->silent) {
            nurl_handle_request_error(NURL_ERR_TLS, NULL, url);
        }
        nurl_tls_free(tls);
        nurl_net_close(sock_fd);
        free(scheme); free(host); free(path);
        return NURL_ERR_TLS;
    }

    NurlStream *stream = nurl_stream_new(sock_fd, tls);
    if (!stream) {
        nurl_tls_free(tls);
        nurl_net_close(sock_fd);
        free(scheme); free(host); free(path);
        return NURL_ERR_OOM;
    }

    size_t extra_hdr_cap = 1024;
    char *extra_hdr = malloc(extra_hdr_cap);
    if (!extra_hdr) {
        nurl_tls_free(tls);
        nurl_net_close(sock_fd);
        free(scheme); free(host); free(path);
        return NURL_ERR_GENERIC;
    }
    extra_hdr[0] = '\0';
    size_t extra_hdr_len = 0;
    bool oom = false;

    for (size_t i = 0; i < common->header_count; i++) {
        if (!nurl_utils_append_hdr_str(&extra_hdr, &extra_hdr_len, &extra_hdr_cap, "%s\r\n", common->header[i])) {
            oom = true; break;
        }
    }

    if (!oom && !common->no_auth) {
        if (common->bearer || common->token) {
            const char *tok = common->bearer ? common->bearer : common->token;
            if (!nurl_utils_has_header(common->header, common->header_count, "Authorization")) {
                if (!nurl_utils_append_hdr_str(&extra_hdr, &extra_hdr_len, &extra_hdr_cap, "Authorization: Bearer %s\r\n", tok)) {
                    oom = true;
                }
            }
        } else if (common->user) {
            if (!nurl_utils_has_header(common->header, common->header_count, "Authorization")) {
                char *b64 = nurl_utils_base64_encode((const unsigned char *)common->user, strlen(common->user));
                if (b64) {
                    if (!nurl_utils_append_hdr_str(&extra_hdr, &extra_hdr_len, &extra_hdr_cap, "Authorization: Basic %s\r\n", b64)) {
                        oom = true;
                    }
                    free(b64);
                } else {
                    oom = true;
                }
            }
        }
    }

    if (!oom && common->user_agent) {
        if (!nurl_utils_has_header(common->header, common->header_count, "User-Agent")) {
            if (!nurl_utils_append_hdr_str(&extra_hdr, &extra_hdr_len, &extra_hdr_cap, "User-Agent: %s\r\n", common->user_agent)) {
                oom = true;
            }
        }
    }

    if (oom) {
        fprintf(stderr, "nurl: (1) Out of memory preparing headers.\n");
        free(extra_hdr);
        nurl_tls_free(tls);
        nurl_net_close(sock_fd);
        free(scheme); free(host); free(path);
        return NURL_ERR_GENERIC;
    }

    if (common->verbose && !common->silent) {
        fprintf(stderr, "> OPTIONS %s HTTP/1.1\n", path);
        fprintf(stderr, "> Host: %s\n", host);
        fprintf(stderr, "> User-Agent: nurl/" NURL_VERSION "\n");
        fprintf(stderr, "> Connection: close\n");

        char *hdr_copy = strdup(extra_hdr);
        char *line = strtok(hdr_copy, "\r\n");
        while (line) {
            char *colon = strchr(line, ':');
            if (colon) {
                *colon = '\0';
                char *key = line;
                char *val = colon + 1;
                while (*val && isspace((unsigned char)*val)) val++;
                const char *redacted = nurl_utils_redact_header(key, val);
                fprintf(stderr, "> %s: %s\n", key, redacted);
            }
            line = strtok(NULL, "\r\n");
        }
        free(hdr_copy);
        fprintf(stderr, "> \n");
    }

    nurl_http_response_t *res = NULL;
    nurl_err_t exec_err = nurl_http_request(stream, "OPTIONS", path, host, extra_hdr, NULL, 0, NULL, 0, NULL, false, true, 0, NULL, NULL, &res);
    free(extra_hdr);

    if (exec_err != NURL_OK) {
        if (!common->silent) {
            nurl_handle_request_error(exec_err, NULL, url);
        }
        nurl_tls_free(tls);
        nurl_net_close(sock_fd);
        nurl_stream_free(stream);
        free(scheme); free(host); free(path);
        return exec_err;
    }

    if (common->verbose && !common->silent) {
        fprintf(stderr, "< HTTP/1.1 %d %s\n", res->status_code, res->status_text);
        for (size_t i = 0; i < res->header_count; i++) {
            fprintf(stderr, "< %s\n", res->headers[i]);
        }
        fprintf(stderr, "< \n");
    }

    if (!common->silent) {
        for (size_t i = 0; i < res->header_count; i++) {
            char *hdr_line = res->headers[i];
            char *colon = strchr(hdr_line, ':');
            if (colon) {
                *colon = '\0';
                char *key = hdr_line;
                char *val = colon + 1;
                while (*val && isspace((unsigned char)*val)) val++;

                if (strcasecmp(key, "Allow") == 0 || strncasecmp(key, "Access-Control-", 15) == 0) {
                    printf("%s: %s\n", key, val);
                }
                *colon = ':'; // Restore
            }
        }
    }

    nurl_tls_free(tls);
    nurl_net_close(sock_fd);
    nurl_stream_free(stream);
    free(scheme); free(host); free(path);

    int ret_code = NURL_OK;
    if (res->status_code >= 500) {
        ret_code = NURL_ERR_STATUS_5XX;
    } else if (res->status_code >= 400) {
        ret_code = NURL_ERR_STATUS_4XX;
    }

    nurl_http_response_free(res);
    return ret_code;
}
