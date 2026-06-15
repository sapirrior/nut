#include "nurl_engine.h"
#include "nurl_net.h"
#include "nurl_tls.h"
#include "nurl_utils.h"
#include "nurl_http.h"
#include "nurl_cookies.h"
#include "nurl_decompress.h"
#include "nurl_redirect.h"
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <ctype.h>

int nurl_engine_execute_request(
    NurlRequest *req,
    nurl_http_response_t **out_response,
    char **out_effective_url
) {
    char *current_url = strdup(req->url);
    int redirects_followed = 0;
    const int max_redirects = req->max_redirects > 0 ? req->max_redirects : 5;

    nurl_http_response_t *res = NULL;

    while (true) {
        char *scheme = NULL;
        char *host = NULL;
        char *path = NULL;
        int port = 0;

        if (nurl_utils_parse_url(current_url, &scheme, &host, &port, &path) != 0) {
            fprintf(stderr, "nurl: (4) Malformed URL: %s\n", current_url);
            free(current_url);
            return NURL_ERR_INVALID_URL;
        }

        bool use_tls = (strcmp(scheme, "https") == 0);
        if (!use_tls) {
            fprintf(stderr, "nurl: (5) nurl currently only supports HTTPS (TLS) requests.\n");
            free(scheme); free(host); free(path); free(current_url);
            return NURL_ERR_TLS;
        }

        int sock_fd = nurl_net_connect_proxy(host, port, req->proxy, req->proxy_user, req->no_proxy);
        if (sock_fd < 0) {
            fprintf(stderr, "nurl: (2) Could not connect to host %s:%d\n", host, port);
            free(scheme); free(host); free(path); free(current_url);
            return NURL_ERR_NETWORK;
        }

        if (req->timeout_sec > 0) {
            nurl_net_set_timeout(sock_fd, req->timeout_sec);
        }

        nurl_tls_t *tls = nurl_tls_create(req->tls_verify, req->cacert, req->cert, req->key, req->tls_version == 12, req->tls_version == 13);
        if (!tls) {
            fprintf(stderr, "nurl: (5) Failed to initialize TLS context.\n");
            nurl_net_close(sock_fd);
            free(scheme); free(host); free(path); free(current_url);
            return NURL_ERR_TLS;
        }

        if (nurl_tls_handshake(tls, sock_fd, host) != 0) {
            fprintf(stderr, "nurl: (5) TLS verification failed.\n");
            nurl_tls_free(tls);
            nurl_net_close(sock_fd);
            free(scheme); free(host); free(path); free(current_url);
            return NURL_ERR_TLS;
        }

        NurlHeaderList *temp_hdrs = nurl_headers_new();
        if (!temp_hdrs) {
            nurl_tls_free(tls);
            nurl_net_close(sock_fd);
            free(scheme); free(host); free(path); free(current_url);
            return NURL_ERR_OOM;
        }

        // Copy base headers
        for (size_t i = 0; i < req->headers->count; i++) {
            nurl_headers_add_raw(temp_hdrs, req->headers->entries[i]);
        }

        if (req->resume_offset > 0 && !nurl_headers_has(temp_hdrs, "Range")) {
            char range_val[64];
            snprintf(range_val, sizeof(range_val), "bytes=%lu-", req->resume_offset);
            nurl_headers_add(temp_hdrs, "Range", range_val);
        }

        // Dynamic Cookie compilation
        nurl_cookie_jar_t *loaded_jar = NULL;
        bool jar_loaded = false;

        if (req->cookie) {
            if (req->cookie[0] == '@') {
                loaded_jar = nurl_cookie_jar_load(req->cookie + 1);
                if (loaded_jar) jar_loaded = true;
            }
        }

        if (req->session) {
            nurl_cookie_jar_t *s_jar = nurl_cookie_jar_load(req->session);
            if (s_jar) {
                if (jar_loaded) {
                    for (size_t i = 0; i < s_jar->count; i++) {
                        nurl_cookie_jar_add(loaded_jar, &s_jar->cookies[i]);
                    }
                    nurl_cookie_jar_free(s_jar);
                } else {
                    loaded_jar = s_jar;
                    jar_loaded = true;
                }
            }
        }

        char *cookie_hdr = NULL;
        size_t cookie_hdr_len = 0;
        size_t cookie_hdr_cap = 0;

        if (req->cookie && req->cookie[0] != '@') {
            cookie_hdr_len = strlen(req->cookie);
            cookie_hdr_cap = cookie_hdr_len + 256;
            cookie_hdr = malloc(cookie_hdr_cap);
            if (cookie_hdr) strcpy(cookie_hdr, req->cookie);
        }

        if (jar_loaded && loaded_jar) {
            for (size_t i = 0; i < loaded_jar->count; i++) {
                nurl_cookie_t *c = &loaded_jar->cookies[i];
                size_t needed = strlen(c->name) + strlen(c->value) + 4;
                if (cookie_hdr_len + needed >= cookie_hdr_cap) {
                    cookie_hdr_cap = (cookie_hdr_cap + needed) * 2;
                    char *temp = realloc(cookie_hdr, cookie_hdr_cap);
                    if (temp) cookie_hdr = temp;
                }
                if (cookie_hdr) {
                    if (cookie_hdr_len > 0) {
                        strcat(cookie_hdr, "; ");
                        cookie_hdr_len += 2;
                    }
                    strcat(cookie_hdr, c->name);
                    strcat(cookie_hdr, "=");
                    strcat(cookie_hdr, c->value);
                    cookie_hdr_len += strlen(c->name) + 1 + strlen(c->value);
                }
            }
        }

        if (cookie_hdr && cookie_hdr_len > 0) {
            if (!nurl_headers_has(temp_hdrs, "Cookie")) {
                nurl_headers_add(temp_hdrs, "Cookie", cookie_hdr);
            }
        }
        free(cookie_hdr);
        if (loaded_jar) nurl_cookie_jar_free(loaded_jar);

        char *extra_hdr = nurl_headers_serialize(temp_hdrs);
        nurl_headers_free(temp_hdrs);

        if (!extra_hdr) {
            nurl_tls_free(tls);
            nurl_net_close(sock_fd);
            free(scheme); free(host); free(path); free(current_url);
            return NURL_ERR_OOM;
        }

        if (req->verbose && !req->silent) {
            fprintf(stderr, "* Connected to %s port %d\n", host, port);
            fprintf(stderr, "* TLS handshake complete\n*\n");
            fprintf(stderr, "> %s %s HTTP/1.1\n", req->method, path);
            fprintf(stderr, "> Host: %s\n", host);
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

        res = nurl_http_request(tls, req->method, path, host, extra_hdr, req->body, req->body_len, req->out, req->progress, req->silent, req->resume_offset);
        free(extra_hdr);

        if (!res) {
            fprintf(stderr, "nurl: (2) HTTP request failed or timed out.\n");
            nurl_tls_free(tls);
            nurl_net_close(sock_fd);
            free(scheme); free(host); free(path); free(current_url);
            return NURL_ERR_NETWORK;
        }

        // Handle decompression if Accept-Encoding was requested
        if (req->decompress && res && res->body_len > 0) {
            bool is_compressed = false;
            for (size_t i = 0; i < res->header_count; i++) {
                if (strncasecmp(res->headers[i], "Content-Encoding:", 17) == 0) {
                    char *encoding = res->headers[i] + 17;
                    while (*encoding && isspace((unsigned char)*encoding)) encoding++;
                    if (strcasecmp(encoding, "gzip") == 0 || strcasecmp(encoding, "deflate") == 0) {
                        is_compressed = true;
                        break;
                    }
                }
            }
            if (is_compressed) {
                size_t decompressed_len = 0;
                unsigned char *decompressed = nurl_decompress_gzip_deflate(
                    (const unsigned char *)res->body, res->body_len, &decompressed_len
                );
                if (decompressed) {
                    free(res->body);
                    res->body = decompressed;
                    res->body_len = decompressed_len;
                } else {
                    fprintf(stderr, "nurl: Failed to decompress response payload.\n");
                }
            }
        }

        // Extract Set-Cookie headers and save to jar
        const char *save_path = req->cookie_jar ? req->cookie_jar : req->session;
        if (save_path && res) {
            nurl_cookie_jar_t *save_jar = nurl_cookie_jar_load(save_path);
            if (!save_jar) {
                save_jar = nurl_cookie_jar_create();
            }

            if (save_jar) {
                for (size_t i = 0; i < res->header_count; i++) {
                    if (strncasecmp(res->headers[i], "Set-Cookie:", 11) == 0) {
                        char *val = strdup(res->headers[i] + 11);
                        if (!val) continue;

                        char *parts[32];
                        size_t part_count = 0;
                        char *tok = val;
                        while (part_count < 32) {
                            char *semi = strchr(tok, ';');
                            if (semi) {
                                *semi = '\0';
                                parts[part_count++] = tok;
                                tok = semi + 1;
                            } else {
                                parts[part_count++] = tok;
                                break;
                            }
                        }

                        if (part_count > 0) {
                            char *eq = strchr(parts[0], '=');
                            if (eq) {
                                *eq = '\0';
                                char *name = nurl_utils_trim(parts[0]);
                                char *value = nurl_utils_trim(eq + 1);
                                char *domain = NULL;
                                char *cookie_path = NULL;
                                bool secure = false;

                                for (size_t p = 1; p < part_count; p++) {
                                    char *attr = parts[p];
                                    char *attr_eq = strchr(attr, '=');
                                    if (attr_eq) {
                                        *attr_eq = '\0';
                                        char *k_attr = nurl_utils_trim(attr);
                                        char *v_attr = nurl_utils_trim(attr_eq + 1);
                                        if (strcasecmp(k_attr, "domain") == 0) {
                                            domain = strdup(v_attr);
                                        } else if (strcasecmp(k_attr, "path") == 0) {
                                            cookie_path = strdup(v_attr);
                                        }
                                    } else {
                                        char *k_attr = nurl_utils_trim(attr);
                                        if (strcasecmp(k_attr, "secure") == 0) {
                                            secure = true;
                                        }
                                    }
                                }

                                if (!domain) {
                                    domain = strdup(host);
                                }
                                if (!cookie_path) {
                                    cookie_path = strdup("/");
                                }

                                nurl_cookie_t c;
                                c.domain = domain;
                                c.include_subdomains = true;
                                c.path = cookie_path;
                                c.secure = secure;
                                c.expiry = 0;
                                c.name = name;
                                c.value = value;

                                nurl_cookie_jar_add(save_jar, &c);

                                free(domain);
                                free(cookie_path);
                            }
                        }
                        free(val);
                    }
                }
                nurl_cookie_jar_save(save_jar, save_path);
                nurl_cookie_jar_free(save_jar);
            }
        }

        if (req->verbose && !req->silent) {
            fprintf(stderr, "< HTTP/1.1 %d %s\n", res->status_code, res->status_text);
            for (size_t i = 0; i < res->header_count; i++) {
                fprintf(stderr, "< %s\n", res->headers[i]);
            }
            fprintf(stderr, "< \n");
        }

        if (req->follow_redirect && res->status_code >= 300 && res->status_code < 400) {
            char *redir_url = NULL;
            for (size_t i = 0; i < res->header_count; i++) {
                if (strncasecmp(res->headers[i], "Location:", 9) == 0) {
                    char *val = res->headers[i] + 9;
                    while (*val && isspace((unsigned char)*val)) val++;
                    redir_url = nurl_resolve_redirect(current_url, val);
                    break;
                }
            }

            if (redir_url) {
                nurl_http_response_free(res);
                nurl_tls_free(tls);
                nurl_net_close(sock_fd);
                free(scheme); free(host); free(path);

                free(current_url);
                current_url = redir_url;
                redirects_followed++;

                if (redirects_followed >= max_redirects) {
                    fprintf(stderr, "Error: Maximum redirect limit exceeded (%d).\n", max_redirects);
                    free(current_url);
                    return NURL_ERR_GENERIC;
                }
                continue;
            }
        }

        nurl_tls_free(tls);
        nurl_net_close(sock_fd);
        free(scheme); free(host); free(path);
        break;
    }

    *out_response = res;
    if (out_effective_url) {
        *out_effective_url = current_url;
    } else {
        free(current_url);
    }

    return NURL_OK;
}
