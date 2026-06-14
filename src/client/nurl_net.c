#include "nurl_net.h"
#include "nurl_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <ctype.h>

int nurl_net_connect(const char *hostname, int port) {
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    struct addrinfo hints;
    struct addrinfo *result, *rp;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* TCP socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;           /* Any protocol */

    int s = getaddrinfo(hostname, port_str, &hints, &result);
    if (s != 0) {
        return -1;
    }

    int socket_fd = -1;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (socket_fd == -1) {
            continue;
        }

        if (connect(socket_fd, rp->ai_addr, rp->ai_addrlen) != -1) {
            break; /* Connected successfully */
        }

        close(socket_fd);
        socket_fd = -1;
    }

    freeaddrinfo(result);
    return socket_fd;
}

void nurl_net_close(int socket_fd) {
    if (socket_fd >= 0) {
        close(socket_fd);
    }
}

int nurl_net_set_timeout(int socket_fd, unsigned long seconds) {
    if (socket_fd < 0 || seconds == 0) {
        return 0;
    }
    struct timeval tv;
    tv.tv_sec = (time_t)seconds;
    tv.tv_usec = 0;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        return -1;
    }
    if (setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        return -1;
    }
    return 0;
}

int nurl_net_connect_proxy(
    const char *host, int port,
    const char *proxy, const char *proxy_user, const char *no_proxy
) {
    bool use_proxy = false;
    if (proxy && strlen(proxy) > 0) {
        use_proxy = true;
        if (no_proxy && strlen(no_proxy) > 0) {
            char *no_proxy_copy = strdup(no_proxy);
            if (no_proxy_copy) {
                char *tok = strtok(no_proxy_copy, ",");
                while (tok) {
                    char *trimmed = tok;
                    while (*trimmed && isspace((unsigned char)*trimmed)) {
                        trimmed++;
                    }
                    // Trim trailing spaces
                    size_t len = strlen(trimmed);
                    while (len > 0 && isspace((unsigned char)trimmed[len - 1])) {
                        len--;
                    }
                    if (len > 0) {
                        char *item = strndup(trimmed, len);
                        if (item) {
                            size_t hlen = strlen(host);
                            size_t ilen = strlen(item);
                            if (strcasecmp(host, item) == 0) {
                                use_proxy = false;
                                free(item);
                                break;
                            }
                            if (item[0] == '.' && hlen > ilen && strcasecmp(host + hlen - ilen, item) == 0) {
                                use_proxy = false;
                                free(item);
                                break;
                            }
                            free(item);
                        }
                    }
                    tok = strtok(NULL, ",");
                }
                free(no_proxy_copy);
            }
        }
    }

    if (!use_proxy) {
        return nurl_net_connect(host, port);
    }

    // Parse proxy string
    char *proxy_host = NULL;
    int proxy_port = 8080;

    // Use nurl_utils_parse_url for proxy parsing
    char *proxy_scheme = NULL;
    char *proxy_path = NULL;
    if (nurl_utils_parse_url(proxy, &proxy_scheme, &proxy_host, &proxy_port, &proxy_path) != 0) {
        // Fallback: raw host:port
        char *colon = strchr(proxy, ':');
        if (colon) {
            proxy_host = strndup(proxy, colon - proxy);
            proxy_port = atoi(colon + 1);
        } else {
            proxy_host = strdup(proxy);
            proxy_port = 8080;
        }
    }
    free(proxy_scheme);
    free(proxy_path);

    if (!proxy_host) {
        return -1;
    }

    int socket_fd = nurl_net_connect(proxy_host, proxy_port);
    free(proxy_host);

    if (socket_fd < 0) {
        return -1;
    }

    // Set a temporary timeout for proxy handshake
    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    // Send CONNECT request
    char connect_req[2048];
    int req_len = 0;
    if (proxy_user && strlen(proxy_user) > 0) {
        char *auth_b64 = nurl_utils_base64_encode((const unsigned char *)proxy_user, strlen(proxy_user));
        if (auth_b64) {
            req_len = snprintf(connect_req, sizeof(connect_req),
                "CONNECT %s:%d HTTP/1.1\r\n"
                "Host: %s:%d\r\n"
                "Proxy-Authorization: Basic %s\r\n"
                "\r\n",
                host, port, host, port, auth_b64);
            free(auth_b64);
        } else {
            req_len = snprintf(connect_req, sizeof(connect_req),
                "CONNECT %s:%d HTTP/1.1\r\n"
                "Host: %s:%d\r\n"
                "\r\n",
                host, port, host, port);
        }
    } else {
        req_len = snprintf(connect_req, sizeof(connect_req),
            "CONNECT %s:%d HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "\r\n",
            host, port, host, port);
    }

    if (write(socket_fd, connect_req, req_len) <= 0) {
        close(socket_fd);
        return -1;
    }

    // Read Response Header
    char resp_buf[1024];
    int resp_len = 0;
    while (resp_len < (int)sizeof(resp_buf) - 1) {
        int n = read(socket_fd, resp_buf + resp_len, 1);
        if (n <= 0) break;
        resp_len++;
        resp_buf[resp_len] = '\0';
        if (resp_len >= 4 && strcmp(resp_buf + resp_len - 4, "\r\n\r\n") == 0) {
            break;
        }
    }

    // Verify 2xx response
    if (strstr(resp_buf, " 200") == NULL && strstr(resp_buf, " 201") == NULL && strstr(resp_buf, " 204") == NULL) {
        close(socket_fd);
        return -1;
    }

    // Reset default socket timeouts
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    return socket_fd;
}
