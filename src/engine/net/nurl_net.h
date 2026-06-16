#ifndef NURL_NET_H
#define NURL_NET_H

#include "errors/nurl_error.h"

/**
 * Resolves the given hostname and connects to it over TCP on the specified port.
 * Returns the socket file descriptor on success, or -1 on error.
 */
int nurl_net_connect(const char *hostname, int port);

/**
 * Advanced connect with detailed error reporting.
 */
int nurl_net_connect_ex(const char *hostname, int port, nurl_err_t *out_err);

/**
 * Connects to a host, optionally routing through an HTTP proxy.
 * If proxying HTTPS, performs a CONNECT request to tunnel the traffic.
 */
int nurl_net_connect_proxy(const char *host, int port, const char *proxy, const char *proxy_user, const char *no_proxy);

/**
 * Advanced proxy connect with detailed error reporting.
 */
int nurl_net_connect_proxy_ex(const char *host, int port, const char *proxy, const char *proxy_user, const char *no_proxy, nurl_err_t *out_err);


/**
 * Sets send and receive timeouts on the socket descriptor.
 * Returns 0 on success, or -1 on failure.
 */
int nurl_net_set_timeout(int socket_fd, unsigned long seconds);

/**
 * Closes the given socket file descriptor.
 */
void nurl_net_close(int socket_fd);

/**
 * Initializes platform socket subsystems (e.g. Winsock WSAStartup).
 * Returns 0 on success, or non-zero on failure.
 */
int nurl_net_init(void);

/**
 * Cleans up platform socket subsystems (e.g. Winsock WSACleanup).
 */
void nurl_net_cleanup(void);

#endif /* NURL_NET_H */
