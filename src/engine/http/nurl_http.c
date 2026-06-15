#include "nurl_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#ifndef NURL_VERSION
#define NURL_VERSION "0.1.3"
#endif

#include <sys/time.h>
#include <unistd.h>

static void update_progress_bar(bool show_progress, bool silent, unsigned long downloaded, unsigned long total_len, unsigned long resume_offset, struct timeval start_time, struct timeval *last_update) {
    if (!show_progress || silent) return;
    struct timeval now;
    gettimeofday(&now, NULL);
    double elapsed_sec = (now.tv_sec - start_time.tv_sec) + (now.tv_usec - start_time.tv_usec) / 1000000.0;
    double since_last_sec = (now.tv_sec - last_update->tv_sec) + (now.tv_usec - last_update->tv_usec) / 1000000.0;

    if (since_last_sec >= 0.2 || (total_len > 0 && downloaded == total_len)) {
        *last_update = now;
        double speed_mb = 0.0;
        if (elapsed_sec > 0.0) {
            speed_mb = ((double)(downloaded - resume_offset) / (1024.0 * 1024.0)) / elapsed_sec;
        }

        if (total_len > 0) {
            int percent = (int)(((double)downloaded / (double)total_len) * 100.0);
            double remaining_sec = 0.0;
            if (total_len > downloaded && speed_mb > 0.0) {
                remaining_sec = (double)(total_len - downloaded) / (speed_mb * 1024.0 * 1024.0);
            }
            fprintf(stderr, "\r  %.2f MB / %.2f MB  %d%%  %.2f MB/s  %.0fs left",
                (double)downloaded / (1024.0 * 1024.0),
                (double)total_len / (1024.0 * 1024.0),
                percent,
                speed_mb,
                remaining_sec);
        } else {
            fprintf(stderr, "\r  %.2f MB / Unknown  %.2f MB/s",
                (double)downloaded / (1024.0 * 1024.0),
                speed_mb);
        }
        fflush(stderr);
    }
}

static int tls_read_line(nurl_tls_t *tls, char *buf, size_t max_len) {
    size_t len = 0;
    while (len < max_len - 1) {
        char c;
        int n = nurl_tls_read(tls, &c, 1);
        if (n <= 0) {
            break;
        }
        buf[len++] = c;
        if (c == '\n') {
            break;
        }
    }
    buf[len] = '\0';
    return (int)len;
}

nurl_http_response_t *nurl_http_request(
    nurl_tls_t *tls,
    const char *method,
    const char *path,
    const char *hostname,
    const char *extra_headers,
    const unsigned char *body,
    size_t body_len,
    FILE *body_out,
    bool show_progress,
    bool silent,
    unsigned long resume_offset
) {
    // 1. Construct HTTP request headers
    size_t req_capacity = 4096 + body_len;
    char *req_buf = malloc(req_capacity);
    if (!req_buf) {
        return NULL;
    }

    bool has_user_agent = false;
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
    }

    int written;
    if (has_user_agent) {
        written = snprintf(req_buf, req_capacity,
            "%s %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Connection: close\r\n",
            method, path, hostname);
    } else {
        written = snprintf(req_buf, req_capacity,
            "%s %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "User-Agent: nurl/" NURL_VERSION "\r\n"
            "Connection: close\r\n",
            method, path, hostname);
    }

    if (body && body_len > 0) {
        written += snprintf(req_buf + written, req_capacity - written,
            "Content-Length: %zu\r\n", body_len);
    }

    if (extra_headers) {
        written += snprintf(req_buf + written, req_capacity - written, "%s", extra_headers);
    }

    // Append final CRLF separating headers and body
    written += snprintf(req_buf + written, req_capacity - written, "\r\n");

    // Send HTTP Headers
    if (nurl_tls_write(tls, req_buf, written) <= 0) {
        free(req_buf);
        return NULL;
    }
    free(req_buf);

    // Send HTTP Body if present
    if (body && body_len > 0) {
        if (nurl_tls_write(tls, body, body_len) <= 0) {
            return NULL;
        }
    }

    // 2. Read response headers
    size_t header_capacity = 8192;
    size_t header_length = 0;
    char *headers_buf = malloc(header_capacity);
    if (!headers_buf) {
        return NULL;
    }

    bool found_boundary = false;
    while (header_length < header_capacity - 1) {
        char c;
        int n = nurl_tls_read(tls, &c, 1);
        if (n <= 0) {
            break;
        }
        headers_buf[header_length++] = c;
        headers_buf[header_length] = '\0';

        if (header_length >= 4 &&
            headers_buf[header_length - 4] == '\r' &&
            headers_buf[header_length - 3] == '\n' &&
            headers_buf[header_length - 2] == '\r' &&
            headers_buf[header_length - 1] == '\n') {
            found_boundary = true;
            break;
        }
    }

    if (!found_boundary) {
        free(headers_buf);
        return NULL;
    }

    nurl_http_response_t *res = calloc(1, sizeof(nurl_http_response_t));
    if (!res) {
        free(headers_buf);
        return NULL;
    }

    // Split headers string into lines
    headers_buf[header_length - 4] = '\0';

    char *line = strtok(headers_buf, "\r\n");
    if (!line) {
        nurl_http_response_free(res);
        free(headers_buf);
        return NULL;
    }

    // Parse status line: e.g. "HTTP/1.1 200 OK"
    char *version = strchr(line, ' ');
    if (version) {
        version++;
        res->status_code = atoi(version);
        char *status_text_start = strchr(version, ' ');
        if (status_text_start) {
            status_text_start++;
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
        free(headers_buf);
        return NULL;
    }

    // Parse all other header lines
    bool is_chunked = false;
    size_t content_len = 0;
    unsigned long total_len = 0;

    while ((line = strtok(NULL, "\r\n")) != NULL) {
        char **temp = realloc(res->headers, sizeof(char *) * (res->header_count + 1));
        if (!temp) {
            nurl_http_response_free(res);
            free(headers_buf);
            return NULL;
        }
        res->headers = temp;
        res->headers[res->header_count] = strdup(line);
        if (!res->headers[res->header_count]) {
            nurl_http_response_free(res);
            free(headers_buf);
            return NULL;
        }
        res->header_count++;

        // Detect chunked or Content-Length
        char *colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            char *key = line;
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
            *colon = ':'; // Restore
        }
    }
    free(headers_buf);

    unsigned long total_len_computed = total_len;
    bool is_resume = (res->status_code == 206 && resume_offset > 0);
    if (total_len_computed == 0) {
        total_len_computed = is_resume ? (content_len + resume_offset) : content_len;
    }

    struct timeval start_time, last_update;
    gettimeofday(&start_time, NULL);
    last_update = start_time;
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
                return NULL;
            }
        }

        while (1) {
            char line_buf[256];
            int line_len = tls_read_line(tls, line_buf, sizeof(line_buf));
            if (line_len <= 0) {
                break; // Premature EOF
            }

            unsigned long chunk_size = strtoul(line_buf, NULL, 16);
            if (chunk_size == 0) {
                // Consume trailing CRLF of the last chunk size 0
                char dummy[2];
                nurl_tls_read(tls, dummy, 2);
                break; // Finished
            }

            size_t read_chunk = 0;
            while (read_chunk < chunk_size) {
                char chunk_buf[4096];
                size_t to_read = chunk_size - read_chunk;
                if (to_read > sizeof(chunk_buf)) to_read = sizeof(chunk_buf);
                int n = nurl_tls_read(tls, chunk_buf, (int)to_read);
                if (n <= 0) {
                    if (body_buf) free(body_buf);
                    nurl_http_response_free(res);
                    return NULL;
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
                            return NULL;
                        }
                        body_buf = temp;
                    }
                    memcpy(body_buf + body_len, chunk_buf, n);
                }
                body_len += n;
                read_chunk += n;
                downloaded += n;
                update_progress_bar(show_progress, silent, downloaded, total_len_computed, resume_offset, start_time, &last_update);
            }

            // Consume trailing CRLF of this chunk
            char dummy[2];
            nurl_tls_read(tls, dummy, 2);
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
                return NULL;
            }
        }

        while (body_len < content_len) {
            char chunk_buf[4096];
            size_t to_read = content_len - body_len;
            if (to_read > sizeof(chunk_buf)) to_read = sizeof(chunk_buf);
            int n = nurl_tls_read(tls, chunk_buf, (int)to_read);
            if (n <= 0) break; // Premature EOF

            if (body_out) {
                fwrite(chunk_buf, 1, n, body_out);
            } else {
                memcpy(body_buf + body_len, chunk_buf, n);
            }
            body_len += n;
            downloaded += n;
            update_progress_bar(show_progress, silent, downloaded, total_len_computed, resume_offset, start_time, &last_update);
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
                return NULL;
            }
        }

        char chunk_buf[4096];
        int n;
        while ((n = nurl_tls_read(tls, chunk_buf, sizeof(chunk_buf))) > 0) {
            if (body_out) {
                fwrite(chunk_buf, 1, n, body_out);
            } else {
                if (body_len + n >= body_cap) {
                    body_cap = (body_cap + n) * 2;
                    unsigned char *temp = realloc(body_buf, body_cap);
                    if (!temp) {
                        free(body_buf);
                        nurl_http_response_free(res);
                        return NULL;
                    }
                    body_buf = temp;
                }
                memcpy(body_buf + body_len, chunk_buf, n);
            }
            body_len += n;
            downloaded += n;
            update_progress_bar(show_progress, silent, downloaded, total_len_computed, resume_offset, start_time, &last_update);
        }

        if (!body_out && body_buf) {
            body_buf[body_len] = '\0';
            res->body = body_buf;
            res->body_len = body_len;
        }
    }

    if (show_progress && !silent) {
        fprintf(stderr, "\n");
    }

    return res;
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
