#include "commands.h"
#include "nurl_net.h"
#include "nurl_tls.h"
#include "nurl_utils.h"
#include "nurl_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <ctype.h>

static bool append_hdr_str(char **buf, size_t *len, size_t *cap, const char *fmt, const char *val) {
    size_t needed = strlen(fmt) + (val ? strlen(val) : 0) + 16;
    if (*len + needed >= *cap) {
        *cap = (*cap + needed) * 2;
        char *temp = realloc(*buf, *cap);
        if (!temp) return false;
        *buf = temp;
    }
    int written = snprintf(*buf + *len, *cap - *len, fmt, val);
    if (written < 0) return false;
    *len += written;
    return true;
}

static bool has_header(char **headers, size_t count, const char *key) {
    size_t key_len = strlen(key);
    for (size_t i = 0; i < count; i++) {
        if (strncasecmp(headers[i], key, key_len) == 0 && headers[i][key_len] == ':') {
            return true;
        }
    }
    return false;
}

int nurl_cmd_upload(const char *url, const CommonArgs *common) {
    if (!common->upload_file) {
        fprintf(stderr, "nurl: (1) No upload file specified.\n");
        return NURL_ERR_GENERIC;
    }

    FILE *f = fopen(common->upload_file, "rb");
    if (!f) {
        fprintf(stderr, "nurl: (6) Could not read upload file '%s'\n", common->upload_file);
        return NURL_ERR_WRITE;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    unsigned char *file_bytes = malloc(fsize > 0 ? fsize : 1);
    if (!file_bytes) {
        fclose(f);
        return NURL_ERR_GENERIC;
    }

    if (fsize > 0) {
        size_t read_bytes = fread(file_bytes, 1, fsize, f);
        if (read_bytes != (size_t)fsize) {
            fprintf(stderr, "nurl: (6) Failed to read file bytes.\n");
            free(file_bytes);
            fclose(f);
            return NURL_ERR_WRITE;
        }
    }
    fclose(f);

    const char *file_name = strrchr(common->upload_file, '/');
    if (file_name) {
        file_name++;
    } else {
        file_name = common->upload_file;
    }

    const char *mime_type = common->upload_mime;
    if (!mime_type) {
        const char *ext = strrchr(file_name, '.');
        if (ext) {
            ext++; // skip '.'
            if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0) {
                mime_type = "image/jpeg";
            } else if (strcasecmp(ext, "png") == 0) {
                mime_type = "image/png";
            } else if (strcasecmp(ext, "gif") == 0) {
                mime_type = "image/gif";
            } else if (strcasecmp(ext, "pdf") == 0) {
                mime_type = "application/pdf";
            } else if (strcasecmp(ext, "txt") == 0) {
                mime_type = "text/plain";
            } else if (strcasecmp(ext, "json") == 0) {
                mime_type = "application/json";
            } else if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0) {
                mime_type = "text/html";
            } else {
                mime_type = "application/octet-stream";
            }
        } else {
            mime_type = "application/octet-stream";
        }
    }

    const char *boundary = "------------------------nurlboundary1234567890";

    // Allocate multipart body buffer
    size_t body_cap = 4096 + fsize;
    for (size_t i = 0; i < common->upload_fields_count; i++) {
        body_cap += strlen(common->upload_fields[i]) + 256;
    }
    unsigned char *body = malloc(body_cap);
    if (!body) {
        free(file_bytes);
        return NURL_ERR_GENERIC;
    }
    size_t body_len = 0;

    // 1. Add form fields
    for (size_t i = 0; i < common->upload_fields_count; i++) {
        char *field_copy = strdup(common->upload_fields[i]);
        if (!field_copy) continue;
        char *eq = strchr(field_copy, '=');
        if (eq) {
            *eq = '\0';
            char *k = nurl_utils_trim(field_copy);
            char *v = nurl_utils_trim(eq + 1);

            body_len += snprintf((char *)body + body_len, body_cap - body_len, "--%s\r\n", boundary);
            body_len += snprintf((char *)body + body_len, body_cap - body_len, "Content-Disposition: form-data; name=\"%s\"\r\n\r\n", k);
            body_len += snprintf((char *)body + body_len, body_cap - body_len, "%s\r\n", v);
        }
        free(field_copy);
    }

    // 2. Add file part
    body_len += snprintf((char *)body + body_len, body_cap - body_len, "--%s\r\n", boundary);
    body_len += snprintf((char *)body + body_len, body_cap - body_len,
        "Content-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n",
        common->upload_name ? common->upload_name : "file", file_name);
    body_len += snprintf((char *)body + body_len, body_cap - body_len, "Content-Type: %s\r\n\r\n", mime_type);

    if (fsize > 0) {
        memcpy(body + body_len, file_bytes, fsize);
        body_len += fsize;
    }
    memcpy(body + body_len, "\r\n", 2);
    body_len += 2;

    // 3. Close boundary
    body_len += snprintf((char *)body + body_len, body_cap - body_len, "--%s--\r\n", boundary);

    free(file_bytes);

    // Prepare extra headers
    size_t extra_hdr_cap = 1024;
    char *extra_hdr = malloc(extra_hdr_cap);
    if (!extra_hdr) {
        free(body);
        return NURL_ERR_GENERIC;
    }
    extra_hdr[0] = '\0';
    size_t extra_hdr_len = 0;
    bool oom = false;

    for (size_t i = 0; i < common->header_count; i++) {
        if (!append_hdr_str(&extra_hdr, &extra_hdr_len, &extra_hdr_cap, "%s\r\n", common->header[i])) {
            oom = true; break;
        }
    }

    if (!oom) {
        char ct_hdr[256];
        snprintf(ct_hdr, sizeof(ct_hdr), "Content-Type: multipart/form-data; boundary=%s\r\n", boundary);
        if (!append_hdr_str(&extra_hdr, &extra_hdr_len, &extra_hdr_cap, "%s", ct_hdr)) {
            oom = true;
        }
    }

    if (!oom && !common->no_auth) {
        if (common->bearer || common->token) {
            const char *tok = common->bearer ? common->bearer : common->token;
            if (!has_header(common->header, common->header_count, "Authorization")) {
                if (!append_hdr_str(&extra_hdr, &extra_hdr_len, &extra_hdr_cap, "Authorization: Bearer %s\r\n", tok)) {
                    oom = true;
                }
            }
        } else if (common->user) {
            if (!has_header(common->header, common->header_count, "Authorization")) {
                char *b64 = nurl_utils_base64_encode((const unsigned char *)common->user, strlen(common->user));
                if (b64) {
                    if (!append_hdr_str(&extra_hdr, &extra_hdr_len, &extra_hdr_cap, "Authorization: Basic %s\r\n", b64)) {
                        oom = true;
                    }
                    free(b64);
                } else {
                    oom = true;
                }
            }
        }
    }

    if (oom) {
        fprintf(stderr, "nurl: (1) Out of memory preparing headers.\n");
        free(body);
        free(extra_hdr);
        return NURL_ERR_GENERIC;
    }

    char *scheme = NULL;
    char *host = NULL;
    char *path = NULL;
    int port = 0;

    if (nurl_utils_parse_url(url, &scheme, &host, &port, &path) != 0) {
        fprintf(stderr, "nurl: (4) Malformed URL: %s\n", url);
        free(body);
        free(extra_hdr);
        return NURL_ERR_INVALID_URL;
    }

    int sock_fd = nurl_net_connect_proxy(host, port, common->proxy, common->proxy_user, common->no_proxy);
    if (sock_fd < 0) {
        fprintf(stderr, "nurl: (2) Could not connect to host %s:%d\n", host, port);
        free(body);
        free(extra_hdr);
        free(scheme); free(host); free(path);
        return NURL_ERR_NETWORK;
    }

    if (common->timeout > 0) {
        nurl_net_set_timeout(sock_fd, common->timeout);
    }

    nurl_tls_t *tls = nurl_tls_create(!common->no_verify, common->cacert, common->cert, common->key);
    if (!tls) {
        fprintf(stderr, "nurl: (5) Failed to initialize TLS context.\n");
        nurl_net_close(sock_fd);
        free(body);
        free(extra_hdr);
        free(scheme); free(host); free(path);
        return NURL_ERR_TLS;
    }

    if (common->verbose && !common->silent) {
        fprintf(stderr, "* Connected to %s port %d\n", host, port);
        fprintf(stderr, "* TLS handshake complete\n*\n");
        fprintf(stderr, "> POST %s HTTP/1.1\n", path);
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

    if (nurl_tls_handshake(tls, sock_fd, host) != 0) {
        fprintf(stderr, "nurl: (5) TLS verification failed.\n");
        nurl_tls_free(tls);
        nurl_net_close(sock_fd);
        free(body);
        free(extra_hdr);
        free(scheme); free(host); free(path);
        return NURL_ERR_TLS;
    }

    nurl_http_response_t *res = nurl_http_request(tls, "POST", path, host, extra_hdr, body, body_len);

    free(body);
    free(extra_hdr);

    if (!res) {
        fprintf(stderr, "nurl: (2) HTTP request failed or timed out.\n");
        nurl_tls_free(tls);
        nurl_net_close(sock_fd);
        free(scheme); free(host); free(path);
        return NURL_ERR_NETWORK;
    }

    if (common->verbose && !common->silent) {
        fprintf(stderr, "< HTTP/1.1 %d %s\n", res->status_code, res->status_text);
        for (size_t i = 0; i < res->header_count; i++) {
            fprintf(stderr, "< %s\n", res->headers[i]);
        }
        fprintf(stderr, "< \n");
    }

    nurl_tls_free(tls);
    nurl_net_close(sock_fd);
    free(scheme); free(host); free(path);

    if (!common->silent) {
        if (common->include && !common->verbose) {
            printf("HTTP/1.1 %d %s\n", res->status_code, res->status_text);
            for (size_t i = 0; i < res->header_count; i++) {
                printf("%s\n", res->headers[i]);
            }
            printf("\n");
        }

        if (common->output) {
            FILE *out_f = fopen(common->output, "wb");
            if (!out_f) {
                fprintf(stderr, "nurl: (6) Could not open file for writing: %s\n", common->output);
                nurl_http_response_free(res);
                return NURL_ERR_WRITE;
            }
            if (res->body_len > 0) {
                fwrite(res->body, 1, res->body_len, out_f);
            }
            fclose(out_f);
        } else {
            if (res->body_len > 0) {
                printf("%s\n", res->body);
            }
        }
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
