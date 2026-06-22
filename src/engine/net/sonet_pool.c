#include "sonet_pool.h"
#include "sonet_net.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

SonetConnPool *sonet_pool_create(void) {
    SonetConnPool *pool = calloc(1, sizeof(SonetConnPool));
    if (!pool) return NULL;
    return pool;
}

void sonet_pool_destroy(SonetConnPool *pool) {
    if (!pool) return;
    for (int i = 0; i < SONET_POOL_MAX; i++) {
        if (pool->entries[i].stream) {
            SonetStream *s = pool->entries[i].stream;
            if (s->tls) sonet_tls_free(s->tls);
            sonet_net_close(s->fd);
            sonet_stream_free(s);
        }
    }
    free(pool);
}

sonet_err_t sonet_pool_acquire(SonetConnPool *pool, const char *host, int port, bool is_tls, const SonetRequest *req, SonetStream **stream) {
    if (!pool) return SONET_ERR_GENERIC;
    time_t now = time(NULL);

    // 1. Scan for existing warm connection
    for (int i = 0; i < SONET_POOL_MAX; i++) {
        SonetPoolEntry *e = &pool->entries[i];
        if (e->stream && !e->in_use && e->port == port && e->is_tls == is_tls && strcmp(e->host, host) == 0) {
            // Idle eviction check (e.g. 60 seconds)
            if (now - e->last_used > 60) {
                if (req->verbose && !req->silent) {
                    fprintf(stderr, "* Connection pool: evicting idle connection to %s:%d (idle %lds)\n", e->host, e->port, (long)(now - e->last_used));
                }
                if (e->stream->tls) sonet_tls_free(e->stream->tls);
                sonet_net_close(e->stream->fd);
                sonet_stream_free(e->stream);
                e->stream = NULL;
            } else if (!sonet_net_is_alive(e->stream->fd)) {
                if (req->verbose && !req->silent) {
                    fprintf(stderr, "* Connection pool: pooled connection to %s:%d died, evicting\n", e->host, e->port);
                }
                if (e->stream->tls) sonet_tls_free(e->stream->tls);
                sonet_net_close(e->stream->fd);
                sonet_stream_free(e->stream);
                e->stream = NULL;
            } else {
                e->in_use = true;
                *stream = e->stream;
                if (req->verbose && !req->silent) {
                    fprintf(stderr, "* Connection pool: reusing warm connection to %s:%d\n", host, port);
                }
                return SONET_OK;
            }
        }
    }

    // 2. Find empty slot or evict oldest unused
    int slot = -1;
    time_t oldest_time = now;
    int oldest_slot = -1;

    for (int i = 0; i < SONET_POOL_MAX; i++) {
        SonetPoolEntry *e = &pool->entries[i];
        if (!e->stream) {
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
            SonetPoolEntry *e = &pool->entries[oldest_slot];
            if (req->verbose && !req->silent) {
                fprintf(stderr, "* Connection pool: pool full, evicting oldest connection to %s:%d\n", e->host, e->port);
            }
            if (e->stream->tls) sonet_tls_free(e->stream->tls);
            sonet_net_close(e->stream->fd);
            sonet_stream_free(e->stream);
            e->stream = NULL;
            slot = oldest_slot;
        } else {
            // All slots in use
            if (req->verbose && !req->silent) {
                fprintf(stderr, "* Connection pool: all connections in use, creating bypass connection\n");
            }
        }
    }

    // 3. Connect & handshake
    sonet_err_t conn_err = SONET_OK;
    int fd = sonet_net_connect_proxy_ex(host, port, req->proxy, req->proxy_user, req->no_proxy, req->connect_to, req->connect_timeout_sec, &conn_err);
    if (fd < 0) {
        return conn_err;
    }
    if (req->read_timeout_sec > 0) {
        sonet_net_set_timeout(fd, req->read_timeout_sec);
    }

    sonet_tls_t *t = NULL;
    if (is_tls) {
        t = sonet_tls_create(req->tls_verify, req->cacert, req->cert, req->key, req->tls_version == 12, req->tls_version == 13);
        if (!t) {
            sonet_net_close(fd);
            return SONET_ERR_TLS;
        }

        if (sonet_tls_handshake(t, fd, host) != 0) {
            sonet_tls_free(t);
            sonet_net_close(fd);
            return SONET_ERR_TLS_HANDSHAKE;
        }
    }

    SonetStream *s = sonet_stream_new(fd, t);
    if (!s) {
        if (t) sonet_tls_free(t);
        sonet_net_close(fd);
        return SONET_ERR_OOM;
    }

    *stream = s;

    // Save to slot if available
    if (slot >= 0) {
        SonetPoolEntry *e = &pool->entries[slot];
        strncpy(e->host, host, sizeof(e->host) - 1);
        e->host[sizeof(e->host) - 1] = '\0';
        e->port = port;
        e->is_tls = is_tls;
        e->stream = s;
        e->in_use = true;
        e->last_used = now;
    }

    return SONET_OK;
}

void sonet_pool_release(SonetConnPool *pool, const char *host, int port, SonetStream *stream) {
    if (!pool) return;
    (void)host;
    (void)port;
    for (int i = 0; i < SONET_POOL_MAX; i++) {
        SonetPoolEntry *e = &pool->entries[i];
        if (e->stream == stream) {
            e->in_use = false;
            e->last_used = time(NULL);
            return;
        }
    }
    // If connection was bypass, close it
    if (stream->tls) sonet_tls_free(stream->tls);
    sonet_net_close(stream->fd);
    sonet_stream_free(stream);
}

void sonet_pool_evict(SonetConnPool *pool, SonetStream *stream) {
    if (!pool || !stream) return;
    for (int i = 0; i < SONET_POOL_MAX; i++) {
        SonetPoolEntry *e = &pool->entries[i];
        if (e->stream == stream) {
            if (e->stream->tls) sonet_tls_free(e->stream->tls);
            sonet_net_close(e->stream->fd);
            sonet_stream_free(e->stream);
            e->stream = NULL;
            e->in_use = false;
            return;
        }
    }
    // Bypass connection not in pool — close it directly
    if (stream->tls) sonet_tls_free(stream->tls);
    sonet_net_close(stream->fd);
    sonet_stream_free(stream);
}
