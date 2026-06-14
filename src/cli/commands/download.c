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
#include <sys/stat.h>
#include <sys/time.h>
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

int nurl_cmd_download(const char *url, const CommonArgs *common) {
    char *scheme = NULL;
    char *host = NULL;
    char *path = NULL;
    int port = 0;

    if (nurl_utils_parse_url(url, &scheme, &host, &port, &path) != 0) {
        fprintf(stderr, "Error: Invalid URL format: %s\n", url);
        return NURL_ERR_INVALID_URL;
    }

    // Determine output filename
    char *filename = NULL;
    if (common->output) {
        filename = strdup(common->output);
    } else {
        char *last_slash = strrchr(path, '/');
        if (last_slash && strlen(last_slash) > 1) {
            // Remove query parameters if present in filename
            char *q = strchr(last_slash + 1, '?');
            if (q) *q = '\0';
            filename = strdup(last_slash + 1);
            if (q) *q = '?'; // Restore
        } else {
            filename = strdup("download");
        }
    }

    if (!filename) {
        free(scheme); free(host); free(path);
        return NURL_ERR_GENERIC;
    }

    // Check if file already exists for resuming
    unsigned long start_pos = 0;
    bool file_exists = false;
    if (common->resume) {
        struct stat st;
        if (stat(filename, &st) == 0 && S_ISREG(st.st_mode)) {
            start_pos = (unsigned long)st.st_size;
            file_exists = true;
        }
    }

    // Connect and start TLS
    int sock_fd = nurl_net_connect(host, port);
    if (sock_fd < 0) {
        fprintf(stderr, "Error: Could not connect to host %s:%d\n", host, port);
        free(filename); free(scheme); free(host); free(path);
        return NURL_ERR_NETWORK;
    }

    if (common->timeout > 0) {
        nurl_net_set_timeout(sock_fd, common->timeout);
    }

    nurl_tls_t *tls = nurl_tls_create(!common->no_verify, common->cacert);
    if (!tls) {
        fprintf(stderr, "Error: Failed to initialize TLS context.\n");
        nurl_net_close(sock_fd);
        free(filename); free(scheme); free(host); free(path);
        return NURL_ERR_TLS;
    }

    if (nurl_tls_handshake(tls, sock_fd, host) != 0) {
        fprintf(stderr, "Error: TLS verification failed.\n");
        nurl_tls_free(tls);
        nurl_net_close(sock_fd);
        free(filename); free(scheme); free(host); free(path);
        return NURL_ERR_TLS;
    }

    // Compile custom headers
    size_t extra_hdr_capacity = 1024;
    char *extra_hdr = malloc(extra_hdr_capacity);
    if (!extra_hdr) {
        nurl_tls_free(tls);
        nurl_net_close(sock_fd);
        free(filename); free(scheme); free(host); free(path);
        return NURL_ERR_GENERIC;
    }
    extra_hdr[0] = '\0';
    size_t extra_hdr_len = 0;
    bool oom = false;

    // 1. User specified headers
    for (size_t i = 0; i < common->header_count; i++) {
        if (!append_hdr_str(&extra_hdr, &extra_hdr_len, &extra_hdr_capacity, "%s\r\n", common->header[i])) {
            oom = true; break;
        }
    }

    // 2. Resume Range Header
    if (!oom && start_pos > 0) {
        char range_val[64];
        snprintf(range_val, sizeof(range_val), "bytes=%lu-", start_pos);
        if (!append_hdr_str(&extra_hdr, &extra_hdr_len, &extra_hdr_capacity, "Range: %s\r\n", range_val)) {
            oom = true;
        }
    }

    // 3. Auth Headers
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

    if (oom) {
        fprintf(stderr, "Error: Out of memory.\n");
        free(extra_hdr);
        nurl_tls_free(tls);
        nurl_net_close(sock_fd);
        free(filename); free(scheme); free(host); free(path);
        return NURL_ERR_GENERIC;
    }

    // Construct and send Request
    size_t req_capacity = 2048 + extra_hdr_len;
    char *req_buf = malloc(req_capacity);
    if (!req_buf) {
        free(extra_hdr);
        nurl_tls_free(tls);
        nurl_net_close(sock_fd);
        free(filename); free(scheme); free(host); free(path);
        return NURL_ERR_GENERIC;
    }

    int written = snprintf(req_buf, req_capacity,
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: nurl/0.1.1\r\n"
        "Connection: close\r\n%s\r\n",
        path, host, extra_hdr);
    free(extra_hdr);

    if (nurl_tls_write(tls, req_buf, written) <= 0) {
        free(req_buf);
        nurl_tls_free(tls);
        nurl_net_close(sock_fd);
        free(filename); free(scheme); free(host); free(path);
        return NURL_ERR_NETWORK;
    }
    free(req_buf);

    // Read Response Headers
    size_t header_buf_cap = 8192;
    char *header_buf = malloc(header_buf_cap);
    if (!header_buf) {
        nurl_tls_free(tls);
        nurl_net_close(sock_fd);
        free(filename); free(scheme); free(host); free(path);
        return NURL_ERR_GENERIC;
    }
    size_t header_buf_len = 0;
    char *boundary = NULL;

    while (header_buf_len < header_buf_cap - 1) {
        int n = nurl_tls_read(tls, header_buf + header_buf_len, 1);
        if (n <= 0) {
            break;
        }
        header_buf_len++;
        header_buf[header_buf_len] = '\0';

        if (header_buf_len >= 4 && strcmp(header_buf + header_buf_len - 4, "\r\n\r\n") == 0) {
            boundary = header_buf + header_buf_len;
            break;
        }
    }

    if (!boundary) {
        fprintf(stderr, "Error: Malformed HTTP response headers.\n");
        free(header_buf);
        nurl_tls_free(tls);
        nurl_net_close(sock_fd);
        free(filename); free(scheme); free(host); free(path);
        return NURL_ERR_NETWORK;
    }

    // Parse status code
    int status_code = 0;
    char *status_line = strtok(header_buf, "\r\n");
    if (status_line) {
        char *space = strchr(status_line, ' ');
        if (space) {
            status_code = atoi(space + 1);
        }
    }

    // Look for Content-Length and Content-Range headers
    unsigned long content_len = 0;
    unsigned long total_len = 0;
    char *line;
    bool is_resume = (status_code == 206 && file_exists);

    while ((line = strtok(NULL, "\r\n")) != NULL) {
        char *colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            char *key = line;
            char *val = colon + 1;
            while (isspace((unsigned char)*val)) val++;

            if (strcasecmp(key, "Content-Length") == 0) {
                content_len = strtoul(val, NULL, 10);
            } else if (strcasecmp(key, "Content-Range") == 0) {
                // e.g. "bytes 100-200/500" -> extract 500
                char *slash = strchr(val, '/');
                if (slash) {
                    total_len = strtoul(slash + 1, NULL, 10);
                }
            }
        }
    }
    free(header_buf);

    if (total_len == 0) {
        total_len = is_resume ? (content_len + start_pos) : content_len;
    }

    // Open output file
    FILE *out = fopen(filename, is_resume ? "ab" : "wb");
    if (!out) {
        fprintf(stderr, "Error: Could not open file for writing: %s\n", filename);
        nurl_tls_free(tls);
        nurl_net_close(sock_fd);
        free(filename); free(scheme); free(host); free(path);
        return NURL_ERR_WRITE;
    }

    if (!common->silent) {
        fprintf(stderr, "* Downloading %s\n", filename);
        if (total_len > 0) {
            fprintf(stderr, "* Size: %.2f MB\n", (double)total_len / (1024.0 * 1024.0));
        } else {
            fprintf(stderr, "* Size: Unknown\n");
        }
    }

    // Stream download chunks
    char chunk[4096];
    unsigned long downloaded = is_resume ? start_pos : 0;
    struct timeval start_time, last_update;
    gettimeofday(&start_time, NULL);
    last_update = start_time;

    int n;
    while ((n = nurl_tls_read(tls, chunk, sizeof(chunk))) > 0) {
        if (fwrite(chunk, 1, n, out) != (size_t)n) {
            fprintf(stderr, "\nError: Disk write failed.\n");
            fclose(out);
            nurl_tls_free(tls);
            nurl_net_close(sock_fd);
            free(filename); free(scheme); free(host); free(path);
            return NURL_ERR_WRITE;
        }
        downloaded += n;

        // Print progress bar if not silent
        struct timeval now;
        gettimeofday(&now, NULL);
        double elapsed_sec = (now.tv_sec - start_time.tv_sec) + (now.tv_usec - start_time.tv_usec) / 1000000.0;
        double since_last_sec = (now.tv_sec - last_update.tv_sec) + (now.tv_usec - last_update.tv_usec) / 1000000.0;

        if (!common->silent && (since_last_sec >= 0.2 || downloaded == total_len)) {
            last_update = now;
            double speed_mb = 0.0;
            if (elapsed_sec > 0.0) {
                speed_mb = ((double)(downloaded - (is_resume ? start_pos : 0)) / (1024.0 * 1024.0)) / elapsed_sec;
            }

            int percent = 0;
            if (total_len > 0) {
                percent = (int)(((double)downloaded / (double)total_len) * 100.0);
            }

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
            fflush(stderr);
        }
    }

    fclose(out);
    nurl_tls_free(tls);
    nurl_net_close(sock_fd);

    if (!common->silent) {
        fprintf(stderr, "\n+ Download complete: %s\n", filename);
    }

    free(filename);
    free(scheme); free(host); free(path);
    return 0;
}
