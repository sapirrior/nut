#include "nurl_pool.h"
#include "nurl_net.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

NurlConnPool *nurl_pool_create(void) {
    NurlConnPool *pool = calloc(1, sizeof(NurlConnPool));
    if (!pool) return NULL;
    for (int i = 0; i < NURL_POOL_MAX; i++) {
        pool->entries[i].sock_fd = -1;
    }
    return pool;
}

void nurl_pool_destroy(NurlConnPool *pool) {
    if (!pool) return;
    for (int i = 0; i < NURL_POOL_MAX; i++) {
        if (pool->entries[i].sock_fd >= 0) {
            if (pool->entries[i].tls) {
                nurl_tls_free(pool->entries[i].tls);
            }
            nurl_net_close(pool->entries[i].sock_fd);
        }
    }
    free(pool);
}

int nurl_pool_acquire(NurlConnPool *pool, const char *host, int port, const NurlRequest *req, int *sock_fd, nurl_tls_t **tls) {
    if (!pool) return -1;
    time_t now = time(NULL);

    // 1. Scan for existing warm connection
    for (int i = 0; i < NURL_POOL_MAX; i++) {
        NurlPoolEntry *e = &pool->entries[i];
        if (e->sock_fd >= 0 && !e->in_use && e->port == port && strcmp(e->host, host) == 0) {
            // Idle eviction check (e.g. 60 seconds)
            if (now - e->last_used > 60) {
                if (req->verbose && !req->silent) {
                    fprintf(stderr, "* Connection pool: evicting idle connection to %s:%d (idle %lds)\n", e->host, e->port, (long)(now - e->last_used));
                }
                if (e->tls) nurl_tls_free(e->tls);
                nurl_net_close(e->sock_fd);
                e->sock_fd = -1;
                e->tls = NULL;
            } else {
                e->in_use = true;
                *sock_fd = e->sock_fd;
                *tls = e->tls;
                if (req->verbose && !req->silent) {
                    fprintf(stderr, "* Connection pool: reusing warm connection to %s:%d\n", host, port);
                }
                return 0;
            }
        }
    }

    // 2. Find empty slot or evict oldest unused
    int slot = -1;
    time_t oldest_time = now;
    int oldest_slot = -1;

    for (int i = 0; i < NURL_POOL_MAX; i++) {
        NurlPoolEntry *e = &pool->entries[i];
        if (e->sock_fd < 0) {
            slot = i;
            break;
        }
        if (!e->in_use && e->last_used < oldest_time) {
            oldest_time = e->last_used;
            oldest_slot = i;
        }
    }

    if (slot < 0) {
        if (oldest_slot >= 0) {
            NurlPoolEntry *e = &pool->entries[oldest_slot];
            if (req->verbose && !req->silent) {
                fprintf(stderr, "* Connection pool: pool full, evicting oldest connection to %s:%d\n", e->host, e->port);
            }
            if (e->tls) nurl_tls_free(e->tls);
            nurl_net_close(e->sock_fd);
            e->sock_fd = -1;
            e->tls = NULL;
            slot = oldest_slot;
        } else {
            // All slots in use
            if (req->verbose && !req->silent) {
                fprintf(stderr, "* Connection pool: all connections in use, creating bypass connection\n");
            }
        }
    }

    // 3. Connect & handshake
    int fd = nurl_net_connect_proxy(host, port, req->proxy, req->proxy_user, req->no_proxy);
    if (fd < 0) {
        return -1;
    }
    if (req->timeout_sec > 0) {
        nurl_net_set_timeout(fd, req->timeout_sec);
    }

    nurl_tls_t *t = nurl_tls_create(req->tls_verify, req->cacert, req->cert, req->key, req->tls_version == 12, req->tls_version == 13);
    if (!t) {
        nurl_net_close(fd);
        return -1;
    }

    if (nurl_tls_handshake(t, fd, host) != 0) {
        nurl_tls_free(t);
        nurl_net_close(fd);
        return -1;
    }

    *sock_fd = fd;
    *tls = t;

    // Save to slot if available
    if (slot >= 0) {
        NurlPoolEntry *e = &pool->entries[slot];
        strncpy(e->host, host, sizeof(e->host) - 1);
        e->host[sizeof(e->host) - 1] = '\0';
        e->port = port;
        e->sock_fd = fd;
        e->tls = t;
        e->in_use = true;
        e->last_used = now;
    }

    return 0;
}

void nurl_pool_release(NurlConnPool *pool, const char *host, int port, int sock_fd, nurl_tls_t *tls) {
    if (!pool) return;
    (void)host;
    (void)port;
    for (int i = 0; i < NURL_POOL_MAX; i++) {
        NurlPoolEntry *e = &pool->entries[i];
        if (e->sock_fd == sock_fd) {
            e->in_use = false;
            e->last_used = time(NULL);
            return;
        }
    }
    // If connection was bypass, close it
    if (tls) nurl_tls_free(tls);
    nurl_net_close(sock_fd);
}

void nurl_pool_evict(NurlConnPool *pool, int sock_fd) {
    if (!pool) return;
    for (int i = 0; i < NURL_POOL_MAX; i++) {
        NurlPoolEntry *e = &pool->entries[i];
        if (e->sock_fd == sock_fd) {
            if (e->tls) nurl_tls_free(e->tls);
            nurl_net_close(e->sock_fd);
            e->sock_fd = -1;
            e->tls = NULL;
            e->in_use = false;
            return;
        }
    }
}
