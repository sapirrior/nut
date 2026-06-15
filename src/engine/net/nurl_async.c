#include "nurl_async.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <stdio.h>

#define MAX_EVENTS 64

typedef struct {
    int fd;
    NurlEventCallback cb;
    void *ctx;
} NurlEventWatcher;

struct NurlEventLoop {
    int epoll_fd;
    NurlEventWatcher watchers[1024]; // Simple array lookup by fd for callbacks
    int watcher_count;
};

NurlEventLoop *nurl_event_loop_create(void) {
    NurlEventLoop *loop = calloc(1, sizeof(NurlEventLoop));
    if (!loop) return NULL;

    loop->epoll_fd = epoll_create1(0);
    if (loop->epoll_fd < 0) {
        free(loop);
        return NULL;
    }

    for (int i = 0; i < 1024; i++) {
        loop->watchers[i].fd = -1;
    }
    return loop;
}

void nurl_event_loop_destroy(NurlEventLoop *loop) {
    if (!loop) return;
    if (loop->epoll_fd >= 0) {
        close(loop->epoll_fd);
    }
    free(loop);
}

int nurl_event_loop_add(NurlEventLoop *loop, int fd, uint32_t events, NurlEventCallback cb, void *ctx) {
    if (!loop || fd < 0 || fd >= 1024) return -1;

    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        return -1;
    }

    loop->watchers[fd].fd = fd;
    loop->watchers[fd].cb = cb;
    loop->watchers[fd].ctx = ctx;
    loop->watcher_count++;
    return 0;
}

int nurl_event_loop_mod(NurlEventLoop *loop, int fd, uint32_t events, NurlEventCallback cb, void *ctx) {
    if (!loop || fd < 0 || fd >= 1024) return -1;

    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0) {
        return -1;
    }

    loop->watchers[fd].fd = fd;
    loop->watchers[fd].cb = cb;
    loop->watchers[fd].ctx = ctx;
    return 0;
}

int nurl_event_loop_del(NurlEventLoop *loop, int fd) {
    if (!loop || fd < 0 || fd >= 1024) return -1;

    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0) {
        // Ignored if fd was already closed/removed
    }

    if (loop->watchers[fd].fd >= 0) {
        loop->watchers[fd].fd = -1;
        loop->watchers[fd].cb = NULL;
        loop->watchers[fd].ctx = NULL;
        loop->watcher_count--;
    }
    return 0;
}

int nurl_event_loop_run(NurlEventLoop *loop, int timeout_ms) {
    if (!loop) return -1;
    if (loop->watcher_count == 0) return 0;

    struct epoll_event events[MAX_EVENTS];
    int nfds = epoll_wait(loop->epoll_fd, events, MAX_EVENTS, timeout_ms);
    if (nfds < 0) {
        return -1;
    }

    for (int i = 0; i < nfds; i++) {
        int fd = events[i].data.fd;
        uint32_t ev = events[i].events;

        if (fd >= 0 && fd < 1024 && loop->watchers[fd].fd >= 0) {
            NurlEventCallback cb = loop->watchers[fd].cb;
            void *ctx = loop->watchers[fd].ctx;
            if (cb) {
                cb(loop, fd, ev, ctx);
            }
        }
    }
    return loop->watcher_count;
}
