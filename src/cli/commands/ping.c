#include "commands.h"
#include "nurl_net.h"
#include "nurl_tls.h"
#include "nurl_utils.h"
#include "nurl_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

static unsigned long get_elapsed_ms(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
}

static int ping_once(const char *method, const char *path, const char *host, int port, const CommonArgs *common, unsigned long *duration_ms, int *status_out, char **status_text_out) {
    int sock_fd = nurl_net_connect_proxy(host, port, common->proxy, common->proxy_user, common->no_proxy);
    if (sock_fd < 0) {
        return -1;
    }

    if (common->timeout > 0) {
        nurl_net_set_timeout(sock_fd, common->timeout);
    }

    nurl_tls_t *tls = nurl_tls_create(!common->no_verify, common->cacert, common->cert, common->key, common->tls12, common->tls13);
    if (!tls) {
        nurl_net_close(sock_fd);
        return -1;
    }

    struct timeval start, end;
    gettimeofday(&start, NULL);

    if (nurl_tls_handshake(tls, sock_fd, host) != 0) {
        nurl_tls_free(tls);
        nurl_net_close(sock_fd);
        return -1;
    }

    nurl_http_response_t *res = nurl_http_request(tls, method, path, host, NULL, NULL, 0, NULL, false, true, 0);
    gettimeofday(&end, NULL);

    nurl_tls_free(tls);
    nurl_net_close(sock_fd);

    if (!res) {
        return -1;
    }

    *duration_ms = get_elapsed_ms(start, end);
    *status_out = res->status_code;
    *status_text_out = strdup(res->status_text);

    nurl_http_response_free(res);
    return 0;
}

int nurl_cmd_ping(const char *url, const CommonArgs *common) {
    char *scheme = NULL;
    char *host = NULL;
    char *path = NULL;
    int port = 0;

    if (nurl_utils_parse_url(url, &scheme, &host, &port, &path) != 0) {
        fprintf(stderr, "nurl: (4) Malformed URL: %s\n", url);
        return NURL_ERR_INVALID_URL;
    }

    unsigned int count = common->ping_count > 0 ? common->ping_count : 1;
    unsigned long interval = common->ping_interval > 0 ? common->ping_interval : 1000;

    unsigned long *latencies = malloc(sizeof(unsigned long) * count);
    if (!latencies) {
        free(scheme); free(host); free(path);
        return NURL_ERR_GENERIC;
    }
    size_t success_count = 0;

    for (unsigned int i = 0; i < count; i++) {
        if (i > 0) {
            usleep(interval * 1000);
        }

        unsigned long duration = 0;
        int status = 0;
        char *status_text = NULL;

        int ret = ping_once("HEAD", path, host, port, common, &duration, &status, &status_text);
        if (ret == 0 && status == 405) {
            free(status_text);
            status_text = NULL;
            ret = ping_once("GET", path, host, port, common, &duration, &status, &status_text);
        }

        if (ret == 0) {
            latencies[success_count++] = duration;
            if (!common->silent) {
                printf("%d  %s  %s  %lums\n", status, status_text, host, duration);
            }
            free(status_text);
        } else {
            fprintf(stderr, "nurl: (2) Ping failed for %s\n", host);
            free(scheme); free(host); free(path);
            free(latencies);
            return NURL_ERR_NETWORK;
        }
    }

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

    free(scheme); free(host); free(path);
    free(latencies);
    return 0;
}
