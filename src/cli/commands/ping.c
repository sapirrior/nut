#include "commands.h"
#include "nurl_net.h"
#include "nurl_tls.h"
#include "nurl_utils.h"
#include "nurl_http.h"
#include "errors/nurl_diag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

static unsigned long get_elapsed_ms(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
}

static NurlStream *connect_and_handshake(const char *host, int port, const CommonArgs *common) {
    int fd = nurl_net_connect_proxy(host, port, common->proxy, common->proxy_user, common->no_proxy);
    if (fd < 0) return NULL;
    if (common->timeout > 0) {
        nurl_net_set_timeout(fd, common->timeout);
    }
    nurl_tls_t *t = nurl_tls_create(!common->no_verify, common->cacert, common->cert, common->key, common->tls12, common->tls13);
    if (!t) {
        nurl_net_close(fd);
        return NULL;
    }
    if (nurl_tls_handshake(t, fd, host) != 0) {
        nurl_tls_free(t);
        nurl_net_close(fd);
        return NULL;
    }
    NurlStream *s = nurl_stream_new(fd, t);
    if (!s) {
        nurl_tls_free(t);
        nurl_net_close(fd);
        return NULL;
    }
    return s;
}

int nurl_cmd_ping(const char *url, const CommonArgs *common) {
    char *scheme = NULL;
    char *host = NULL;
    char *path = NULL;
    int port = 0;

    if (nurl_utils_parse_url(url, &scheme, &host, &port, &path) != 0) {
        nurl_diag_err("malformed URL '%s' provided for ping.", url);
        nurl_diag_hint("ensure the URL uses a supported scheme like 'https://' and has a valid hostname.");
        return NURL_ERR_INVALID_URL;
    }

    unsigned int count = common->ping_count > 0 ? common->ping_count : 1;
    unsigned long interval = common->ping_interval > 0 ? common->ping_interval : 1000;

    NurlStream *stream = connect_and_handshake(host, port, common);
    if (!stream) {
        nurl_diag_err("initial TLS connection failed for '%s'.", host);
        nurl_diag_hint("verify the host is reachable and the certificate is valid, or use --no-verify.");
        free(scheme); free(host); free(path);
        return NURL_ERR_TLS;
    }

    if (common->verbose && !common->silent) {
        fprintf(stderr, "* Connected to %s:%d (TLS warm)\n", host, port);
    }

    unsigned long *latencies = malloc(sizeof(unsigned long) * count);
    if (!latencies) {
        nurl_tls_free(stream->tls);
        nurl_net_close(stream->fd);
        nurl_stream_free(stream);
        free(scheme); free(host); free(path);
        return NURL_ERR_GENERIC;
    }
    size_t success_count = 0;

    for (unsigned int i = 0; i < count; i++) {
        if (i > 0) {
            usleep(interval * 1000);
        }

        struct timeval start, end;
        nurl_http_response_t *res = NULL;
        unsigned long duration = 0;
        bool reconnected = false;

        while (1) {
            gettimeofday(&start, NULL);
            nurl_err_t err = nurl_http_request(stream, "HEAD", path, host, "Connection: keep-alive\r\n", NULL, 0, NULL, 0, NULL, false, true, 0, NULL, NULL, &res);
            gettimeofday(&end, NULL);

            if (err == NURL_OK && res && res->status_code == 405) {
                // Method not allowed, retry with GET
                nurl_http_response_free(res);
                res = NULL;
                gettimeofday(&start, NULL);
                err = nurl_http_request(stream, "GET", path, host, "Connection: keep-alive\r\n", NULL, 0, NULL, 0, NULL, false, true, 0, NULL, NULL, &res);
                gettimeofday(&end, NULL);
            }

            if (err != NURL_OK) {
                // Connection might have been closed by server (keep-alive timeout). Try to reconnect once.
                if (!reconnected) {
                    nurl_tls_free(stream->tls);
                    nurl_net_close(stream->fd);
                    nurl_stream_free(stream);
                    stream = connect_and_handshake(host, port, common);
                    if (stream) {
                        reconnected = true;
                        continue;
                    }
                }
                break;
            } else {
                break;
            }
        }

        if (res) {
            duration = get_elapsed_ms(start, end);
            latencies[success_count++] = duration;
            if (!common->silent) {
                printf("%d  %s  %s  %lums\n", res->status_code, res->status_text, host, duration);
            }
            nurl_http_response_free(res);
        } else {
            fprintf(stderr, "nurl: (2) Ping failed for %s on iteration %u\n", host, i + 1);
            break;
        }
    }

    if (stream) {
        nurl_tls_free(stream->tls);
        nurl_net_close(stream->fd);
        nurl_stream_free(stream);
    }
    free(scheme); free(host); free(path);

    if (count > 1 && success_count > 0 && !common->silent) {
        unsigned long min = latencies[0];
        unsigned long max = latencies[0];
        unsigned long sum = 0;
        for (size_t i = 0; i < success_count; i++) {
            if (latencies[i] < min) min = latencies[i];
            if (latencies[i] > max) max = latencies[i];
            sum += latencies[i];
        }
        unsigned long avg = sum / success_count;
        printf("\nmin %lums  avg %lums  max %lums\n", min, avg, max);
    }

    free(latencies);
    return (success_count > 0) ? 0 : NURL_ERR_NETWORK;
}
