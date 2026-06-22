#ifndef SONET_POOL_H
#define SONET_POOL_H

#include "engine/sonet_engine_request.h"
#include "sonet_stream.h"
#include <time.h>
#include <stdbool.h>

#define SONET_POOL_MAX 8  /* max cached connections */

typedef struct {
    char         host[256];
    int          port;
    bool         is_tls;
    SonetStream  *stream;
    bool         in_use;
    time_t       last_used;   /* for idle eviction */
} SonetPoolEntry;

typedef struct SonetConnPool {
    SonetPoolEntry entries[SONET_POOL_MAX];
} SonetConnPool;

SonetConnPool  *sonet_pool_create(void);
void           sonet_pool_destroy(SonetConnPool *pool);

/* Acquire a cached connection or open a new one.
 * Returns SONET_OK on success; *stream is populated. */
sonet_err_t     sonet_pool_acquire(SonetConnPool *pool,
                   const char *host, int port, bool is_tls,
                   const SonetRequest *req,
                   SonetStream **stream);

/* Return a connection to the pool after a successful keep-alive request. */
void           sonet_pool_release(SonetConnPool *pool,
                   const char *host, int port,
                   SonetStream *stream);

/* Permanently close and evict a connection (on error or Connection: close). */
void           sonet_pool_evict(SonetConnPool *pool, SonetStream *stream);

#endif /* SONET_POOL_H */
