#include "commands.h"
#include "nurl_utils.h"
#include "errors/nurl_diag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

int nurl_cmd_resolve(const char *url_or_host, const CommonArgs *common) {
    (void)common;
    char *scheme = NULL;
    char *host = NULL;
    char *path = NULL;
    int port = 0;

    // Strip scheme if it is a full URL
    char *target_host = NULL;
    if (strstr(url_or_host, "://")) {
        if (nurl_utils_parse_url(url_or_host, &scheme, &host, &port, &path) == 0) {
            target_host = strdup(host);
        }
    }
    if (!target_host) {
        target_host = strdup(url_or_host);
    }

    if (!target_host) {
        free(scheme); free(host); free(path);
        return 2;
    }

    // Resolve host
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int s = getaddrinfo(target_host, NULL, &hints, &result);
    if (s != 0) {
        nurl_diag_err("DNS resolution failed for '%s'.", target_host);
        nurl_diag_hint("check your internet connection or verify the hostname is correct.");
        free(target_host);
        free(scheme); free(host); free(path);
        return 2;
    }

    bool found = false;
    char ip_str[INET6_ADDRSTRLEN];
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        found = true;
        void *addr;
        const char *record_type;
        if (rp->ai_family == AF_INET) {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)rp->ai_addr;
            addr = &(ipv4->sin_addr);
            record_type = "A";
        } else {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)rp->ai_addr;
            addr = &(ipv6->sin6_addr);
            record_type = "AAAA";
        }
        inet_ntop(rp->ai_family, addr, ip_str, sizeof(ip_str));
        printf("%s\t%s\t%s\n", target_host, ip_str, record_type);
    }

    freeaddrinfo(result);
    free(target_host);
    free(scheme); free(host); free(path);

    if (!found) {
        return 2;
    }

    return 0;
}
