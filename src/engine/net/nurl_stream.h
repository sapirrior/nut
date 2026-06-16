#ifndef NURL_STREAM_H
#define NURL_STREAM_H

#include <stddef.h>
#include <stdbool.h>
#include "nurl_tls.h"

#define NURL_STREAM_BUFFER_SIZE 8192

typedef struct {
    unsigned char data[NURL_STREAM_BUFFER_SIZE];
    size_t        pos;
    size_t        len;
} NurlStreamBuffer;

typedef struct {
    int              fd;
    nurl_tls_t      *tls;
    NurlStreamBuffer read_buf;
} NurlStream;

/**
 * Creates a new stream wrapping a socket and optionally a TLS context.
 * The stream does NOT take ownership of the fd or tls (it won't close them on free).
 */
NurlStream *nurl_stream_new(int fd, nurl_tls_t *tls);

/**
 * Frees the stream structure.
 */
void        nurl_stream_free(NurlStream *s);

/**
 * Reads up to 'len' bytes into 'buf'. Returns bytes read, or <= 0 on error/EOF.
 * Uses the internal buffer to minimize syscalls/SSL_read calls.
 */
int         nurl_stream_read(NurlStream *s, void *buf, size_t len);

/**
 * Reads a single line (up to \n) into 'buf' of max size 'max_len'.
 * The resulting string is null-terminated and includes the newline if space permits.
 * Returns bytes read (including \n), or <= 0 on error/EOF.
 */
int         nurl_stream_read_line(NurlStream *s, char *buf, size_t max_len);

/**
 * Reads exactly 'len' bytes into 'buf'.
 * Returns 'len' on success, or <= 0 on error/EOF.
 */
int         nurl_stream_read_exact(NurlStream *s, void *buf, size_t len);

/**
 * Writes 'len' bytes from 'buf' to the stream.
 * Returns bytes written, or <= 0 on error.
 */
int         nurl_stream_write(NurlStream *s, const void *buf, size_t len);

/**
 * Checks if the stream has any buffered data remaining.
 */
bool        nurl_stream_has_buffered(const NurlStream *s);

#endif /* NURL_STREAM_H */
