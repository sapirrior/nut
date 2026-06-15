#ifndef NURL_ASYNC_H
#define NURL_ASYNC_H

#include <stdbool.h>
#include <stdint.h>

typedef struct NurlEventLoop NurlEventLoop;

typedef void (*NurlEventCallback)(NurlEventLoop *loop, int fd, uint32_t events, void *ctx);

NurlEventLoop *nurl_event_loop_create(void);
void           nurl_event_loop_destroy(NurlEventLoop *loop);

int            nurl_event_loop_add(NurlEventLoop *loop, int fd, uint32_t events, NurlEventCallback cb, void *ctx);
int            nurl_event_loop_mod(NurlEventLoop *loop, int fd, uint32_t events, NurlEventCallback cb, void *ctx);
int            nurl_event_loop_del(NurlEventLoop *loop, int fd);

/* Runs the event loop. Returns 0 when all descriptors are processed or stopped. */
int            nurl_event_loop_run(NurlEventLoop *loop, int timeout_ms);

#endif /* NURL_ASYNC_H */
