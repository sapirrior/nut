#ifndef NURL_POOL_H
#define NURL_POOL_H

#include "engine/request.h"
#include "nurl_tls.h"
#include <time.h>
#include <stdbool.h>

#define NURL_POOL_MAX 8  /* max cached connections */

typedef struct {
    char         host[256];
    int          port;
    int          sock_fd;
    nurl_tls_t  *tls;
    bool         in_use;
    time_t       last_used;   /* for idle eviction */
} NurlPoolEntry;

typedef struct NurlConnPool {
    NurlPoolEntry entries[NURL_POOL_MAX];
} NurlConnPool;

NurlConnPool  *nurl_pool_create(void);
void           nurl_pool_destroy(NurlConnPool *pool);

/* Acquire a cached connection or open a new one.
 * Returns 0 on success; *sock_fd and *tls are populated. */
int            nurl_pool_acquire(NurlConnPool *pool,
                   const char *host, int port,
                   const NurlRequest *req,
                   int *sock_fd, nurl_tls_t **tls);

/* Return a connection to the pool after a successful keep-alive request. */
void           nurl_pool_release(NurlConnPool *pool,
                   const char *host, int port,
                   int sock_fd, nurl_tls_t *tls);

/* Permanently close and evict a connection (on error or Connection: close). */
void           nurl_pool_evict(NurlConnPool *pool, int sock_fd);

#endif /* NURL_POOL_H */
