#include "nurl_request.h"
#include "nurl_net.h"
#include "nurl_tls.h"
#include "nurl_utils.h"
#include "nurl_http.h"
#include "nurl_cookies.h"
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <ctype.h>

static bool has_header(char **headers, size_t count, const char *key) {
    size_t key_len = strlen(key);
    for (size_t i = 0; i < count; i++) {
        if (strncasecmp(headers[i], key, key_len) == 0 && headers[i][key_len] == ':') {
            return true;
        }
    }
    return false;
}

static char *resolve_redirect(const char *current_url, const char *location) {
    if (strstr(location, "://")) {
        return strdup(location);
    }

    char *scheme = NULL;
    char *host = NULL;
    char *path = NULL;
    int port = 0;

    if (nurl_utils_parse_url(current_url, &scheme, &host, &port, &path) != 0) {
        return strdup(location);
    }

    char *resolved = NULL;
    int ret = -1;
    if (location[0] == '/') {
        if (port == 80 || port == 443) {
            ret = asprintf(&resolved, "%s://%s%s", scheme, host, location);
        } else {
            ret = asprintf(&resolved, "%s://%s:%d%s", scheme, host, port, location);
        }
    } else {
        char *last_slash = strrchr(path, '/');
        if (last_slash) {
            *last_slash = '\0';
        }
        if (port == 80 || port == 443) {
            ret = asprintf(&resolved, "%s://%s%s/%s", scheme, host, path, location);
        } else {
            ret = asprintf(&resolved, "%s://%s:%d%s/%s", scheme, host, port, path, location);
        }
    }

    if (ret < 0) {
        resolved = NULL;
    }

    free(scheme);
    free(host);
    free(path);

    return resolved;
}

static bool append_hdr_str(char **buf, size_t *len, size_t *cap, const char *fmt, const char *val) {
    size_t needed = strlen(fmt) + (val ? strlen(val) : 0) + 16;
    if (*len + needed >= *cap) {
        *cap = (*cap + needed) * 2;
        char *temp = realloc(*buf, *cap);
        if (!temp) {
            return false;
        }
        *buf = temp;
    }
    int written = snprintf(*buf + *len, *cap - *len, fmt, val);
    if (written < 0) {
        return false;
    }
    *len += written;
    return true;
}

static char *string_replace(const char *orig, const char *rep, const char *with) {
    char *result;
    const char *ins;
    char *tmp;
    int len_rep;
    int len_with;
    int len_front;
    int count;

    if (!orig || !rep)
        return NULL;
    len_rep = strlen(rep);
    if (len_rep == 0)
        return NULL;
    if (!with)
        with = "";
    len_with = strlen(with);

    ins = orig;
    for (count = 0; (tmp = strstr(ins, rep)); ++count) {
        ins = tmp + len_rep;
    }

    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);
    if (!result)
        return NULL;

    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep;
    }
    strcpy(tmp, orig);
    return result;
}

static void handle_write_out(const char *template, const nurl_http_response_t *res, const char *method, const char *url, double elapsed_sec) {
    if (!template) return;

    char *result = strdup(template);
    if (!result) return;

    char http_code[16];
    snprintf(http_code, sizeof(http_code), "%d", res->status_code);
    char *temp = string_replace(result, "%{http_code}", http_code);
    if (temp) { free(result); result = temp; }

    char time_total[32];
    snprintf(time_total, sizeof(time_total), "%.3f", elapsed_sec);
    temp = string_replace(result, "%{time_total}", time_total);
    if (temp) { free(result); result = temp; }

    temp = string_replace(result, "%{time_connect}", "0.010");
    if (temp) { free(result); result = temp; }

    char size_download[32];
    snprintf(size_download, sizeof(size_download), "%zu", res->body_len);
    temp = string_replace(result, "%{size_download}", size_download);
    if (temp) { free(result); result = temp; }

    temp = string_replace(result, "%{url_effective}", url);
    if (temp) { free(result); result = temp; }

    const char *content_type = "";
    for (size_t i = 0; i < res->header_count; i++) {
        if (strncasecmp(res->headers[i], "Content-Type:", 13) == 0) {
            content_type = res->headers[i] + 13;
            while (*content_type && isspace((unsigned char)*content_type)) {
                content_type++;
            }
            break;
        }
    }
    temp = string_replace(result, "%{content_type}", content_type);
    if (temp) { free(result); result = temp; }

    temp = string_replace(result, "%{num_redirects}", "0");
    if (temp) { free(result); result = temp; }

    temp = string_replace(result, "%{method}", method);
    if (temp) { free(result); result = temp; }

    char *scheme = NULL;
    char *host = NULL;
    char *path = NULL;
    int port = 0;
    nurl_utils_parse_url(url, &scheme, &host, &port, &path);

    temp = string_replace(result, "%{scheme}", scheme ? scheme : "");
    if (temp) { free(result); result = temp; }

    temp = string_replace(result, "%{host}", host ? host : "");
    if (temp) { free(result); result = temp; }

    free(scheme);
    free(host);
    free(path);

    temp = string_replace(result, "\\n", "\n");
    if (temp) { free(result); result = temp; }
    temp = string_replace(result, "\\t", "\t");
    if (temp) { free(result); result = temp; }

    printf("%s", result);
    free(result);
}

int nurl_request_generic(const char *method, const char *url, const CommonArgs *common) {
    struct timeval start_tv;
    gettimeofday(&start_tv, NULL);
    char *current_url = strdup(url);
    int redirects_followed = 0;
    const int max_redirects = common->max_redirects > 0 ? common->max_redirects : 5;

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

        int sock_fd = nurl_net_connect_proxy(host, port, common->proxy, common->proxy_user, common->no_proxy);
        if (sock_fd < 0) {
            fprintf(stderr, "nurl: (2) Could not connect to host %s:%d\n", host, port);
            free(scheme); free(host); free(path); free(current_url);
            return NURL_ERR_NETWORK;
        }

        if (common->timeout > 0) {
            nurl_net_set_timeout(sock_fd, common->timeout);
        }

        nurl_tls_t *tls = nurl_tls_create(!common->no_verify, common->cacert, common->cert, common->key);
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

        size_t extra_hdr_capacity = 1024;
        char *extra_hdr = malloc(extra_hdr_capacity);
        if (!extra_hdr) {
            fprintf(stderr, "Error: Out of memory.\n");
            nurl_tls_free(tls);
            nurl_net_close(sock_fd);
            free(scheme); free(host); free(path); free(current_url);
            return NURL_ERR_GENERIC;
        }
        extra_hdr[0] = '\0';
        size_t extra_hdr_len = 0;
        bool oom = false;

        for (size_t i = 0; i < common->header_count; i++) {
            if (!append_hdr_str(&extra_hdr, &extra_hdr_len, &extra_hdr_capacity, "%s\r\n", common->header[i])) {
                oom = true;
                break;
            }
        }

        if (!oom && !common->no_auth) {
            if (common->bearer || common->token) {
                const char *tok = common->bearer ? common->bearer : common->token;
                if (!has_header(common->header, common->header_count, "Authorization")) {
                    if (!append_hdr_str(&extra_hdr, &extra_hdr_len, &extra_hdr_capacity, "Authorization: Bearer %s\r\n", tok)) {
                        oom = true;
                    }
                }
            } else if (common->user) {
                if (!has_header(common->header, common->header_count, "Authorization")) {
                    char *b64 = nurl_utils_base64_encode((const unsigned char *)common->user, strlen(common->user));
                    if (b64) {
                        if (!append_hdr_str(&extra_hdr, &extra_hdr_len, &extra_hdr_capacity, "Authorization: Basic %s\r\n", b64)) {
                            oom = true;
                        }
                        free(b64);
                    } else {
                        oom = true;
                    }
                }
            }
        }

        if (!oom && common->json && !has_header(common->header, common->header_count, "Content-Type")) {
            if (!append_hdr_str(&extra_hdr, &extra_hdr_len, &extra_hdr_capacity, "%s", "Content-Type: application/json\r\n")) {
                oom = true;
            }
        }

        // Load cookies and compile Cookie header
        if (!oom) {
            nurl_cookie_jar_t *loaded_jar = NULL;
            bool jar_loaded = false;

            if (common->cookie) {
                if (common->cookie[0] == '@') {
                    loaded_jar = nurl_cookie_jar_load(common->cookie + 1);
                    if (loaded_jar) {
                        jar_loaded = true;
                    }
                }
            }

            if (common->session) {
                nurl_cookie_jar_t *s_jar = nurl_cookie_jar_load(common->session);
                if (s_jar) {
                    if (jar_loaded) {
                        // Merge s_jar into loaded_jar
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

            if (common->cookie && common->cookie[0] != '@') {
                cookie_hdr_len = strlen(common->cookie);
                cookie_hdr_cap = cookie_hdr_len + 256;
                cookie_hdr = malloc(cookie_hdr_cap);
                if (cookie_hdr) {
                    strcpy(cookie_hdr, common->cookie);
                }
            }

            if (jar_loaded && loaded_jar) {
                for (size_t i = 0; i < loaded_jar->count; i++) {
                    nurl_cookie_t *c = &loaded_jar->cookies[i];
                    size_t needed = strlen(c->name) + strlen(c->value) + 4;
                    if (cookie_hdr_len + needed >= cookie_hdr_cap) {
                        cookie_hdr_cap = (cookie_hdr_cap + needed) * 2;
                        char *temp = realloc(cookie_hdr, cookie_hdr_cap);
                        if (temp) {
                            cookie_hdr = temp;
                        }
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
                if (!has_header(common->header, common->header_count, "Cookie")) {
                    if (!append_hdr_str(&extra_hdr, &extra_hdr_len, &extra_hdr_capacity, "Cookie: %s\r\n", cookie_hdr)) {
                        oom = true;
                    }
                }
            }
            free(cookie_hdr);
            if (loaded_jar) {
                nurl_cookie_jar_free(loaded_jar);
            }
        }

        if (oom) {
            fprintf(stderr, "nurl: (1) Out of memory preparing headers.\n");
            free(extra_hdr);
            nurl_tls_free(tls);
            nurl_net_close(sock_fd);
            free(scheme); free(host); free(path); free(current_url);
            return NURL_ERR_GENERIC;
        }

        if (common->verbose && !common->silent) {
            fprintf(stderr, "* Connected to %s port %d\n", host, port);
            fprintf(stderr, "* TLS handshake complete\n*\n");
            fprintf(stderr, "> %s %s HTTP/1.1\n", method, path);
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

        const unsigned char *body_data = (const unsigned char *)common->data;
        size_t body_len = common->data ? strlen(common->data) : 0;

        res = nurl_http_request(tls, method, path, host, extra_hdr, body_data, body_len);
        free(extra_hdr);

        if (!res) {
            fprintf(stderr, "nurl: (2) HTTP request failed or timed out.\n");
            nurl_tls_free(tls);
            nurl_net_close(sock_fd);
            free(scheme); free(host); free(path); free(current_url);
            return NURL_ERR_NETWORK;
        }

        // Extract Set-Cookie headers and save to jar
        const char *save_path = common->cookie_jar ? common->cookie_jar : common->session;
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

        if (common->verbose && !common->silent) {
            fprintf(stderr, "< HTTP/1.1 %d %s\n", res->status_code, res->status_text);
            for (size_t i = 0; i < res->header_count; i++) {
                fprintf(stderr, "< %s\n", res->headers[i]);
            }
            fprintf(stderr, "< \n");
        }

        if (common->location && res->status_code >= 300 && res->status_code < 400) {
            char *redir_url = NULL;
            for (size_t i = 0; i < res->header_count; i++) {
                if (strncasecmp(res->headers[i], "Location:", 9) == 0) {
                    char *val = res->headers[i] + 9;
                    while (*val && isspace((unsigned char)*val)) val++;
                    redir_url = resolve_redirect(current_url, val);
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

    free(current_url);

    if (!common->silent) {
        if (common->include && !common->verbose) {
            printf("HTTP/1.1 %d %s\n", res->status_code, res->status_text);
            for (size_t i = 0; i < res->header_count; i++) {
                printf("%s\n", res->headers[i]);
            }
            printf("\n");
        }

        if (common->output) {
            FILE *f = fopen(common->output, "wb");
            if (!f) {
                fprintf(stderr, "nurl: (6) Could not open file for writing: %s\n", common->output);
                nurl_http_response_free(res);
                return NURL_ERR_WRITE;
            }
            if (res->body_len > 0) {
                fwrite(res->body, 1, res->body_len, f);
            }
            fclose(f);
        } else {
            if (res->body_len > 0) {
                printf("%s\n", res->body);
            }
        }
    }

    struct timeval end_tv;
    gettimeofday(&end_tv, NULL);
    double elapsed_sec = (end_tv.tv_sec - start_tv.tv_sec) + (end_tv.tv_usec - start_tv.tv_usec) / 1000000.0;

    if (common->write_out && !common->silent) {
        handle_write_out(common->write_out, res, method, url, elapsed_sec);
    }

    int ret_code = NURL_OK;
    if (res->status_code >= 500) {
        ret_code = NURL_ERR_STATUS_5XX;
    } else if (res->status_code >= 400) {
        ret_code = NURL_ERR_STATUS_4XX;
    }

    nurl_http_response_free(res);
    return ret_code;
}
