#include "nurl_stream.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef _WIN32
#include <winsock2.h>
#define socket_read(fd, buf, len) recv(fd, (char *)(buf), (int)(len), 0)
#define socket_write(fd, buf, len) send(fd, (const char *)(buf), (int)(len), 0)
#else
#define socket_read(fd, buf, len) read(fd, buf, len)
#define socket_write(fd, buf, len) write(fd, buf, len)
#endif

NurlStream *nurl_stream_new(int fd, nurl_tls_t *tls) {
    NurlStream *s = calloc(1, sizeof(NurlStream));
    if (!s) return NULL;
    s->fd = fd;
    s->tls = tls;
    return s;
}

void nurl_stream_free(NurlStream *s) {
    free(s);
}

static int fill_buffer(NurlStream *s) {
    if (s->read_buf.pos < s->read_buf.len) {
        return (int)(s->read_buf.len - s->read_buf.pos);
    }

    int n;
    if (s->tls) {
        n = nurl_tls_read(s->tls, s->read_buf.data, NURL_STREAM_BUFFER_SIZE);
    } else {
        n = socket_read(s->fd, s->read_buf.data, NURL_STREAM_BUFFER_SIZE);
    }

    if (n > 0) {
        s->read_buf.pos = 0;
        s->read_buf.len = (size_t)n;
    } else {
        s->read_buf.pos = 0;
        s->read_buf.len = 0;
    }
    return n;
}

int nurl_stream_read(NurlStream *s, void *buf, size_t len) {
    if (len == 0) return 0;

    // If buffer is empty, try to fill it
    if (s->read_buf.pos >= s->read_buf.len) {
        int n = fill_buffer(s);
        if (n <= 0) return n;
    }

    size_t avail = s->read_buf.len - s->read_buf.pos;
    size_t to_copy = len < avail ? len : avail;
    memcpy(buf, s->read_buf.data + s->read_buf.pos, to_copy);
    s->read_buf.pos += to_copy;

    return (int)to_copy;
}

int nurl_stream_read_exact(NurlStream *s, void *buf, size_t len) {
    size_t total_read = 0;
    unsigned char *p = (unsigned char *)buf;

    while (total_read < len) {
        int n = nurl_stream_read(s, p + total_read, len - total_read);
        if (n <= 0) return n;
        total_read += n;
    }
    return (int)total_read;
}

int nurl_stream_read_line(NurlStream *s, char *buf, size_t max_len) {
    if (max_len <= 1) return 0;

    size_t total_read = 0;
    while (total_read < max_len - 1) {
        if (s->read_buf.pos >= s->read_buf.len) {
            int n = fill_buffer(s);
            if (n <= 0) {
                if (total_read > 0) break; // Return what we have
                return n;
            }
        }

        unsigned char c = s->read_buf.data[s->read_buf.pos++];
        buf[total_read++] = (char)c;
        if (c == '\n') break;
    }
    buf[total_read] = '\0';
    return (int)total_read;
}

int nurl_stream_write(NurlStream *s, const void *buf, size_t len) {
    if (s->tls) {
        return nurl_tls_write(s->tls, buf, (int)len);
    } else {
        return socket_write(s->fd, buf, len);
    }
}

bool nurl_stream_has_buffered(const NurlStream *s) {
    return s->read_buf.pos < s->read_buf.len;
}
