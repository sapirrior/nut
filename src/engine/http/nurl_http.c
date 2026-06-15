#include "nurl_http.h"

#ifndef NURL_VERSION
#define NURL_VERSION "0.1.1"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

static unsigned char *parse_chunked_body(const unsigned char *src, size_t src_len, size_t *out_len) {
    unsigned char *dest = malloc(src_len); // Dest will always be smaller or equal to src_len
    if (!dest) {
        return NULL;
    }

    size_t dest_idx = 0;
    const unsigned char *ptr = src;
    const unsigned char *end = src + src_len;

    while (ptr < end) {
        // Find the end of the line containing the chunk size (hex)
        const unsigned char *line_end = ptr;
        while (line_end < end - 1 && !(line_end[0] == '\r' && line_end[1] == '\n')) {
            line_end++;
        }
        if (line_end >= end - 1) {
            break; // Malformed chunk header
        }

        // Convert the hex string to size
        size_t chunk_size = 0;
        const unsigned char *hex_ptr = ptr;
        while (hex_ptr < line_end) {
            char c = *hex_ptr;
            if (isxdigit((unsigned char)c)) {
                chunk_size = chunk_size * 16 + (isdigit((unsigned char)c) ? (c - '0') : (tolower((unsigned char)c) - 'a' + 10));
            } else {
                break; // Stop at extension parameters
            }
            hex_ptr++;
        }

        ptr = line_end + 2; // Advance past CRLF

        if (chunk_size == 0) {
            break; // Last chunk (size 0)
        }

        // Make sure we have enough bytes in source
        if (ptr + chunk_size > end) {
            break; // Incomplete chunk data
        }

        // Copy chunk data to dest
        memcpy(dest + dest_idx, ptr, chunk_size);
        dest_idx += chunk_size;

        ptr += chunk_size; // Advance past chunk data
        
        // Skip trailing CRLF of the chunk data
        if (ptr < end - 1 && ptr[0] == '\r' && ptr[1] == '\n') {
            ptr += 2;
        }
    }

    *out_len = dest_idx;
    return dest;
}

nurl_http_response_t *nurl_http_request(
    nurl_tls_t *tls,
    const char *method,
    const char *path,
    const char *hostname,
    const char *extra_headers,
    const unsigned char *body,
    size_t body_len
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

    // 2. Read response stream
    size_t capacity = 8192;
    size_t length = 0;
    unsigned char *raw_data = malloc(capacity);
    if (!raw_data) {
        return NULL;
    }

    char chunk[4096];
    int n;
    while ((n = nurl_tls_read(tls, chunk, sizeof(chunk))) > 0) {
        if (length + n >= capacity) {
            capacity *= 2;
            unsigned char *new_buf = realloc(raw_data, capacity);
            if (!new_buf) {
                free(raw_data);
                return NULL;
            }
            raw_data = new_buf;
        }
        memcpy(raw_data + length, chunk, n);
        length += n;
    }

    if (length == 0) {
        free(raw_data);
        return NULL;
    }
    raw_data[length] = '\0'; // Null-terminate for string safety

    // 3. Find boundary between headers and body (\r\n\r\n)
    unsigned char *boundary = NULL;
    for (size_t i = 0; i < length - 3; i++) {
        if (raw_data[i] == '\r' && raw_data[i+1] == '\n' &&
            raw_data[i+2] == '\r' && raw_data[i+3] == '\n') {
            boundary = raw_data + i;
            break;
        }
    }

    if (!boundary) {
        free(raw_data);
        return NULL;
    }

    nurl_http_response_t *res = malloc(sizeof(nurl_http_response_t));
    if (!res) {
        free(raw_data);
        return NULL;
    }
    res->headers = NULL;
    res->header_count = 0;
    res->body = NULL;
    res->body_len = 0;
    res->status_text = NULL;

    // Separate headers from body
    *boundary = '\0'; // Null-terminate headers section
    unsigned char *body_start = boundary + 4;
    size_t raw_body_len = length - (body_start - raw_data);

    // 4. Parse headers
    char *headers_str = (char *)raw_data;
    char *line = strtok(headers_str, "\r\n");
    if (!line) {
        free(res);
        free(raw_data);
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
        free(raw_data);
        return NULL;
    }

    // Parse all other header lines
    bool is_chunked = false;
    while ((line = strtok(NULL, "\r\n")) != NULL) {
        char **temp = realloc(res->headers, sizeof(char *) * (res->header_count + 1));
        if (!temp) {
            nurl_http_response_free(res);
            free(raw_data);
            return NULL;
        }
        res->headers = temp;
        res->headers[res->header_count] = strdup(line);
        if (!res->headers[res->header_count]) {
            nurl_http_response_free(res);
            free(raw_data);
            return NULL;
        }
        res->header_count++;

        // Detect if transfer-encoding is chunked
        char *colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            char *key = line;
            char *val = colon + 1;
            while (isspace((unsigned char)*val)) val++;
            if (strcasecmp(key, "Transfer-Encoding") == 0 && strcasecmp(val, "chunked") == 0) {
                is_chunked = true;
            }
            *colon = ':'; // Restore colon
        }
    }

    // 5. Parse body (accounting for chunked encoding)
    if (is_chunked) {
        res->body = parse_chunked_body(body_start, raw_body_len, &res->body_len);
        if (!res->body && raw_body_len > 0) {
            nurl_http_response_free(res);
            free(raw_data);
            return NULL;
        }
    } else {
        res->body = malloc(raw_body_len + 1);
        if (!res->body) {
            nurl_http_response_free(res);
            free(raw_data);
            return NULL;
        }
        memcpy(res->body, body_start, raw_body_len);
        res->body[raw_body_len] = '\0';
        res->body_len = raw_body_len;
    }

    free(raw_data);
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
