#include "nurl_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#ifndef NURL_VERSION
#define NURL_VERSION "0.2.0"
#endif

#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>

nurl_err_t nurl_http_request(
    NurlStream *stream,
    const char *method,
    const char *path,
    const char *hostname,
    const char *extra_headers,
    const unsigned char *body,
    size_t body_len,
    NurlBodyPart *body_parts,
    size_t body_parts_count,
    FILE *body_out,
    bool show_progress,
    bool silent,
    unsigned long resume_offset,
    nurl_progress_cb progress_cb,
    void *progress_data,
    nurl_http_response_t **out_response
) {
    (void)show_progress;
    (void)silent;
    if (out_response) *out_response = NULL;

    size_t total_body_len = body_len;
    if (body_parts && body_parts_count > 0) {
        total_body_len = 0;
        for (size_t i = 0; i < body_parts_count; i++) {
            if (body_parts[i].type == NURL_BODY_PART_MEM) {
                total_body_len += body_parts[i].len;
            } else if (body_parts[i].type == NURL_BODY_PART_FILE) {
                struct stat st;
                if (stat(body_parts[i].filepath, &st) == 0) {
                    total_body_len += st.st_size;
                }
            }
        }
    }

    // 1. Construct HTTP request headers
    size_t req_capacity = 4096 + (extra_headers ? strlen(extra_headers) : 0);
    char *req_buf = malloc(req_capacity);
    if (!req_buf) {
        return NURL_ERR_OOM;
    }

    bool has_user_agent = false;
    bool has_connection = false;
    if (extra_headers) {
        const char *p = extra_headers;
        while ((p = strcasestr(p, "User-Agent")) != NULL) {
            if (p == extra_headers || p[-1] == '\n' || p[-1] == '\r') {
                const char *colon = strchr(p, ':');
                if (colon) {
                    has_user_agent = true;
                    break;
                }
            }
            p++;
        }
        p = extra_headers;
        while ((p = strcasestr(p, "Connection")) != NULL) {
            if (p == extra_headers || p[-1] == '\n' || p[-1] == '\r') {
                const char *colon = strchr(p, ':');
                if (colon) {
                    has_connection = true;
                    break;
                }
            }
            p++;
        }
    }

    int written;
    written = snprintf(req_buf, req_capacity, "%s %s HTTP/1.1\r\nHost: %s\r\n", method, path, hostname);
    if (!has_user_agent) {
        written += snprintf(req_buf + written, req_capacity - written, "User-Agent: nurl/" NURL_VERSION "\r\n");
    }
    if (!has_connection) {
        written += snprintf(req_buf + written, req_capacity - written, "Connection: close\r\n");
    }

    if (total_body_len > 0) {
        written += snprintf(req_buf + written, req_capacity - written,
            "Content-Length: %zu\r\n", total_body_len);
    }

    if (extra_headers) {
        written += snprintf(req_buf + written, req_capacity - written, "%s", extra_headers);
    }

    // Append final CRLF separating headers and body
    written += snprintf(req_buf + written, req_capacity - written, "\r\n");

    // Send HTTP Headers
    if (nurl_stream_write(stream, req_buf, written) <= 0) {
        free(req_buf);
        return NURL_ERR_NETWORK;
    }
    free(req_buf);

    // Send HTTP Body Parts if present
    if (body_parts && body_parts_count > 0) {
        for (size_t i = 0; i < body_parts_count; i++) {
            NurlBodyPart *part = &body_parts[i];
            if (part->type == NURL_BODY_PART_MEM) {
                if (part->data && part->len > 0) {
                    if (nurl_stream_write(stream, part->data, part->len) <= 0) {
                        return NURL_ERR_NETWORK;
                    }
                }
            } else if (part->type == NURL_BODY_PART_FILE) {
                if (part->filepath) {
                    FILE *bf = fopen(part->filepath, "rb");
                    if (!bf) {
                        return NURL_ERR_IO;
                    }
                    char send_buf[65536];
                    size_t r;
                    while ((r = fread(send_buf, 1, sizeof(send_buf), bf)) > 0) {
                        if (nurl_stream_write(stream, send_buf, r) <= 0) {
                            fclose(bf);
                            return NURL_ERR_NETWORK;
                        }
                    }
                    fclose(bf);
                }
            }
        }
    } else if (body && body_len > 0) {
        if (nurl_stream_write(stream, body, body_len) <= 0) {
            return NURL_ERR_NETWORK;
        }
    }

    // 2. Read response headers
    nurl_http_response_t *res = calloc(1, sizeof(nurl_http_response_t));
    if (!res) return NURL_ERR_OOM;

    char line_buf[8192];
    int line_len;

    // Read Status Line
    line_len = nurl_stream_read_line(stream, line_buf, sizeof(line_buf));
    if (line_len <= 0) {
        nurl_http_response_free(res);
        return NURL_ERR_NETWORK;
    }

    // Parse status line: e.g. "HTTP/1.1 200 OK"
    char *version = strchr(line_buf, ' ');
    if (version) {
        version++;
        res->status_code = atoi(version);
        char *status_text_start = strchr(version, ' ');
        if (status_text_start) {
            status_text_start++;
            // Trim \r\n
            char *p = status_text_start;
            while (*p && *p != '\r' && *p != '\n') p++;
            *p = '\0';
            res->status_text = strdup(status_text_start);
        } else {
            res->status_text = strdup("Unknown");
        }
    } else {
        res->status_code = 0;
        res->status_text = strdup("Unknown");
    }

    if (!res->status_text) {
        nurl_http_response_free(res);
        return NURL_ERR_OOM;
    }

    // Read headers until empty line
    bool is_chunked = false;
    size_t content_len = 0;
    unsigned long total_len = 0;

    while ((line_len = nurl_stream_read_line(stream, line_buf, sizeof(line_buf))) > 0) {
        if (line_len <= 2 && (line_buf[0] == '\n' || (line_buf[0] == '\r' && line_buf[1] == '\n'))) {
            break; // End of headers
        }

        // Trim trailing CRLF for strdup
        char *p = line_buf + line_len - 1;
        while (p >= line_buf && (*p == '\r' || *p == '\n')) {
            *p = '\0';
            p--;
        }

        char **temp = realloc(res->headers, sizeof(char *) * (res->header_count + 1));
        if (!temp) {
            nurl_http_response_free(res);
            return NURL_ERR_OOM;
        }
        res->headers = temp;
        res->headers[res->header_count] = strdup(line_buf);
        if (!res->headers[res->header_count]) {
            nurl_http_response_free(res);
            return NURL_ERR_OOM;
        }
        res->header_count++;

        // Detect chunked or Content-Length
        char *colon = strchr(line_buf, ':');
        if (colon) {
            *colon = '\0';
            char *key = line_buf;
            char *val = colon + 1;
            while (isspace((unsigned char)*val)) val++;
            if (strcasecmp(key, "Transfer-Encoding") == 0 && strcasecmp(val, "chunked") == 0) {
                is_chunked = true;
            } else if (strcasecmp(key, "Content-Length") == 0) {
                content_len = strtoul(val, NULL, 10);
            } else if (strcasecmp(key, "Content-Range") == 0) {
                char *slash = strchr(val, '/');
                if (slash) {
                    total_len = strtoul(slash + 1, NULL, 10);
                }
            }
        }
    }

    if (strcasecmp(method, "HEAD") == 0) {
        if (out_response) *out_response = res;
        return NURL_OK;
    }

    unsigned long total_len_computed = total_len;
    bool is_resume = (res->status_code == 206 && resume_offset > 0);
    if (total_len_computed == 0) {
        total_len_computed = is_resume ? (content_len + resume_offset) : content_len;
    }

    unsigned long downloaded = resume_offset;

    // 3. Read body from socket (either streaming to body_out or accumulating in memory)
    if (is_chunked) {
        size_t body_cap = 8192;
        size_t body_len = 0;
        unsigned char *body_buf = NULL;
        if (!body_out) {
            body_buf = malloc(body_cap);
            if (!body_buf) {
                nurl_http_response_free(res);
                return NURL_ERR_OOM;
            }
        }

        while (1) {
            char chunk_size_buf[128];
            int size_line_len = nurl_stream_read_line(stream, chunk_size_buf, sizeof(chunk_size_buf));
            if (size_line_len <= 0) {
                break; // Premature EOF
            }

            unsigned long chunk_size = strtoul(chunk_size_buf, NULL, 16);
            if (chunk_size == 0) {
                // Consume trailing CRLF of the last chunk size 0
                char dummy[2];
                nurl_stream_read_exact(stream, dummy, 2);
                break; // Finished
            }

            size_t read_chunk = 0;
            while (read_chunk < chunk_size) {
                char chunk_buf[4096];
                size_t to_read = chunk_size - read_chunk;
                if (to_read > sizeof(chunk_buf)) to_read = sizeof(chunk_buf);
                int n = nurl_stream_read(stream, chunk_buf, to_read);
                if (n <= 0) {
                    if (body_buf) free(body_buf);
                    nurl_http_response_free(res);
                    return NURL_ERR_NETWORK;
                }

                if (body_out) {
                    fwrite(chunk_buf, 1, n, body_out);
                } else {
                    if (body_len + n >= body_cap) {
                        body_cap = (body_cap + n) * 2;
                        unsigned char *temp = realloc(body_buf, body_cap);
                        if (!temp) {
                            free(body_buf);
                            nurl_http_response_free(res);
                            return NURL_ERR_OOM;
                        }
                        body_buf = temp;
                    }
                    memcpy(body_buf + body_len, chunk_buf, n);
                }
                body_len += n;
                read_chunk += n;
                downloaded += n;
                if (progress_cb) progress_cb(downloaded, total_len_computed, false, progress_data);
            }

            // Consume trailing CRLF of this chunk
            char dummy[2];
            nurl_stream_read_exact(stream, dummy, 2);
        }

        if (!body_out && body_buf) {
            body_buf[body_len] = '\0';
            res->body = body_buf;
            res->body_len = body_len;
        }
    } else if (content_len > 0) {
        size_t body_len = 0;
        unsigned char *body_buf = NULL;
        if (!body_out) {
            body_buf = malloc(content_len + 1);
            if (!body_buf) {
                nurl_http_response_free(res);
                return NURL_ERR_OOM;
            }
        }

        while (body_len < content_len) {
            char chunk_buf[4096];
            size_t to_read = content_len - body_len;
            if (to_read > sizeof(chunk_buf)) to_read = sizeof(chunk_buf);
            int n = nurl_stream_read(stream, chunk_buf, to_read);
            if (n <= 0) break; // Premature EOF

            if (body_out) {
                fwrite(chunk_buf, 1, n, body_out);
            } else {
                memcpy(body_buf + body_len, chunk_buf, n);
            }
            body_len += n;
            downloaded += n;
            if (progress_cb) progress_cb(downloaded, total_len_computed, false, progress_data);
        }

        if (!body_out && body_buf) {
            body_buf[body_len] = '\0';
            res->body = body_buf;
            res->body_len = body_len;
        }
    } else {
        // Read until EOF
        size_t body_cap = 8192;
        size_t body_len = 0;
        unsigned char *body_buf = NULL;
        if (!body_out) {
            body_buf = malloc(body_cap);
            if (!body_buf) {
                nurl_http_response_free(res);
                return NURL_ERR_OOM;
            }
        }

        char chunk_buf[4096];
        int n;
        while ((n = nurl_stream_read(stream, chunk_buf, sizeof(chunk_buf))) > 0) {
            if (body_out) {
                fwrite(chunk_buf, 1, n, body_out);
            } else {
                if (body_len + n >= body_cap) {
                    body_cap = (body_cap + n) * 2;
                    unsigned char *temp = realloc(body_buf, body_cap);
                    if (!temp) {
                        free(body_buf);
                        nurl_http_response_free(res);
                        return NURL_ERR_OOM;
                    }
                    body_buf = temp;
                }
                memcpy(body_buf + body_len, chunk_buf, n);
            }
            body_len += n;
            downloaded += n;
            if (progress_cb) progress_cb(downloaded, total_len_computed, false, progress_data);
        }

        if (!body_out && body_buf) {
            body_buf[body_len] = '\0';
            res->body = body_buf;
            res->body_len = body_len;
        }
    }

    if (progress_cb) progress_cb(downloaded, total_len_computed, true, progress_data);

    if (show_progress && !silent) {
        fprintf(stderr, "\n");
    }

    if (out_response) *out_response = res;
    return NURL_OK;
}

void nurl_http_response_free(nurl_http_response_t *res) {
    if (res) {
        if (res->status_text) {
            free(res->status_text);
        }
        if (res->headers) {
            for (size_t i = 0; i < res->header_count; i++) {
                free(res->headers[i]);
            }
            free(res->headers);
        }
        if (res->body) {
            free(res->body);
        }
        free(res);
    }
}
